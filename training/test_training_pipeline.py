"""Preflight tests for the Yukihana v0.2 dataset and assistant-only loss mask."""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path

from transformers import AutoTokenizer
from trl.chat_template_utils import get_training_chat_template

from build_v02_dataset import TARGETS
from train_qlora import verify_assistant_mask
from yukihana_prompt import SYSTEM_PROMPT


def load_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def main() -> None:
    train = load_jsonl(Path("dataset/yukihana_train_v02.jsonl"))
    validation = load_jsonl(Path("dataset/yukihana_validation_v02.jsonl"))
    assert len(train) == 220, len(train)
    assert len(validation) == 25, len(validation)
    assert Counter(item["category"] for item in train) == Counter(TARGETS)

    for item in train + validation:
        systems = [message for message in item["messages"] if message["role"] == "system"]
        assert len(systems) == 1
        assert systems[0]["content"] == SYSTEM_PROMPT

    tool_record = next(
        item for item in train
        if any("<tool_call>" in message["content"] for message in item["messages"])
    )
    tokenizer = get_training_chat_template(
        AutoTokenizer.from_pretrained("Qwen/Qwen2.5-3B-Instruct", use_fast=True)
    )
    serialized = tokenizer.apply_chat_template(
        tool_record["messages"], tokenize=False, add_generation_prompt=False
    )
    assert "<tool_call>\n" in serialized
    assert "\n</tool_call>" in serialized
    mask = verify_assistant_mask(tokenizer, tool_record["messages"])
    assert mask["tool_call_tokens_active"] > 0
    assert mask["assistant_tokens"] > 0
    assert mask["masked_tokens"] > 0
    print("Pipeline tests passed.")
    print(json.dumps(mask, indent=2))


if __name__ == "__main__":
    main()
