# LVGL ported to STM32F746 Discovery

This is LVGL ported to [STM32F746G-DISCO](https://www.st.com/en/evaluation-tools/32f746gdiscovery.html) using 
[CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) or IAR Embedded Workbench.

## Try it with just a few clicks!

1. Download [lv_stm32f746.bin.zip](https://nightly.link/lvgl/lv_port_stm32f746_disco/workflows/stm32_port/master/lv_stm32f746.bin.zip) and extract the binary inside.
2. Plug in the Discovery board.
3. Copy the binary to the `DIS_F746NG` drive provided by the board's USB interface.

![image](https://user-images.githubusercontent.com/42941056/103720909-71ef5400-4f9a-11eb-8d31-0420c5794b52.png)


# How to compile and make changes
1. Clone (or download) this GitHub repository to a folder on your computer and update the submodules:
`git clone --recursive https://github.com/lvgl/lv_port_stm32f746_disco.git`
2. Open/Import the project
  * CubeIDE
    1. Install [CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html).
    2. Import the project into your workspace.
  * IAR Embedded Workbench
    * Open IAR workspace at `ide/iar/stm32f746_disco_lvgl.eww`
3. Connect the Discovery board
4. Build and run!
