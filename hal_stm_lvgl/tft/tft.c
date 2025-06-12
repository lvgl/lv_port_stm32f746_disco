/**
 * @file tft.c
 *
 */

 /*********************
 *      INCLUDES
 *********************/

#include "tft.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st_ltdc/lv_st_ltdc.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

// SDRAM MT48LC4M32B2B5-7 contains 128 MBits memory (64 Mbits accessible) mapped
// to address 0xC0000000. SDRAM organized into 4 banks with 32 Mbits in each bank.
#define SDRAM_BANK1_ADDR ((uint32_t)0xC0000000U)
#define SDRAM_BANK2_ADDR ((uint32_t)0xC0400000U)

#define LTDC_LAYER_IDX   0

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_stm32f746_display_init (void)
{
	/* display initialization */

#if !(LV_COLOR_DEPTH == 16 || LV_COLOR_DEPTH == 24 || LV_COLOR_DEPTH == 32)
   #error LV_COLOR_DEPTH not supported
#endif
  
   // Largest framebuffer possible at 32-bit depth would be (width * height * 4).
   // For our display that is (480 * 272 * 4) which fits within single SDRAM bank.
   // Use double buffering with framebuffers in separate SDRAM banks to avoid contention.
   void* framebuffer_1 = (void*)SDRAM_BANK1_ADDR;
   void* framebuffer_2 = (void*)SDRAM_BANK2_ADDR;

   lv_st_ltdc_create_direct(framebuffer_1, framebuffer_2, LTDC_LAYER_IDX);

}

/**********************
 *   STATIC FUNCTIONS
 **********************/