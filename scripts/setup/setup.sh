#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

LOG_DIR="$SCRIPT_DIR/../logs/setup/$(date +%s)"
mkdir -p "$LOG_DIR"
printf "💬 Logs: %s\n" "$LOG_DIR"

source "$SCRIPT_DIR/../build/stages/build_env.sh" "$LOG_DIR"
