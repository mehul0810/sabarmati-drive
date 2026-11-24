#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: bash Scripts/run-mac.sh [--editor|--game|--standalone|--package] [--no-build] [--4k] [-- extra UE args]
       UE_ROOT="/path/to/UE_5.8" bash Scripts/run-mac.sh
Both game presets open a resizable window. The default targets 1600x900; --4k
uses Retina backing pixels up to 3840x2160. Both fit the desktop work area.
--editor opens the source for Play and import.
--standalone starts the cooked offline app with the same window fit and local player profile.
--package replaces ~/Applications/Sabarmati Drive.app with the local Development build.
USAGE
}

mode=game
skip_build=false
uhd=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --editor) mode=editor; shift ;;
        --game) mode=game; shift ;;
        --package) mode=package; shift ;;
        --standalone) mode=standalone; shift ;;
        --no-build) skip_build=true; shift ;;
        --4k) uhd=true; shift ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    printf 'Run this in a native arm64 Terminal on Apple Silicon macOS.\n' >&2
    exit 1
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
project="$root/SabarmatiPrototype.uproject"
engine="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
engine="${engine%/}"
# Use the installed full SDK for the game process without changing xcode-select globally.
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
mac_build="$engine/Engine/Build/BatchFiles/Mac/Build.sh"
uat="$engine/Engine/Build/BatchFiles/RunUAT.sh"
editor="$engine/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
if [[ "$mode" != standalone && ( ! -x "$editor" || ! -f "$mac_build" ) ]]; then
    printf 'Unreal Engine 5.8 not found at "%s". Install it from Epic or set UE_ROOT.\n' "$engine" >&2
    exit 1
fi

if [[ "$mode" == package || ( "$mode" != standalone && "$skip_build" == false ) ]]; then
    if ! xcodebuild -version >/dev/null 2>&1 || ! xcrun --sdk macosx --show-sdk-path >/dev/null 2>&1; then
        printf 'A full Xcode installation with an active macOS SDK is required. Open Xcode once and finish its setup.\n' >&2
        exit 1
    fi
fi

if [[ "$mode" == package ]]; then
    if [[ "$skip_build" == true ]]; then
        printf '%s\n' '--package always builds and cooks; remove --no-build.' >&2
        exit 2
    fi
    if [[ ! -f "$uat" ]]; then
        printf 'Unreal Automation Tool was not found in "%s".\n' "$engine" >&2
        exit 1
    fi
    exec bash "$root/Scripts/package-local-mac.sh" "$@"
fi

if [[ "$mode" != standalone && "$skip_build" == false ]]; then
    # The launcher starts a fresh process, including when another editor window is open.
    bash "$mac_build" SabarmatiPrototypeEditor Mac Development "$project" -WaitMutex -NoHotReloadFromIDE
fi

if [[ "$mode" == editor ]]; then
    exec "$editor" "$project" "$@"
fi

launcher=("$editor" "$project" -game)
# Keep normal editor-game and standalone progress together. The packaged app can
# open its own Application Support folder without asking to read all of Documents.
explicit_data=false
review=false
for argument in "$@"; do
    case "$argument" in
        -[Uu]ser[Dd]ir=*|-USERDIR=*) explicit_data=true ;;
        -SabarmatiBenchmark|-Sabarmati*Review|-SabarmatiEndurance=*) review=true ;;
    esac
done
if [[ "$mode" == standalone ]]; then
    source "$root/Scripts/mac-game-path.sh"
    if [[ ! -x "$sabarmati_app/Contents/MacOS/SabarmatiPrototype" ]]; then
        printf '%s\n' 'Install the game with bash Scripts/run-mac.sh --package first.' >&2
        exit 1
    fi
    launcher=(/usr/bin/open)
    if [[ "$review" == true || "$explicit_data" == true ]]; then
        # Isolated test profiles need their own process. Normal play reopens the current game.
        launcher+=(-n)
    fi
    launcher+=("$sabarmati_app" --args)
fi
if [[ "$explicit_data" == false && ( "$review" == false || "$mode" == standalone ) ]]; then
    data_dir="$(/usr/bin/osascript -l JavaScript <<'JXA'
ObjC.import('Foundation');
const directories = $.NSFileManager.defaultManager.URLsForDirectoryInDomains(
    $.NSApplicationSupportDirectory, $.NSUserDomainMask);
ObjC.unwrap(directories.firstObject.URLByAppendingPathComponent('Sabarmati Drive').path);
JXA
)"
    if [[ "$review" == false ]]; then
        profile_file="Saved/SaveGames/SabarmatiPlayerProfileV1.sav"
        if [[ -f "$root/$profile_file" && ! -e "$data_dir/$profile_file" ]]; then
            python3 "$root/Scripts/prepare-mac-profile.py" "$root/$profile_file" "$data_dir/$profile_file"
        fi
    fi
    set -- "$@" "-UserDir=$data_dir"
fi

# Match Unreal's default window placement on the primary display. NSScreen reports
# points; the high-DPI Unreal window takes a client size in backing pixels.
dpi=False
video_args=(-metalsm5)
if [[ "$uhd" == true ]]; then
    dpi=True
    video_args+=(-Sabarmati4K)
fi
size="$(/usr/bin/osascript -l JavaScript - "$uhd" <<'JXA' || true
function run(args) {
    ObjC.import('AppKit');
    const screen = $.NSScreen.screens.firstObject;
    const visible = screen.visibleFrame.size;
    const retina = args[0] === 'true';
    const scale = retina ? screen.backingScaleFactor : 1;
    // Leave room for the title bar, window border and desktop around it.
    const units = Math.floor(Math.min(retina ? 240 : 100,
        (visible.width - 64) * scale / 16, (visible.height - 84) * scale / 9));
    if (units > 0) return (16 * units) + ' ' + (9 * units);
}
JXA
)"
width=960
height=540
if [[ "$size" =~ ^([0-9]+)[[:space:]]([0-9]+)$ ]]; then
    width="${BASH_REMATCH[1]}"
    height="${BASH_REMATCH[2]}"
fi
exec "${launcher[@]}" -windowed "-ResX=$width" "-ResY=$height" \
    "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=$dpi" \
    "${video_args[@]}" "$@"
