# build_cpp.sh

if [ "$#" -eq 0 ]; then
  echo "build_cpp.sh requires a log directory location as argument 1"
fi

LOG_FILENAME=${2:-"stage-build_cpp"}

{
  printf "🏗️ Building C/C++ addon\n"
  echo "++++++[node-gyp build]++++++" &>>"$1/$LOG_FILENAME.log"
  node-gyp build &>>"$1/$LOG_FILENAME.log" &&
    printf "   ➡️ Copying 'Release' binaries\n" &&
    {
      printf "      ➡️  ️Copying 'x86_64-linux-gnu' binary" &&
        echo "++++++[mkdir -p ./package/bin/x86_64-linux-gnu]++++++" &>>"$1/$LOG_FILENAME.log" &&
        mkdir -p ./package/bin/x86_64-linux-gnu &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
        echo "++++++[cp -r build/Release/node-lxc.node ./package/bin/x86_64-linux-gnu/node-lxc.node]++++++" &>>"$1/$LOG_FILENAME.log" &&
        cp -r build/Release/node-lxc.node ./package/bin/x86_64-linux-gnu/node-lxc.node &>>"$1/$LOG_FILENAME.log" && echo "done" &>>"$1/$LOG_FILENAME.log" &&
        printf " 🟢\n"
    } ||
    {
      printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
      exit 1
    }
} || {
  printf " 🔴\n SEE: $1/$LOG_FILENAME.log"
  exit 1
}
