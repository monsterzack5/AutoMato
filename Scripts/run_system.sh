# Run CanRed and Node-Red
# https://spin.atomicobject.com/2017/08/24/start-stop-bash-background-process/
STARTING_DIR=$(pwd)

trap "kill 0" EXIT
trap "exit" INT TERM ERR

cd $STARTING_DIR/../CanRed/
./build/CanRed &

cd $STARTING_DIR/../NodeRed
npm run prod &

wait