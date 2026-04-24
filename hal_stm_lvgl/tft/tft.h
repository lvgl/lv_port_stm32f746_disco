/**
 * @file tft.h
 *
 */

#ifndef __LV_STM32F746_DISPLAY_H
#define __LV_STM32F746_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/

#define STM32F746_DISPLAY_WIDTH  480
#define STM32F746_DISPLAY_HEIGHT 272

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_stm32f746_display_init (void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* __LV_STM32F746_DISPLAY_H */
