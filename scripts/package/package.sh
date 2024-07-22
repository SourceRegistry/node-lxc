#!/bin/bash

printf "📦 Starting Packaging '$(node -p "require('./package.json').name")'\n"

SCRIPT_DIR=$(realpath $(dirname $0))

#region Setup logging
LOG_DIR="$SCRIPT_DIR/../logs/build/$(date +%s)"
mkdir -p "$LOG_DIR"
printf "💬 You can find the logs in the script directory(%s)\n" "$LOG_DIR"
#endregion

rm -r ./package &>>"$LOG_DIR/stage-remove.log"


sh -c "$SCRIPT_DIR/../build/build.sh" "$LOG_DIR"
source "$SCRIPT_DIR/stages/package_miscellaneous.sh" "$LOG_DIR"

#{
#  printf "️💫 Running NodeJS Packaging script" &&
#    "$SCRIPT_DIR/stages/build_package_json.js" &>>"$LOG_DIR/action-build_package_json.log" &&
#    printf " 🟢\n"
#} ||
#  {
#    printf " 🔴\n"
#    exit 1
#  }
