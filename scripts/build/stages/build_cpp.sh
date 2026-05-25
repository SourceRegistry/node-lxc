LOG_DIR="${1:?build_cpp.sh requires a log directory as argument 1}"
LOG_FILE="${2:-stage-build_cpp}"
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

printf "🧩 C/C++ addon\n"
step "   🏗️  Building C/C++ bindings"         npx node-gyp build
step "   ➡️  Staging x86_64-linux-gnu binary"  bash -c "
  mkdir -p ./package/bin/x86_64-linux-gnu &&
  cp build/Release/node-lxc.node ./package/bin/x86_64-linux-gnu/node-lxc.node
"
