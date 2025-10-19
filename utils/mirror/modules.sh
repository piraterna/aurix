#!/bin/bash
set -e

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
MIRROR_FILE="$ROOT_DIR/utils/mirror/mirror.gitmodules"
TARGET_FILE="$ROOT_DIR/.gitmodules"

echo "Using mirror gitmodules file..."
echo "Source: $MIRROR_FILE"
echo "Target: $TARGET_FILE"

if [[ ! -f "$MIRROR_FILE" ]]; then
    echo "Error: mirror file not found at $MIRROR_FILE"
    exit 1
fi

cp "$MIRROR_FILE" "$TARGET_FILE"
echo "Replaced .gitmodules with mirror version."

if git ls-files --error-unmatch .gitmodules >/dev/null 2>&1; then
    git update-index --assume-unchanged .gitmodules
    echo "Marked .gitmodules as 'assume-unchanged' to exclude from commits."
else
    echo ".gitmodules is not tracked — no need to exclude from commits."
fi

echo "Done."
