#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ -x "$root/Saved/MacPackage/SabarmatiPrototype.app/Contents/MacOS/SabarmatiPrototype" ]]; then
    exec bash "$root/Scripts/run-mac.sh" --standalone --4k
fi
exec bash "$root/Scripts/run-mac.sh" --game --no-build --4k -- -SabarmatiPublicAvatar
