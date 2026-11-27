#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$root/Scripts/mac-game-path.sh"
if [[ -x "$sabarmati_app/Contents/MacOS/SabarmatiPrototype" ]]; then
    exec bash "$root/Scripts/run-mac.sh" --standalone --4k
fi
exec bash "$root/Scripts/run-mac.sh" --game --no-build --desktop --4k -- -SabarmatiPublicAvatar
