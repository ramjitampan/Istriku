"""Run a small qualitative evaluation against a trained Yukihana adapter."""

from __future__ import annotations

import argparse

import torch
from peft import AutoPeftModelForCausalLM
from transformers import AutoTokenizer


PROMPTS = [
    "Aku capek dan project-ku masih error.",
    "Tolong edit file auth.cpp sekarang.",
    "Pin keputusan database ini selama seminggu.",
    "Besok ingatkan aku melanjutkan coding.",
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adapter", default="outputs/yukihana-qlora")
    parser.add_argument("--max-new-tokens", type=int, default=160)
    args = parser.parse_args()

    tokenizer = AutoTokenizer.from_pretrained(args.adapter)
    model = AutoPeftModelForCausalLM.from_pretrained(
        args.adapter,
        device_map="auto",
        torch_dtype="auto",
    )
    model.eval()

    for prompt in PROMPTS:
        messages = [{"role": "user", "content": prompt}]
        inputs = tokenizer.apply_chat_template(
            messages,
            add_generation_prompt=True,
            return_tensors="pt",
        ).to(model.device)
        with torch.inference_mode():
            output = model.generate(
                inputs,
                max_new_tokens=args.max_new_tokens,
                do_sample=False,
                pad_token_id=tokenizer.eos_token_id,
            )
        response = tokenizer.decode(output[0, inputs.shape[-1]:], skip_special_tokens=True)
        print(f"\nUSER: {prompt}\nYUKIHANA: {response.strip()}")


if __name__ == "__main__":
    main()
