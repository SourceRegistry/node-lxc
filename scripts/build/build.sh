#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

LOG_DIR="${1:-"$SCRIPT_DIR/../logs/build/$(date +%s)"}"
mkdir -p "$LOG_DIR"

printf "🚀 Building '%s'\n" "$(node -p "require('./package.json').name")"
printf "💬 Logs: %s\n" "$LOG_DIR"

rm -rf ./package 2>>"$LOG_DIR/remove.log" || true

source "$SCRIPT_DIR/stages/build_env.sh"  "$LOG_DIR"
source "$SCRIPT_DIR/stages/build_cpp.sh"  "$LOG_DIR"
source "$SCRIPT_DIR/stages/build_ts.sh"   "$LOG_DIR"
