LOG_DIR="${1:?build_ts.sh requires a log directory as argument 1}"
LOG_FILE="${2:-stage-build_ts}"
LOG="$LOG_DIR/$LOG_FILE.log"

step() {
  local desc="$1"; shift
  printf "%s" "$desc"
  if "$@" &>>"$LOG"; then
    printf " 🟢\n"
  else
    printf " 🔴\n   SEE: %s\n" "$LOG"
    exit 1
  fi
}

printf "🔨 TypeScript\n"
step "   🔨  Compiling"    tsc --build
step "   ➡️  Staging lib"  bash -c "mkdir -p ./package/lib && cp -r build/tsc/* ./package/lib"
