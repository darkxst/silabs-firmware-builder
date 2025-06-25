#!/bin/bash
set -e

# Find all YAML manifests
mapfile -t manifests < <(find ./manifests -name '*.yaml')

if [ ${#manifests[@]} -eq 0 ]; then
    echo "No manifest files found."
    exit 1
fi

echo "Select a manifest to build:"
select manifest in "${manifests[@]}"; do
    if [[ -n "$manifest" ]]; then
        echo "Building with $manifest"
        ./tools/buildit.sh "$manifest"
        echo "Build completed successfully."
        exit 0
    else
        echo "Invalid selection."
    fi
done