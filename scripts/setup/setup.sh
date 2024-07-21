SCRIPT_DIR=$(realpath $(dirname $0))

#region Setup logging
LOG_DIR="$SCRIPT_DIR/../logs/setup/$EPOCH"
mkdir -p $LOG_DIR
printf "💬 You can find the logs in the script directory($LOG_DIR)\n"
#endregion

source "$SCRIPT_DIR/../build/actions/build_env.sh" $LOG_DIR "setup_env"
