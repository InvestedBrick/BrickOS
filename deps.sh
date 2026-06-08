#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "apt-get not found. This script requires Debian/Ubuntu."
  exit 1
fi

sudo apt-get update
sudo apt-get install -y qemu-system-x86 qemu-utils make nasm clang llvm