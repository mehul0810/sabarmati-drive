#!/usr/bin/env bash
# Read-only diagnostics apart from writing compiler probes to a temporary directory.
set -uo pipefail

if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    printf 'Use a native Apple Silicon macOS terminal.\n' >&2
    exit 1
fi

status=0
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi

engine="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
printf 'macOS %s (%s)\n' "$(sw_vers -productVersion)" "$(uname -m)"
printf 'Xcode for this process: %s\n' "${DEVELOPER_DIR:-$(xcode-select -p 2>/dev/null || printf 'not selected')}"
printf 'Build volume space:\n'
df -h "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
if [[ -x "$engine/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" && -f "$engine/Engine/Build/BatchFiles/Mac/Build.sh" ]]; then
    printf 'Unreal Engine found at %s\n' "$engine"
    if [[ -f "$engine/Engine/Build/Build.version" ]]; then
        plutil -extract MajorVersion raw "$engine/Engine/Build/Build.version" >/dev/null 2>&1 &&
            printf 'Engine version: %s.%s.%s\n' \
                "$(plutil -extract MajorVersion raw "$engine/Engine/Build/Build.version")" \
                "$(plutil -extract MinorVersion raw "$engine/Engine/Build/Build.version")" \
                "$(plutil -extract PatchVersion raw "$engine/Engine/Build/Build.version")"
    fi
else
    printf 'Unreal Engine not found at %s. Set UE_ROOT for a different install.\n' "$engine" >&2
    status=1
fi
if ! xcodebuild -version || ! xcrun --sdk macosx --show-sdk-version; then
    printf 'Full Xcode and the macOS SDK are required.\n' >&2
    exit 1
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/sabarmati-preflight.XXXXXX")" || exit 1
printf 'int main(){return 0;}\n' > "$tmp/probe.cpp"
printf '#include <metal_stdlib>\nusing namespace metal;\nkernel void probe(device uint* v [[buffer(0)]]) { v[0] = 1; }\n' > "$tmp/probe.metal"
if xcrun --sdk macosx clang++ -arch arm64 "$tmp/probe.cpp" -o "$tmp/probe" && "$tmp/probe"; then
    printf 'Apple Silicon C++ compile and run: passed.\n'
else
    printf 'Apple Silicon C++ compile or run: failed.\n' >&2
    status=1
fi
if xcrun --sdk macosx metal -c "$tmp/probe.metal" -o "$tmp/probe.air"; then
    printf 'Metal shader compile: passed.\n'
else
    printf 'Metal shader compile: failed. Check Xcode > Settings > Components.\n' >&2
    status=1
fi
printf 'This check does not compile Unreal or prove runtime compatibility.\n'
exit "$status"
