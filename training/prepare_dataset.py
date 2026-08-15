"""Validate Yukihana conversational JSONL datasets before training."""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any


ALLOWED_ROLES = {"system", "user", "assistant", "tool"}
ALLOWED_TOOLS = {
    "memory_save", "memory_search", "memory_delete", "memory_pin_current_context",
    "create_reminder", "reminder_search", "reminder_cancel",
    "list_files", "search_files", "read_file", "prepare_patch", "apply_patch",
    "git_status", "git_diff", "run_command", "run_tests", "inspect_image",
}
TOOL_CALL_PATTERN = re.compile(r"^<tool_call>\n(\{.*\})\n</tool_call>$", re.DOTALL)
HARDCODED_TIMESTAMP_PATTERN = re.compile(
    r"\b20\d{2}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:Z|[+-]\d{2}:\d{2})\b"
)


def load_and_validate(path: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                raise ValueError(f"{path}:{line_number}: invalid JSON: {error}") from error

            messages = record.get("messages")
            if not isinstance(messages, list) or len(messages) < 2:
                raise ValueError(f"{path}:{line_number}: messages must contain at least two items")
            if messages[-1].get("role") != "assistant":
                raise ValueError(f"{path}:{line_number}: final message must be assistant")

            for message_index, message in enumerate(messages):
                if not isinstance(message, dict):
                    raise ValueError(f"{path}:{line_number}: message {message_index} must be an object")
                if message.get("role") not in ALLOWED_ROLES:
                    raise ValueError(f"{path}:{line_number}: unsupported role in message {message_index}")
                if not isinstance(message.get("content"), str) or not message["content"].strip():
                    raise ValueError(f"{path}:{line_number}: empty content in message {message_index}")

                if message_index > 0 and messages[message_index - 1]["role"] == message["role"]:
                    raise ValueError(
                        f"{path}:{line_number}: adjacent {message['role']} roles at message {message_index}"
                    )

                content = message["content"]
                if HARDCODED_TIMESTAMP_PATTERN.search(content):
                    raise ValueError(f"{path}:{line_number}: hardcoded timestamp in message {message_index}")

                if message["role"] == "assistant" and "<tool_call>" in content:
                    match = TOOL_CALL_PATTERN.fullmatch(content)
                    if not match:
                        raise ValueError(f"{path}:{line_number}: invalid tool call wrapper")
                    try:
                        tool_call = json.loads(match.group(1))
                    except json.JSONDecodeError as error:
                        raise ValueError(f"{path}:{line_number}: invalid tool call JSON: {error}") from error
                    if tool_call.get("name") not in ALLOWED_TOOLS:
                        raise ValueError(f"{path}:{line_number}: unsupported tool {tool_call.get('name')}")
                    if not isinstance(tool_call.get("arguments"), dict):
                        raise ValueError(f"{path}:{line_number}: tool arguments must be an object")

                    if message_index + 1 >= len(messages) or messages[message_index + 1]["role"] != "tool":
                        raise ValueError(f"{path}:{line_number}: tool call must be followed by tool result")
            records.append(record)
    return records


def fingerprint(record: dict[str, Any]) -> str:
    return json.dumps(record["messages"], ensure_ascii=False, sort_keys=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--train", type=Path, default=Path("dataset/yukihana_train.jsonl"))
    parser.add_argument("--validation", type=Path, default=Path("dataset/yukihana_validation.jsonl"))
    args = parser.parse_args()

    train = load_and_validate(args.train)
    validation = load_and_validate(args.validation)
    duplicates = set(map(fingerprint, train)) & set(map(fingerprint, validation))
    if duplicates:
        raise ValueError("training and validation datasets contain duplicate conversations")

    role_counts = Counter(
        message["role"]
        for record in train + validation
        for message in record["messages"]
    )
    print(f"Training records: {len(train)}")
    print(f"Validation records: {len(validation)}")
    print(f"Message roles: {dict(sorted(role_counts.items()))}")
    print("Dataset validation passed.")


if __name__ == "__main__":
    main()
