#!/bin/bash

# Remember to use this script as: ./flash-stlink2-1-STM32F7.sh FIRMWARE.elf
declare -a ARRAY
for arg; do
   ARRAY+=("$arg")
done

openocd -f stm32f7.cfg  \
        -c "flash write_image erase ${ARRAY[0]}" \
        -c "verify_image ${ARRAY[0]}" \
        -c "reset run" -c shutdown
