# build_ts.sh

if [ "$#" -eq 0 ]; then
  echo "build_ts.sh requires a log directory location as argument 1"
fi

LOG_FILENAME=${2:-"stage-build_ts"}

{
  printf "🔨 Compiling typescript files"
  tsc --build &>>"$1/$LOG_FILENAME.log" && printf " 🟢\n" &&
    {
      printf "   ➡️ Moving typescript definition to 'package/lib'"
      echo "++++++[mkdir -p ./package/lib]++++++" &>>"$1/$LOG_FILENAME.log" &&
        mkdir -p ./package/lib &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
        echo "++++++[cp -r build/tsc/* ./package/lib]++++++" &>>"$1/$LOG_FILENAME.log" &&
        cp -r build/tsc/* ./package/lib &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log"
      printf " 🟢\n"
    } || {
    printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
    exit 1
  }
} || {
  printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
  exit 1
}
