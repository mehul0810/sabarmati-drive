#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: bash Scripts/run-mac.sh [--editor|--game|--package] [--no-build] [--4k] [-- extra UE args]
       UE_ROOT="/path/to/UE_5.8" bash Scripts/run-mac.sh
The default is a 1600x900 game window. --4k fits the active display in native fullscreen.
--editor opens the source for Play and import.
--package cooks a local Development .app under Saved/MacPackage (no upload or signing).
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
if [[ ! -x "$editor" || ! -f "$mac_build" ]]; then
    printf 'Unreal Engine 5.8 not found at "%s". Install it from Epic or set UE_ROOT.\n' "$engine" >&2
    exit 1
fi

if [[ "$mode" == package || "$skip_build" == false ]]; then
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
    exec bash "$uat" BuildCookRun "-project=$project" -platform=Mac -architecture=arm64 \
        -clientconfig=Development -build -cook -map=/Engine/Maps/Entry -stage -package -archive \
        "-archivedirectory=$root/Saved/MacPackage" "$@"
fi

if [[ "$skip_build" == false ]]; then
    bash "$mac_build" SabarmatiPrototypeEditor Mac Development "$project" -WaitMutex
fi

if [[ "$mode" == editor ]]; then
    exec "$editor" "$project" "$@"
fi

if [[ "$uhd" == true ]]; then
    # macOS uses logical points for window placement and backing pixels for Metal.
    exec "$editor" "$project" -game -fullscreen -Sabarmati4K -metalsm5 \
        '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True' "$@"
fi
exec "$editor" "$project" -game -windowed -ResX=1600 -ResY=900 -metalsm5 "$@"
