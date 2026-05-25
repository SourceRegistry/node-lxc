LOG_DIR="${1:?build_env.sh requires a log directory as argument 1}"
LOG_FILE="${2:-stage-build_env}"
LOG="$LOG_DIR/$LOG_FILE.log"

if [ "$(id -u)" -eq 0 ]; then
  SUDO=""
elif command -v sudo &>/dev/null; then
  SUDO="sudo"
else
  SUDO=""
fi

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

printf "🚧 Setting up build environment\n"
step "   🎛️  Updating system"           $SUDO apt update -y
step "   🎛️  Upgrading system"          $SUDO apt upgrade -y
step "   📚  Installing g++ cmake"      $SUDO apt install g++ cmake -y
step "   🗳️  Installing lxc lxc-dev"   $SUDO apt install lxc lxc-dev -y
step "   📦  Installing npm deps"       npm install
step "   🔩  Configuring node-gyp"      bash -c "node-gyp clean && node-gyp configure"
