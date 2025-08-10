SCRIPT_DIR=$(realpath $(dirname $0))

#region Setup logging
LOG_DIR="$SCRIPT_DIR/../logs/setup/$EPOCH"
mkdir -p $LOG_DIR
printf "💬 You can find the logs in the script directory($LOG_DIR)\n"
#endregion

BUILD_ENV="$(realpath "$SCRIPT_DIR/../build/stages/build_env.sh")"

echo $BUILD_ENV

source $BUILD_ENV $LOG_DIR "setup_env"
