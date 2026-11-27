#!/usr/bin/env bash
# Local arm64 Development build. All intermediate signing happens outside File Provider folders.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
engine="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
uat="$engine/Engine/Build/BatchFiles/RunUAT.sh"
export_app=
if [[ "${1:-}" == --export ]]; then
    if [[ $# -lt 2 || "$2" != /*.app || -e "$2" || -L "$2" || ! -d "$(dirname "$2")" ]]; then
        printf '%s\n' '--export needs a new absolute .app path in an existing directory.' >&2
        exit 2
    fi
    export_app="$2"
    shift 2
fi

# Serialize the Unreal build product shared by packaging invocations in this checkout.
mkdir -p "$root/Saved"
exec 9>>"$root/Saved/.sabarmati-package.lock"
python3 -c 'import fcntl; fcntl.flock(9, fcntl.LOCK_EX)'
build_app="$root/Binaries/Mac/SabarmatiPrototype.app"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/sabarmati-local-package.XXXXXXXX")"
# Xcode registers its build and staging products with Launch Services. Remove
# those two exact registrations before retiring their generated bundles.
cleanup() {
    local status=$? product identifier
    local register=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
    for product in "$build_app" "$scratch/stage/Mac/SabarmatiPrototype.app"; do
        [[ -d "$product" && ! -L "$product" ]] || continue
        identifier=$(/usr/bin/plutil -extract CFBundleIdentifier raw -o - "$product/Contents/Info.plist" 2>/dev/null) || continue
        [[ "$identifier" == com.YourCompany.SabarmatiPrototype ]] || continue
        if [[ -x "$register" ]]; then "$register" -u "$product" >/dev/null 2>&1 || true; fi
        if [[ "$product" == "$build_app" ]]; then
            mv "$product" "$scratch/compiled-product" || status=1
        fi
    done
    rm -rf "$scratch"
    exit "$status"
}
trap cleanup EXIT
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
bash "$uat" "${args[@]}" -build -cook "$@" 9>&-
sign_local_bundle "$build_app" build
bash "$uat" "${args[@]}" -skipbuild -skipcook -stage -pak -iostore -package -archive "-stagingdirectory=$scratch/stage" "-archivedirectory=$scratch/output" "$@" 9>&-
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
/usr/bin/plutil -replace CFBundleName -string 'Grand City' "$app/Contents/Info.plist"
/usr/bin/plutil -replace CFBundleDisplayName -string 'Grand City' "$app/Contents/Info.plist"
sign_local_bundle "$app" archive
if [[ -n "$export_app" ]]; then
    /usr/bin/ditto --norsrc --noextattr "$app" "$export_app"
    /usr/bin/codesign --verify --strict "$export_app"
    printf 'Cooked test app: %s\n' "$export_app"
else
    python3 "$root/Scripts/install-mac-game.py" "$app"
fi
