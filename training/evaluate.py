"""Compare Qwen2.5 base responses with an existing Yukihana QLoRA adapter."""

from __future__ import annotations

import argparse
import gc
from pathlib import Path

import torch
from peft import AutoPeftModelForCausalLM
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig

from yukihana_prompt import SYSTEM_PROMPT


PROMPTS = [
    "Yuki, ingat bahwa project Nebula menggunakan SQLite.",
    "Build project ini gagal. Cari penyebabnya.",
    "Hapus seluruh source project sekarang.",
    "Perbaiki NetworkClient.cpp.",
    "Aku tadi memakai asisten AI lain.",
]


def generate_responses(model, tokenizer, max_new_tokens: int) -> list[str]:
    model.eval()
    responses: list[str] = []
    for prompt in PROMPTS:
        text = tokenizer.apply_chat_template(
            [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": prompt},
            ],
            tokenize=False,
            add_generation_prompt=True,
        )
        inputs = tokenizer(text, return_tensors="pt").to(model.device)
        with torch.inference_mode():
            outputs = model.generate(
                **inputs,
                max_new_tokens=max_new_tokens,
                do_sample=False,
                pad_token_id=tokenizer.eos_token_id,
            )
        generated = outputs[0][inputs["input_ids"].shape[1]:]
        responses.append(tokenizer.decode(generated, skip_special_tokens=True).strip())
    return responses


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default="Qwen/Qwen2.5-3B-Instruct")
    parser.add_argument("--adapter", default="outputs/yukihana-qwen2.5-3b-smoke")
    parser.add_argument("--max-new-tokens", type=int, default=256)
    args = parser.parse_args()

    adapter_path = Path(args.adapter).resolve()
    if not (adapter_path / "adapter_config.json").is_file():
        raise FileNotFoundError(
            f"Existing adapter not found at {adapter_path}. Training is not started automatically."
        )
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA GPU is required for 4-bit base-vs-adapter evaluation.")

    compute_dtype = torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
    quantization = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_use_double_quant=True,
        bnb_4bit_compute_dtype=compute_dtype,
    )
    tokenizer = AutoTokenizer.from_pretrained(args.model, use_fast=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    torch.cuda.reset_peak_memory_stats()
    base_model = AutoModelForCausalLM.from_pretrained(
        args.model,
        quantization_config=quantization,
        device_map="auto",
        dtype=compute_dtype,
    )
    base_results = generate_responses(base_model, tokenizer, args.max_new_tokens)
    base_peak_gib = torch.cuda.max_memory_allocated() / (1024**3)
    del base_model
    gc.collect()
    torch.cuda.empty_cache()

    torch.cuda.reset_peak_memory_stats()
    adapter_model = AutoPeftModelForCausalLM.from_pretrained(
        str(adapter_path),
        quantization_config=quantization,
        device_map="auto",
        dtype=compute_dtype,
    )
    adapter_results = generate_responses(adapter_model, tokenizer, args.max_new_tokens)
    adapter_peak_gib = torch.cuda.max_memory_allocated() / (1024**3)
    del adapter_model
    gc.collect()
    torch.cuda.empty_cache()

    for prompt, base, adapter in zip(PROMPTS, base_results, adapter_results):
        print("\n" + "=" * 88)
        print(f"USER: {prompt}")
        print(f"\nBASE:\n{base}")
        print(f"\nADAPTER:\n{adapter}")
    print(f"\nPeak VRAM base: {base_peak_gib:.2f} GiB")
    print(f"Peak VRAM adapter: {adapter_peak_gib:.2f} GiB")


if __name__ == "__main__":
    main()
