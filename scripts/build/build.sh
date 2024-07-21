#!/bin/bash

printf "🚀 Starting Build for '$(node -p "require('./package.json').name")'\n"

SCRIPT_DIR=$(realpath $(dirname $0))

EPOCH=$(date +%s)

#region Setup logging
LOG_DIR="$SCRIPT_DIR/../logs/build/$EPOCH"
mkdir -p $LOG_DIR
printf "💬 You can find the logs in the script directory($LOG_DIR)\n"
#endregion

rm -r ./package &>>"$LOG_DIR/stage-remove.log"


source "$SCRIPT_DIR/stages/build_env.sh" $LOG_DIR
source "$SCRIPT_DIR/stages/build_cpp.sh" $LOG_DIR
source "$SCRIPT_DIR/stages/build_ts.sh" $LOG_DIR
source "$SCRIPT_DIR/stages/build_miscellaneous.sh" $LOG_DIR

#{
#  printf "️💫 Running NodeJS Packaging script" &&
#    "$SCRIPT_DIR/stages/build_package_json.js" &>>"$LOG_DIR/action-build_package_json.log" &&
#    printf " 🟢\n"
#} ||
#  {
#    printf " 🔴\n"
#    exit 1
#  }
