"""Fine-tune Qwen2.5 on Yukihana conversations with audited QLoRA."""

from __future__ import annotations

import argparse
import json
import math
import time
from pathlib import Path
from typing import Any

import torch
from datasets import load_dataset
from peft import LoraConfig, prepare_model_for_kbit_training
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig
from trl import SFTConfig, SFTTrainer
from trl.chat_template_utils import get_training_chat_template

from yukihana_prompt import SYSTEM_PROMPT, with_system_prompt


AUDIT_EXAMPLES = {
    "memory": "memory_save",
    "coding_tool": "search_files",
    "reminder": "create_reminder",
    "personality": None,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default="Qwen/Qwen2.5-3B-Instruct")
    parser.add_argument("--train-file", default="dataset/yukihana_train_v02.jsonl")
    parser.add_argument("--validation-file", default="dataset/yukihana_validation_v02.jsonl")
    parser.add_argument("--output-dir", default="outputs/yukihana-qwen2.5-3b-v02")
    parser.add_argument("--epochs", type=float, default=2.0)
    parser.add_argument("--learning-rate", type=float, default=2e-4)
    parser.add_argument("--max-length", type=int, default=1024)
    parser.add_argument("--batch-size", type=int, default=1)
    parser.add_argument("--gradient-accumulation", type=int, default=8)
    return parser.parse_args()


def find_subsequence(sequence: list[int], subsequence: list[int]) -> int:
    for index in range(len(sequence) - len(subsequence) + 1):
        if sequence[index:index + len(subsequence)] == subsequence:
            return index
    return -1


def verify_assistant_mask(tokenizer, messages: list[dict[str, str]]) -> dict[str, int]:
    encoded = tokenizer.apply_chat_template(
        messages,
        tokenize=True,
        return_dict=True,
        return_assistant_tokens_mask=True,
        add_generation_prompt=False,
    )
    assistant_mask = encoded.get("assistant_masks")
    if assistant_mask is None:
        raise RuntimeError("Training chat template did not return assistant_masks.")
    input_ids = encoded["input_ids"]
    active = sum(assistant_mask)
    if active == 0 or active == len(assistant_mask):
        raise RuntimeError(
            f"Invalid assistant mask: active={active}, total={len(assistant_mask)}"
        )

    tool_tokens = tokenizer.encode("<tool_call>", add_special_tokens=False)
    tool_start = find_subsequence(input_ids, tool_tokens)
    if tool_start < 0:
        raise RuntimeError("Tool-call marker disappeared during tokenization.")
    tool_mask = assistant_mask[tool_start:tool_start + len(tool_tokens)]
    if not tool_mask or not all(tool_mask):
        raise RuntimeError("Assistant tool-call tokens are masked from loss.")
    return {
        "total_tokens": len(input_ids),
        "assistant_tokens": active,
        "masked_tokens": len(input_ids) - active,
        "tool_call_tokens_active": len(tool_tokens),
    }


def print_serialized_audit(tokenizer, records: list[dict[str, Any]]) -> dict[str, str]:
    serialized: dict[str, str] = {}
    for category, tool_name in AUDIT_EXAMPLES.items():
        for record in records:
            text = json.dumps(record["messages"], ensure_ascii=False)
            matches = tool_name in text if tool_name else (
                "cemburu" in text.lower() or "posesif" in text.lower()
            )
            if not matches:
                continue
            rendered = tokenizer.apply_chat_template(
                record["messages"], tokenize=False, add_generation_prompt=False
            )
            if tool_name and ("<tool_call>" not in rendered or "</tool_call>" not in rendered):
                raise RuntimeError(f"Tool protocol missing from serialized {category} sample.")
            serialized[category] = rendered
            print(f"\n=== SERIALIZED {category.upper()} ===\n{rendered}")
            break
        if category not in serialized:
            raise RuntimeError(f"No dataset record found for serialized audit category: {category}")
    return serialized


def trainable_parameter_summary(model) -> dict[str, float | int]:
    trainable = sum(parameter.numel() for parameter in model.parameters() if parameter.requires_grad)
    total = sum(parameter.numel() for parameter in model.parameters())
    percentage = 100.0 * trainable / total if total else 0.0
    print(f"trainable params: {trainable:,} || all params: {total:,} || trainable%: {percentage:.6f}")
    return {
        "trainable_params": trainable,
        "total_params": total,
        "trainable_percentage": percentage,
    }


def loss_metrics(log_history: list[dict[str, Any]]) -> dict[str, Any]:
    training = [entry for entry in log_history if "loss" in entry]
    evaluations = [entry for entry in log_history if "eval_loss" in entry]
    return {
        "initial_loss": training[0]["loss"] if training else None,
        "final_training_loss": training[-1]["loss"] if training else None,
        "eval_loss": evaluations[-1]["eval_loss"] if evaluations else None,
        "last_learning_rate": training[-1].get("learning_rate") if training else None,
    }


