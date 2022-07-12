#!/bin/bash

# This is a hacky workaround to a problem that I can not find much info about,
# except, that it was supposidly fixed in 2021? Either way, I found a workaround here:
# https://github.com/platformio/platform-atmelsam/issues/153

# The problem stems from Zephyer and platformio not playing nicely,
# where platformio has the file sx126x.c, and so does Zephyer
# The easist way to fix this, is by just renaming one of the files
# and also, changing it's name in the CMakeLists.txt file. 

# If you wish to revert these changes, simply rm -rf ~/.platformio/packages/framework-zephyr/
# and platformio will redownload it on next build. 

mv ~/.platformio/packages/framework-zephyr/_pio/modules/lib/loramac-node/src/radio/sx126x/sx126x.c ~/.platformio/packages/framework-zephyr/_pio/modules/lib/loramac-node/src/radio/sx126x/radio_sx126x.c

sed -i "s/sx126x.c/radio_sx126x.c/g" ~/.platformio/packages/framework-zephyr/modules/loramac-node/CMakeLists.txt

# You !NEED! to either delete your .pio folder, or to run "">platformio: clean" after running this script,
# or it will not work!
echo "Now, You !NEED! to either delete your local .pio folder, or run \">platformio: clean\" from the command pallet"
echo "If you don't, this fix won't work!"