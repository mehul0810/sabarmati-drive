#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd -P)"
case "${1:-all}" in
    all) times=(reference day golden night) ;;
    reference|day|golden|night) times=("$1") ;;
    *) echo "Usage: bash Scripts/benchmark-riverfront.sh [all|reference|day|golden|night] [--no-build]" >&2; exit 2 ;;
esac
build=(--game)
if [[ "${2:-}" == --no-build ]]; then build=(--game --no-build); fi
if [[ $# -gt 2 || ( $# -eq 2 && "$2" != --no-build ) ]]; then
    echo "The second argument, if supplied, must be --no-build." >&2
    exit 2
fi
for time in "${times[@]}"; do
    scratch="$(mktemp -d -t sabarmati-benchmark)"
    log="$scratch/engine.log"
    options=(-SabarmatiBenchmark)
    if [[ "$time" == reference ]]; then
        options+=(-SabarmatiLightingReference)
    else
        options+=("-SabarmatiTime=$time")
    fi
    # Rendering measurements run serially, using a different profile and output directory each time.
    # macOS can return success on a requested engine exit after a failed check; require the marker and assets.
    bash "$root/Scripts/run-mac.sh" "${build[@]}" -- "${options[@]}" "-abslog=$log" ||
        { echo "Game launch failed. See $log" >&2; exit 1; }
    folder="$(sed -n 's/^.*Sabarmati benchmark complete: //p' "$log" | tail -1)"
    if [[ -z "$folder" || ! -d "$folder" ]]; then
        echo "Incomplete benchmark. See $log" >&2
        exit 1
    fi
    if [[ "$folder" != "$root/Saved/RenderingBenchmarks/"* ]]; then
        echo "Unexpected output directory in $log" >&2
        exit 1
    fi
    mv "$log" "$folder/engine.log"
    rmdir "$scratch"
    python3 "$root/Scripts/report-rendering-benchmark.py" "$folder"
    echo "Captures and frame samples: $folder"
    build=(--game --no-build)
done
