#!/bin/bash

set -e

# Fix `fatal: detected dubious ownership in repository at`
git config --global --add safe.directory "$(pwd)"

# Pass all SDKs as consecutive `--sdk ...` arguments
sdk_args=""
for sdk_dir in /*_sdk*; do
    sdk_args="$sdk_args --sdk $sdk_dir"
done

# Pass all toolchains as consecutive `--toolchain ...` arguments
toolchain_args=""
for toolchain_dir in /opt/*arm-none-eabi*; do
    toolchain_args="$toolchain_args --toolchain $toolchain_dir"
done

# Build it
python3 tools/build_project.py \
    $sdk_args \
    $toolchain_args \
    --manifest "$1" \
    --build-dir build \
    --build-system makefile \
    --output-dir outputs \
    --output gbl \
    --output hex \
    --output out

# Get the basename of the GBL in `outputs`
output_basename=$(basename -- $(basename -- $(ls -1 outputs/*.gbl | head -n 1)) .gbl)
echo "output_basename=$output_basename"