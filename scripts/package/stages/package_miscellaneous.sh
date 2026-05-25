LOG_DIR="${1:?package_miscellaneous.sh requires a log directory as argument 1}"
LOG_FILE="${2:-stage-package_miscellaneous}"
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

printf "📚 Staging miscellaneous files\n"
step "   📖  examples/"    bash -c "mkdir -p ./package/examples && cp -r ./examples/* ./package/examples/"
step "   🗒️  package.json" cp ./package.json ./package/package.json
step "   ⚖️  LICENSE"       cp ./LICENSE ./package/LICENSE
step "   👓  README.md"    cp ./README.md ./package/README.md
