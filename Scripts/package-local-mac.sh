#!/usr/bin/env bash
# Local arm64 Development build. All intermediate signing happens outside File Provider folders.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
engine="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
uat="$engine/Engine/Build/BatchFiles/RunUAT.sh"
stage="$root/Saved/MacPackage"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/sabarmati-local-package.XXXXXXXX")"
trap 'rm -rf "$scratch"' EXIT
printf '%s\n' 'CODE_SIGNING_ALLOWED = NO' > "$scratch/LocalBuild.xcconfig"
export XCODE_XCCONFIG_FILE="$scratch/LocalBuild.xcconfig"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi

sign_local_bundle() {
    local source_bundle="$1" work_bundle="$scratch/$2/SabarmatiPrototype.app"
    ditto --norsrc --noextattr "$source_bundle" "$work_bundle"
    codesign --force --sign - --timestamp=none "$work_bundle"
    codesign --verify --strict "$work_bundle"
    # UAT stages CodeResources explicitly. Restore both pieces of the same signature.
    ditto --norsrc --noextattr "$work_bundle/Contents/MacOS" "$source_bundle/Contents/MacOS"
    ditto --norsrc --noextattr "$work_bundle/Contents/_CodeSignature" "$source_bundle/Contents/_CodeSignature"
    codesign --verify "$source_bundle"
}

args=(BuildCookRun "-project=$root/SabarmatiPrototype.uproject" -platform=Mac -architecture=arm64
    -clientconfig=Development -map=/Engine/Maps/Entry)
bash "$uat" "${args[@]}" -build -cook "$@"
sign_local_bundle "$root/Binaries/Mac/SabarmatiPrototype.app" build
bash "$uat" "${args[@]}" -skipbuild -skipcook -stage -pak -iostore -package -archive "-archivedirectory=$scratch/output" "$@"
app="$scratch/output/Mac/SabarmatiPrototype.app"
if [[ ! -d "$app" ]]; then app="$scratch/output/SabarmatiPrototype.app"; fi
content="$app/Contents/UE/SabarmatiPrototype/Content/Paks"
if ! compgen -G "$content/*.utoc" >/dev/null || ! compgen -G "$content/*.ucas" >/dev/null || ! compgen -G "$content/*.pak" >/dev/null; then
    printf '%s\n' 'The archive is missing cooked content containers.' >&2
    exit 1
fi
# UAT changes the content of the .app again during archiving.
if [[ -e "$app/Contents/UE/ue.projectstore" ]]; then
    printf '%s\n' 'A development cook-server marker was staged in the offline package.' >&2
    exit 1
fi
sign_local_bundle "$app" archive
mkdir -p "$stage"
destination="$stage/SabarmatiPrototype.app"
if [[ -e "$destination" ]]; then mv "$destination" "$scratch/previous.app"; fi
if ! mv "$app" "$destination"; then
    if [[ -e "$scratch/previous.app" ]]; then mv "$scratch/previous.app" "$destination"; fi
    exit 1
fi
app="$destination"
printf 'Local Development app: %s\n' "$app"
