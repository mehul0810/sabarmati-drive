#!/usr/bin/env bash
# Run the cooked game in its normal fitted window and record macOS focus separately.
set -euo pipefail
minutes="${1:-20}"
if [[ $# -gt 2 || ! "$minutes" =~ ^([1-9]|[1-5][0-9]|60)$ || ( $# == 2 && "${2:-}" != --4k ) ]]; then
    printf '%s\n' 'Usage: bash Scripts/benchmark-standalone-mac.sh [minutes 1–60] [--4k]' >&2
    exit 2
fi
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "$root/Scripts/mac-game-path.sh"
app="$sabarmati_app"
if [[ ! -x "$app/Contents/MacOS/SabarmatiPrototype" ]]; then
    printf '%s\n' 'Build the standalone game with bash Scripts/run-mac.sh --package first.' >&2
    exit 1
fi
run="$root/Saved/StandaloneBenchmarks/$(uuidgen)"
log="$run/engine.log"
mkdir -p "$run"
printf 'Standalone review: %s\n' "$run"
/usr/bin/pmset -g batt > "$run/power-before.txt"
printf '%s\n' 'unix_time,game_instances,game_frontmost,screen_locked' > "$run/window-focus.csv"
options=(--standalone)
if [[ "${2:-}" == --4k ]]; then options+=(--4k); fi
bash "$root/Scripts/run-mac.sh" "${options[@]}" -- "-UserDir=$run" "-abslog=$log" \
    -SabarmatiSystemsReview "-SabarmatiEndurance=$minutes"

deadline=$((SECONDS + minutes * 60 + 240))
while ((SECONDS < deadline)); do
    /usr/bin/osascript -l JavaScript - "$app" >> "$run/window-focus.csv" <<'JXA'
function run(args) {
    ObjC.import('AppKit');
    ObjC.import('CoreGraphics');
    const workspace = $.NSWorkspace.sharedWorkspace;
    const applications = workspace.runningApplications;
    const frontPid = Number(workspace.frontmostApplication.processIdentifier);
    const games = [];
    for (let i = 0; i < applications.count; i++) {
        const app = applications.objectAtIndex(i);
        if (String(ObjC.unwrap(app.bundleURL.path) || '') === args[0]) {
            games.push(Number(app.processIdentifier));
        }
    }
    const session = ObjC.deepUnwrap($.CGSessionCopyCurrentDictionary());
    return [Math.floor(Date.now() / 1000), games.length,
            Number(games.includes(frontPid)), Number(!!session.CGSSessionScreenIsLocked)].join(',');
}
JXA
    if [[ -f "$log" ]]; then
        if grep -q 'Sabarmati systems review complete:' "$log"; then break; fi
        if grep -Eq 'Sabarmati systems review failed|Fatal error:' "$log"; then
            printf 'Game review failed. See %s\n' "$log" >&2
            exit 1
        fi
    fi
    sleep 15
done
if [[ ! -f "$log" ]] || ! grep -q 'Sabarmati systems review complete:' "$log"; then
    printf 'Timed out waiting for the game review. See %s\n' "$log" >&2
    exit 1
fi
/usr/bin/pmset -g batt > "$run/power-after.txt"
shopt -s nullglob
summaries=("$run"/Saved/SystemsReviews/*/endurance.txt)
if [[ ${#summaries[@]} != 1 ]]; then
    printf 'Expected one completed endurance recording under %s\n' "$run" >&2
    exit 1
fi
python3 "$root/Scripts/report-district-endurance.py" "$(dirname "${summaries[0]}")" --minutes "$minutes" | tee "$run/result.txt"
