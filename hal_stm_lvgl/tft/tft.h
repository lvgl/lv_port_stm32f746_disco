/**
 * @file tft.h
 *
 */

#ifndef DISP_H
#define DISP_H

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include "src/misc/lv_color.h"
#include "src/misc/lv_area.h"
/*********************
 *      DEFINES
 *********************/
#define TFT_HOR_RES 480
#define TFT_VER_RES 272

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void tft_init(void);

/**********************
 *      MACROS
 **********************/

#endif
