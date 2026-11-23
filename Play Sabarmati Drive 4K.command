#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
exec bash "$root/Scripts/run-mac.sh" --game --no-build --4k -- -SabarmatiCity
