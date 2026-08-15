"""Run the guarded Yukihana v0.2 Colab experiment without any Hub upload."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


TRAINING_DIR = Path(__file__).resolve().parent


def run(command: list[str]) -> None:
    print("\nRUN:", " ".join(command), flush=True)
    subprocess.run(command, check=True, cwd=TRAINING_DIR)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default="Qwen/Qwen2.5-3B-Instruct")
    parser.add_argument("--output", default="outputs/yukihana-qwen2.5-3b-v02")
    args = parser.parse_args()

    output = (TRAINING_DIR / args.output).resolve()
    if (output / "adapter_config.json").exists():
        raise SystemExit(
            f"Refusing to overwrite existing v0.2 adapter: {output}. "
            "Run audit_adapter.py directly to re-evaluate it."
        )

    python = sys.executable
    run([python, "build_v02_dataset.py"])
    run([
        python, "prepare_dataset.py",
        "--train", "dataset/yukihana_train_v02.jsonl",
        "--validation", "dataset/yukihana_validation_v02.jsonl",
    ])
    run([python, "test_training_pipeline.py"])
    run([
        python, "train_qlora.py",
        "--model", args.model,
        "--train-file", "dataset/yukihana_train_v02.jsonl",
        "--validation-file", "dataset/yukihana_validation_v02.jsonl",
        "--output-dir", str(output),
        "--epochs", "2",
        "--max-length", "1024",
        "--batch-size", "1",
        "--gradient-accumulation", "8",
    ])
    run([
        python, "audit_adapter.py",
        "--model", args.model,
        "--adapter", str(output),
        "--train-file", "dataset/yukihana_train_v02.jsonl",
        "--report", str(output / "audit_report.json"),
    ])

    print("\nYukihana v0.2 experiment complete.")
    print("Training summary:", output / "training_summary.json")
    print("Audit report:", output / "audit_report.json")
    print("No Hugging Face upload was performed.")


if __name__ == "__main__":
    main()
