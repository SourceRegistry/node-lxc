#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

LOG_DIR="$SCRIPT_DIR/../logs/build/$(date +%s)"
mkdir -p "$LOG_DIR"

printf "📦 Packaging '%s'\n" "$(node -p "require('./package.json').name")"
printf "💬 Logs: %s\n" "$LOG_DIR"

rm -rf ./package 2>>"$LOG_DIR/remove.log" || true

bash "$SCRIPT_DIR/../build/build.sh" "$LOG_DIR"
source "$SCRIPT_DIR/stages/package_miscellaneous.sh" "$LOG_DIR"
