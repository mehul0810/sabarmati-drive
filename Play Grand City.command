#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$root/Scripts/mac-game-path.sh"
if [[ $# -eq 0 ]]; then
    if [[ -x "$sabarmati_app/Contents/MacOS/SabarmatiPrototype" ]]; then
        exec bash "$root/Scripts/run-mac.sh" --standalone
    fi
    exec bash "$root/Scripts/run-mac.sh" --game --no-build --desktop -- -SabarmatiPublicAvatar
fi
exec bash "$root/Scripts/run-mac.sh" --game --no-build "$@"
