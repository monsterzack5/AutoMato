
# Setup.sh
# For CanRed: Build
# For NodeRed: Install Dependencies

# TODO: This is a simple script for development.
#       thats not intended for use in production.


STARTING_DIR=$(pwd)

mkdir /home/pi/.nodered
mkdir /home/pi/.automato

# Build CanRed

cd $STARTING_DIR/../CanRed
rm -rf ./Build
./make.sh --release

if [[ $? -ne 0 ]]
then
    echo "CanRed Failed to Build!"
    exit 1
fi

# Setup CanRed

# copy over .env.example if .env does not exist
if [ ! -f "./.env" ] 
then
    echo "CanRed .env file not found, creating one!"
    cp .env.example .env
fi

# CanRed Done

# NodeRed Setup

# Install Dependencies
cd $STARTING_DIR/../NodeRed
npm run fullInstall

if [[ $? -ne 0 ]]
then
    echo "Failed to install depdencies for NodeRed!"
    exit 1
fi

# copy over .env.example if .env does not exist
if [[ ! -f "./.env" ]]
then
    echo "NodeRed .env file not found, creating one!"
    cp .env.example .env
    # Remove the # comment in the .env file (by deleting the whole line)
    sed -i -e "/^credentialSecret/d"
fi

# NodeRed Done
