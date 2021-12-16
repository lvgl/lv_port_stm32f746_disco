#!/bin/bash

openocd -f Release/stm32f7.cfg  \
        -c "program build/debug/lv_stm32f746.elf verify reset exit"  
