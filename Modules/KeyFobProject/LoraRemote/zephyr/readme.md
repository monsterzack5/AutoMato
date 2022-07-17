PlatformIO is currently limited to Zephyr version 2.7, which is out of date for quite a bit of things, including the power states configuration for the lora_e5_dev_board.

I could have just added that into my overlay file, but where's the fun in that?

Inside of ./boards/arm/lora_e5_dev_board/ is a copy of the same location in the Zephyr repo at commit: `106f82013bfb153e82c5d2f537cf568c5b2ad13f`

Github link: https://github.com/zephyrproject-rtos/zephyr/commit/106f82013bfb153e82c5d2f537cf568c5b2ad13f

This is the newest version before they modified all of the stm32 boards to use PINCTRL, which is not available in Zephyr version 2.7.