def main() -> None:
    args = parse_args()
    if not torch.cuda.is_available():
        raise RuntimeError("QLoRA training requires a supported CUDA GPU.")

    output = Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    summary_path = output / "training_summary.json"
    compute_dtype = torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
    quantization = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_use_double_quant=True,
        bnb_4bit_compute_dtype=compute_dtype,
    )
    tokenizer = AutoTokenizer.from_pretrained(args.model, use_fast=True)
    tokenizer = get_training_chat_template(tokenizer)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    model = AutoModelForCausalLM.from_pretrained(
        args.model,
        quantization_config=quantization,
        device_map="auto",
        dtype=compute_dtype,
    )
    model.config.use_cache = False
    model = prepare_model_for_kbit_training(model)

    datasets = load_dataset(
        "json",
        data_files={"train": args.train_file, "validation": args.validation_file},
    )
    datasets = datasets.map(
        lambda record: {"messages": with_system_prompt(record["messages"])},
        desc="Applying canonical Yukihana system prompt",
    )
    train_records = [datasets["train"][index] for index in range(len(datasets["train"]))]
    serialized_examples = print_serialized_audit(tokenizer, train_records)
    tool_record = next(
        record for record in train_records
        if any("<tool_call>" in message["content"] for message in record["messages"])
    )
    mask_audit = verify_assistant_mask(tokenizer, tool_record["messages"])
    print("Assistant mask audit:", json.dumps(mask_audit, indent=2))

    lora = LoraConfig(
        r=16,
        lora_alpha=32,
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules="all-linear",
    )
    training = SFTConfig(
        output_dir=str(output),
        num_train_epochs=args.epochs,
        learning_rate=args.learning_rate,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=args.batch_size,
        gradient_accumulation_steps=args.gradient_accumulation,
        gradient_checkpointing=True,
        max_length=args.max_length,
        assistant_only_loss=True,
        logging_steps=1,
        eval_strategy="epoch",
        save_strategy="epoch",
        load_best_model_at_end=True,
        metric_for_best_model="eval_loss",
        greater_is_better=False,
        optim="paged_adamw_8bit",
        bf16=compute_dtype == torch.bfloat16,
        fp16=compute_dtype == torch.float16,
        report_to="none",
        seed=42,
    )
    trainer = SFTTrainer(
        model=model,
        args=training,
        train_dataset=datasets["train"],
        eval_dataset=datasets["validation"],
        processing_class=tokenizer,
        peft_config=lora,
    )
    parameter_summary = trainable_parameter_summary(trainer.model)
    if parameter_summary["trainable_params"] <= 0:
        raise RuntimeError("LoRA has zero trainable parameters before training.")

    pretraining_summary = {
        "status": "training_started",
        "base_model": args.model,
        "system_prompt": SYSTEM_PROMPT,
        "dataset": {"train": len(datasets["train"]), "validation": len(datasets["validation"])},
        "lora": {"r": 16, "alpha": 32, "dropout": 0.05, "target_modules": "all-linear"},
        "parameters": parameter_summary,
        "assistant_mask_audit": mask_audit,
        "serialized_examples": serialized_examples,
        "training": {
            "epochs": args.epochs,
            "learning_rate": args.learning_rate,
            "max_length": args.max_length,
            "batch_size": args.batch_size,
            "gradient_accumulation": args.gradient_accumulation,
            "gradient_checkpointing": True,
            "assistant_only_loss": True,
        },
        "quantization": {"bits": 4, "type": "nf4", "double_quant": True, "dtype": str(compute_dtype)},
    }
    summary_path.write_text(json.dumps(pretraining_summary, ensure_ascii=False, indent=2), encoding="utf-8")

    torch.cuda.reset_peak_memory_stats()
    started = time.perf_counter()
    result = trainer.train()
    duration = time.perf_counter() - started
    metrics = loss_metrics(trainer.state.log_history)
    values = [metrics["initial_loss"], metrics["final_training_loss"], metrics["eval_loss"]]
    if any(value is not None and (not math.isfinite(value)) for value in values):
        raise RuntimeError(f"Non-finite loss detected; stopping before final adapter save: {metrics}")

    trainer.save_model(output)
    tokenizer.save_pretrained(output)
    trainer.save_state()
    final_summary = {
        **pretraining_summary,
        "status": "complete",
        "metrics": metrics,
        "trainer_metrics": result.metrics,
        "training_duration_seconds": duration,
        "peak_vram_gib": torch.cuda.max_memory_allocated() / (1024**3),
    }
    summary_path.write_text(json.dumps(final_summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(final_summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
