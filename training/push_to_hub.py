"""Upload a reviewed Yukihana adapter directory to Hugging Face Hub."""

from __future__ import annotations

import argparse
from pathlib import Path

from huggingface_hub import HfApi


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model-dir", type=Path, default=Path("outputs/yukihana-qlora"))
    parser.add_argument("--repo-id", required=True, help="For example: username/Yukihana-3B-QLoRA")
    parser.add_argument("--private", action="store_true")
    parser.add_argument("--yes", action="store_true", help="Confirm that the upload was reviewed")
    args = parser.parse_args()

    if not args.yes:
        raise SystemExit("Upload cancelled. Review the adapter, then rerun with --yes.")
    if not args.model_dir.is_dir():
        raise SystemExit(f"Model directory does not exist: {args.model_dir}")

    api = HfApi()
    api.create_repo(args.repo_id, private=args.private, exist_ok=True, repo_type="model")
    api.upload_folder(
        repo_id=args.repo_id,
        repo_type="model",
        folder_path=args.model_dir,
        commit_message="Upload Yukihana QLoRA adapter",
    )


if __name__ == "__main__":
    main()
