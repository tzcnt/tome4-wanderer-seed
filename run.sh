#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

if [ "${1:-}" = "generate" ]; then
    ./build/wanderer_seed generate "${2:-12345}" unlocks.json
else
    ./build/wanderer_seed search unlocks.json "${2:-rules.json}"
fi
