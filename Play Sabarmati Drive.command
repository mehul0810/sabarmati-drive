#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ $# -eq 0 ]]; then
    if [[ -x "$root/Saved/MacPackage/SabarmatiPrototype.app/Contents/MacOS/SabarmatiPrototype" ]]; then
        exec bash "$root/Scripts/run-mac.sh" --standalone
    fi
    exec bash "$root/Scripts/run-mac.sh" --game --no-build -- -SabarmatiPublicAvatar
fi
exec bash "$root/Scripts/run-mac.sh" --game --no-build "$@"
