#!/usr/bin/env bash
# Build the two supported release targets; Linux uses Zig for glibc 2.17 compatibility.
set -euo pipefail

cd "$(dirname "$0")/.."
python_bin="${PYTHON:-python3}"
output_dir="${1:-dist}"

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64)
        "$python_bin" -m maturin build --release --locked --strip \
            --target x86_64-unknown-linux-gnu --zig \
            --compatibility manylinux2014 --auditwheel check \
            --interpreter "$python_bin" --out "$output_dir"
        ;;
    Darwin-arm64)
        MACOSX_DEPLOYMENT_TARGET=11.0 "$python_bin" -m maturin build \
            --release --locked --strip --target aarch64-apple-darwin \
            --interpreter "$python_bin" --out "$output_dir"
        ;;
    *)
        echo "Supported wheel build hosts: Linux x86_64 and macOS arm64" >&2
        exit 1
        ;;
esac
