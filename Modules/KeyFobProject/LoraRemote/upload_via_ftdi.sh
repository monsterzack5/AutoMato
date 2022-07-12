if [ -z $1  ]
then
    echo "Usage: ./upload_to_stm32_ftdi_f2232h.sh <source file>"
    exit 1
fi

# ./pio/build/example/firmware.bin -> ./pio/build/example/firmware.elf
FIRMWARE_FILE=$(echo $1 | sed "s/.bin/.elf/g")

/usr/bin/openocd -d2 -f openocd_ftdi_f2232h.cfg -f target/stm32wlx.cfg -c "init; reset halt; flash write_image erase $FIRMWARE_FILE; flash verify_image $FIRMWARE_FILE; reset; shutdown"
