if [ "$#" -eq 0 ]; then
  echo "build_env.sh requires a log directory location as argument 1"
fi

LOG_FILENAME=${2:-"stage-build_env"}

{
  printf "🚧️️ Setting up build environment\n"
  {
    printf "   🎛️ Updating en Upgrading System" &&
      echo "++++++[apt update -y]++++++" &>>"$1/$LOG_FILENAME.log" &&
      apt update -y &>>"$1/$LOG_FILENAME.log" &&
      echo "++++++[apt upgrade -y]++++++" &>>"$1/$LOG_FILENAME.log" &&
      apt upgrade -y &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
  }
  {
    printf "   📚️ Installing g++ and cmake" &&
      echo "++++++[apt install g++ cmake -y]++++++" &>>"$1/$LOG_FILENAME.log" &&
      apt install g++ cmake -y &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
  }
  {
    printf "   🗳️ Installing lxc and lxc-dev (header files)" &&
      echo "++++++[apt install lxc lxc-dev -y]++++++" &>>"$1/$LOG_FILENAME.log" &&
      apt install lxc lxc-dev -y &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
  }
  {
    printf "   🗳️ Installing NodeJS dependencies" &&
      echo "++++++[npm install]++++++" &>>"$1/$LOG_FILENAME.log" &&
      npm install &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
  }
  {
    printf "   🔩️ Configuring node-gyp" &&
      echo "++++++[node-gyp clean && node-gyp configure]++++++" &>>"$1/$LOG_FILENAME.log" &&
      node-gyp clean &>>"$1/$LOG_FILENAME.log" && node-gyp configure &>>"$1/$LOG_FILENAME.log" &&
      printf " 🟢\n"
  } || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
  }
} || {
    printf " 🔴\n SEE: %s\n" "$1/$LOG_FILENAME.log" && exit 1
}
