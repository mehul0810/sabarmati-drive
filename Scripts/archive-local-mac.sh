#!/usr/bin/env bash
# Keep the locally signed app portable when the checkout sits in a synced Documents folder.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "$root/Scripts/mac-game-path.sh"
app="$sabarmati_app"
archive="$root/Saved/MacPackage/Sabarmati-Drive-Mac-arm64.zip"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/sabarmati-mac-archive.XXXXXXXX")"
trap 'rm -rf "$scratch"' EXIT
if [[ ! -d "$app" ]]; then
    echo 'Build the Mac app with bash Scripts/run-mac.sh --package first.' >&2
    exit 1
fi
clean="$scratch/SabarmatiPrototype.app"
ditto --norsrc --noextattr "$app" "$clean"
codesign --verify --strict "$clean"
ditto -c -k --norsrc --noextattr --keepParent "$clean" "$scratch/game.zip"
unzip -tq "$scratch/game.zip" >/dev/null
mkdir -p "$root/Saved/MacPackage"
mv -f "$scratch/game.zip" "$archive"
printf 'Local Mac archive: %s\n' "$archive"
