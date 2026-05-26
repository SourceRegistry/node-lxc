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

MACHINE=$(uname -m)
case "$MACHINE" in
  x86_64)  TRIPLET="x86_64-linux-gnu" ;;
  aarch64) TRIPLET="aarch64-linux-gnu" ;;
  armv7l)  TRIPLET="arm-linux-gnueabihf" ;;
  *) printf "Unsupported architecture: %s\n" "$MACHINE" >&2; exit 1 ;;
esac

printf "🧩 C/C++ addon\n"
step "   🏗️  Building C/C++ bindings"      npx node-gyp build
step "   ➡️  Staging binary ($TRIPLET)"    bash -c "
  mkdir -p ./package/bin/$TRIPLET &&
  cp build/Release/node-lxc.node ./package/bin/$TRIPLET/node-lxc.node
"
