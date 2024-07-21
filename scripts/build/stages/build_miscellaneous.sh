# build_ts.sh

if [ "$#" -eq 0 ]; then
  echo "copy_package_json.sh requires a log directory location as argument 1"
fi

LOG_FILENAME=${2:-"action-copy_package_json"}

{
  printf "📚 Adding miscellaneous files\n"
  {
    printf "   📖️ Copying examples folder" &&
      echo "++++++[mkdir -p ./package/examples]++++++" &>>"$1/$LOG_FILENAME.log" &&
      mkdir -p ./package/examples &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
      echo "++++++[cp -r ./examples/* ./package/examples/]++++++" &>>"$1/$LOG_FILENAME.log" &&
      cp -r ./examples/* ./package/examples/ &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
    exit 1
  }
  {
    printf "   🗒️ Copying package.json" &&
      echo "++++++[cp ./package.json ./package/]++++++" &>>"$1/$LOG_FILENAME.log" &&
      cp ./package.json ./package/package.json &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
    exit 1
  }
  {
    printf "   ⚖️ Copying LICENSE file" &&
      echo "++++++[cp ./LICENSE ./package/LICENSE]++++++" &>>"$1/$LOG_FILENAME.log" &&
      cp ./LICENSE ./package/LICENSE &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
    exit 1
  }
  {
    printf "   👓 Copying README file" &&
      echo "++++++[cp ./README.md ./package/README.md]++++++" &>>"$1/$LOG_FILENAME.log" &&
      cp ./README.md ./package/README.md &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
    exit 1
  }

} || {
  printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
  exit 1
}
