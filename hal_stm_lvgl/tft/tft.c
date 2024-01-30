/**
 * @file tft.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_conf.h"
#include "lvgl/lvgl.h"
#include <string.h>
#include <stdlib.h>

#include "tft.h"
#include "stm32f7xx.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_ts.h"
#include "../Components/rk043fn48h/rk043fn48h.h"

/*********************
 *      DEFINES
 *********************/

#if LV_COLOR_DEPTH != 16 && LV_COLOR_DEPTH != 24 && LV_COLOR_DEPTH != 32
#error LV_COLOR_DEPTH must be 16, 24, or 32
#endif

/**
  * @brief  LCD status structure definition
  */
#define LCD_OK                          ((uint8_t)0x00)
#define LCD_ERROR                       ((uint8_t)0x01)
#define LCD_TIMEOUT                     ((uint8_t)0x02)

/**
  * @brief LCD special pins
  */
/* Display enable pin */
#define LCD_DISP_PIN                    GPIO_PIN_12
#define LCD_DISP_GPIO_PORT              GPIOI
#define LCD_DISP_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOI_CLK_ENABLE()
#define LCD_DISP_GPIO_CLK_DISABLE()     __HAL_RCC_GPIOI_CLK_DISABLE()

/* Backlight control pin */
#define LCD_BL_CTRL_PIN                  GPIO_PIN_3
#define LCD_BL_CTRL_GPIO_PORT            GPIOK
#define LCD_BL_CTRL_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOK_CLK_ENABLE()
#define LCD_BL_CTRL_GPIO_CLK_DISABLE()   __HAL_RCC_GPIOK_CLK_DISABLE()

#define CPY_BUF_DMA_STREAM               DMA2_Stream0
#define CPY_BUF_DMA_CHANNEL              DMA_CHANNEL_0
#define CPY_BUF_DMA_STREAM_IRQ           DMA2_Stream0_IRQn
#define CPY_BUF_DMA_STREAM_IRQHANDLER    DMA2_Stream0_IRQHandler

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/*These 3 functions are needed by LittlevGL*/
static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static uint8_t LCD_Init(void);
static void LCD_LayerRgb565Init(uint32_t FB_Address);
static void LCD_DisplayOn(void);

static void DMA_Config(void);
static void DMA_TransferComplete(DMA_HandleTypeDef *han);
static void DMA_TransferError(DMA_HandleTypeDef *han);

/**********************
 *  STATIC VARIABLES
 **********************/
static LTDC_HandleTypeDef  hLtdcHandler;
static lv_display_t * display;

#if LV_COLOR_DEPTH == 16
typedef uint16_t uintpixel_t;
#elif LV_COLOR_DEPTH == 24 || LV_COLOR_DEPTH == 32
typedef uint32_t uintpixel_t;
#endif

/* You can try to change buffer to internal ram by uncommenting line below and commenting
 * SDRAM one. */
//static uintpixel_t my_fb[TFT_HOR_RES * TFT_VER_RES];

static __IO uintpixel_t * my_fb = (__IO uintpixel_t*) (0x60000000);

static DMA_HandleTypeDef  DmaHandle;
static int32_t            x1_flush;
static int32_t            y1_flush;
static int32_t            x2_flush;
static int32_t            y2_fill;
static int32_t            y_fill_act;
static const uint16_t * buf_to_flush;

static lv_disp_t *our_disp = NULL;
/**********************
 *      MACROS
 **********************/

/**
 * Initialize your display here
 */

void tft_init(void)
{
	/* There is only one display on STM32 */
	if(our_disp != NULL)
		abort();
    /* LCD Initialization */
    LCD_Init();

    /* LCD Initialization */
    LCD_LayerRgb565Init((uint32_t)my_fb);

    /* Enable the LCD */
    LCD_DisplayOn();

    DMA_Config();


	static uint16_t buf1[TFT_HOR_RES * 68];
	static uint16_t buf2[TFT_HOR_RES * 68];
    display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_cb);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void flush_cb(lv_display_t * disp, const lv_area_t *area, uint8_t * px_map)
{
	int32_t x1 = area->x1;
	int32_t x2 = area->x2;
	int32_t y1 = area->y1;
	int32_t y2 = area->y2;
    /*Return if the area is out the screen*/

    if(x2 < 0) return;
    if(y2 < 0) return;
    if(x1 > TFT_HOR_RES - 1) return;
    if(y1 > TFT_VER_RES - 1) return;

    /*Truncate the area to the screen*/
    int32_t act_x1 = x1 < 0 ? 0 : x1;
    int32_t act_y1 = y1 < 0 ? 0 : y1;
    int32_t act_x2 = x2 > TFT_HOR_RES - 1 ? TFT_HOR_RES - 1 : x2;
    int32_t act_y2 = y2 > TFT_VER_RES - 1 ? TFT_VER_RES - 1 : y2;

    x1_flush = act_x1;
    y1_flush = act_y1;
    x2_flush = act_x2;
    y2_fill = act_y2;
    y_fill_act = act_y1;
    buf_to_flush = (uint16_t *)px_map;

	SCB_CleanInvalidateDCache();
	SCB_InvalidateICache();

    /*##-7- Start the DMA transfer using the interrupt mode #*/
    /* Configure the source, destination and buffer size DMA fields and Start DMA Stream transfer */
    /* Enable All the DMA interrupts */
    HAL_StatusTypeDef err;
    uint32_t length = (x2_flush - x1_flush + 1);
#if LV_COLOR_DEPTH == 24 || LV_COLOR_DEPTH == 32
    length *= 2; /* STM32 DMA uses 16-bit chunks so multiply by 2 for 32-bit color */
#endif
    err = HAL_DMA_Start_IT(&DmaHandle,(uint32_t)buf_to_flush, (uint32_t)&my_fb[y_fill_act * TFT_HOR_RES + x1_flush],
             length);
    if(err != HAL_OK)
    {
        while(1);	/*Halt on error*/
    }
}


/**
 * @brief Configure LCD pins, and peripheral clocks.
 */
static void LCD_MspInit(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    /* Enable the LTDC and DMA2D clocks */
    __HAL_RCC_LTDC_CLK_ENABLE();
    __HAL_RCC_DMA2D_CLK_ENABLE();
    /* Enable GPIOs clock */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    LCD_DISP_GPIO_CLK_ENABLE();
    LCD_BL_CTRL_GPIO_CLK_ENABLE();

    /*** LTDC Pins configuration ***/
    /* GPIOE configuration */
    gpio_init_structure.Pin       = GPIO_PIN_4;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Pull      = GPIO_NOPULL;
    gpio_init_structure.Speed     = GPIO_SPEED_FAST;
    gpio_init_structure.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOE, &gpio_init_structure);

    /* GPIOG configuration */
    gpio_init_structure.Pin       = GPIO_PIN_12;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Alternate = GPIO_AF9_LTDC;
    HAL_GPIO_Init(GPIOG, &gpio_init_structure);

    /* GPIOI LTDC alternate configuration */
    gpio_init_structure.Pin       = GPIO_PIN_9 | GPIO_PIN_10 | \
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOI, &gpio_init_structure);

    /* GPIOJ configuration */
    gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
            GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | \
            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | \
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOJ, &gpio_init_structure);

    /* GPIOK configuration */
    gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | \
            GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOK, &gpio_init_structure);

    /* LCD_DISP GPIO configuration */
    gpio_init_structure.Pin       = LCD_DISP_PIN;     /* LCD_DISP pin has to be manually controlled */
    gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(LCD_DISP_GPIO_PORT, &gpio_init_structure);

    /* LCD_BL_CTRL GPIO configuration */
    gpio_init_structure.Pin       = LCD_BL_CTRL_PIN;  /* LCD_BL_CTRL pin has to be manually controlled */
    gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(LCD_BL_CTRL_GPIO_PORT, &gpio_init_structure);
}

/**
 * @brief Configure LTDC PLL.
 */
static void LCD_ClockConfig(void)
{
    static RCC_PeriphCLKInitTypeDef  periph_clk_init_struct;

    /* RK043FN48H LCD clock configuration */
    /* PLLSAI_VCO Input = HSE_VALUE/PLL_M = 1 Mhz */
    /* PLLSAI_VCO Output = PLLSAI_VCO Input * PLLSAIN = 192 Mhz */
    /* PLLLCDCLK = PLLSAI_VCO Output/PLLSAIR = 192/5 = 38.4 Mhz */
    /* LTDC clock frequency = PLLLCDCLK / LTDC_PLLSAI_DIVR_4 = 38.4/4 = 9.6Mhz */
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periph_clk_init_struct.PLLSAI.PLLSAIN = 192;
    periph_clk_init_struct.PLLSAI.PLLSAIR = RK043FN48H_FREQUENCY_DIVIDER;
    periph_clk_init_struct.PLLSAIDivR = RCC_PLLSAIDIVR_4;
    HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct);
}

/**
  * @brief  Initializes the LCD.
  * @retval LCD state
  */
static uint8_t LCD_Init(void)
{
    /* Select the used LCD */

    /* The RK043FN48H LCD 480x272 is selected */
    /* Timing Configuration */
    hLtdcHandler.Init.HorizontalSync = (RK043FN48H_HSYNC - 1);
    hLtdcHandler.Init.VerticalSync = (RK043FN48H_VSYNC - 1);
    hLtdcHandler.Init.AccumulatedHBP = (RK043FN48H_HSYNC + RK043FN48H_HBP - 1);
    hLtdcHandler.Init.AccumulatedVBP = (RK043FN48H_VSYNC + RK043FN48H_VBP - 1);
    hLtdcHandler.Init.AccumulatedActiveH = (RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP - 1);
    hLtdcHandler.Init.AccumulatedActiveW = (RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP - 1);
    hLtdcHandler.Init.TotalHeigh = (RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP + RK043FN48H_VFP - 1);
    hLtdcHandler.Init.TotalWidth = (RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP + RK043FN48H_HFP - 1);

    /* LCD clock configuration */
    LCD_ClockConfig();

    /* Initialize the LCD pixel width and pixel height */
    hLtdcHandler.LayerCfg->ImageWidth  = RK043FN48H_WIDTH;
    hLtdcHandler.LayerCfg->ImageHeight = RK043FN48H_HEIGHT;

    /* Background value */
    hLtdcHandler.Init.Backcolor.Blue = 0;
    hLtdcHandler.Init.Backcolor.Green = 0;
    hLtdcHandler.Init.Backcolor.Red = 0;

    /* Polarity */
    hLtdcHandler.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    hLtdcHandler.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    hLtdcHandler.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    hLtdcHandler.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
    hLtdcHandler.Instance = LTDC;

    if(HAL_LTDC_GetState(&hLtdcHandler) == HAL_LTDC_STATE_RESET)
    {
        /* Initialize the LCD Msp: this __weak function can be rewritten by the application */
        LCD_MspInit();
    }
    HAL_LTDC_Init(&hLtdcHandler);

    /* Assert display enable LCD_DISP pin */
    HAL_GPIO_WritePin(LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);

    /* Assert backlight LCD_BL_CTRL pin */
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);

    BSP_SDRAM_Init();
    HAL_EnableFMCMemorySwapping();

    uint32_t i;
    for(i = 0; i < (TFT_HOR_RES * TFT_VER_RES) ; i++)
    {
        my_fb[i] = 0;
    }

    return LCD_OK;
}

static void LCD_LayerRgb565Init(uint32_t FB_Address)
{
    LTDC_LayerCfgTypeDef  layer_cfg;

    /* Layer Init */
    layer_cfg.WindowX0 = 0;
    layer_cfg.WindowX1 = TFT_HOR_RES;
    layer_cfg.WindowY0 = 0;
    layer_cfg.WindowY1 = TFT_VER_RES;

#if LV_COLOR_DEPTH == 16
    layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
#elif LV_COLOR_DEPTH == 24 || LV_COLOR_DEPTH == 32
    layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
#else
#error Unsupported color depth (see tft.c)
#endif
    layer_cfg.FBStartAdress = FB_Address;
    layer_cfg.Alpha = 255;
    layer_cfg.Alpha0 = 0;
    layer_cfg.Backcolor.Blue = 0;
    layer_cfg.Backcolor.Green = 0;
    layer_cfg.Backcolor.Red = 0;
    layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    layer_cfg.ImageWidth = TFT_HOR_RES;
    layer_cfg.ImageHeight = TFT_VER_RES;

    HAL_LTDC_ConfigLayer(&hLtdcHandler, &layer_cfg, 0);
}

static void LCD_DisplayOn(void)
{
    /* Display On */
    __HAL_LTDC_ENABLE(&hLtdcHandler);
    HAL_GPIO_WritePin(LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);        /* Assert LCD_DISP pin */
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);  /* Assert LCD_BL_CTRL pin */
}

static void DMA_Config(void)
{
    /*## -1- Enable DMA2 clock #################################################*/
    __HAL_RCC_DMA2_CLK_ENABLE();

    /*##-2- Select the DMA functional Parameters ###############################*/
    DmaHandle.Init.Channel = CPY_BUF_DMA_CHANNEL;                   /* DMA_CHANNEL_0                    */
    DmaHandle.Init.Direction = DMA_MEMORY_TO_MEMORY;                /* M2M transfer mode                */
    DmaHandle.Init.PeriphInc = DMA_PINC_ENABLE;                     /* Peripheral increment mode Enable */
    DmaHandle.Init.MemInc = DMA_MINC_ENABLE;                        /* Memory increment mode Enable     */
    DmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;   /* Peripheral data alignment : 16bit */
    DmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;      /* memory data alignment : 16bit     */
    DmaHandle.Init.Mode = DMA_NORMAL;                               /* Normal DMA mode                  */
    DmaHandle.Init.Priority = DMA_PRIORITY_HIGH;                    /* priority level : high            */
    DmaHandle.Init.FIFOMode = DMA_FIFOMODE_ENABLE;                  /* FIFO mode enabled                */
    DmaHandle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL; /* FIFO threshold: 1/4 full   */
    DmaHandle.Init.MemBurst = DMA_MBURST_SINGLE;                    /* Memory burst                     */
    DmaHandle.Init.PeriphBurst = DMA_PBURST_SINGLE;                 /* Peripheral burst                 */

    /*##-3- Select the DMA instance to be used for the transfer : DMA2_Stream0 #*/
    DmaHandle.Instance = CPY_BUF_DMA_STREAM;

    /*##-4- Initialize the DMA stream ##########################################*/
    if(HAL_DMA_Init(&DmaHandle) != HAL_OK)
    {
        while(1)
        {
        }
    }

    /*##-5- Select Callbacks functions called after Transfer complete and Transfer error */
    HAL_DMA_RegisterCallback(&DmaHandle, HAL_DMA_XFER_CPLT_CB_ID, DMA_TransferComplete);
    HAL_DMA_RegisterCallback(&DmaHandle, HAL_DMA_XFER_ERROR_CB_ID, DMA_TransferError);

    /*##-6- Configure NVIC for DMA transfer complete/error interrupts ##########*/
    HAL_NVIC_SetPriority(CPY_BUF_DMA_STREAM_IRQ, 0, 0);
    HAL_NVIC_EnableIRQ(CPY_BUF_DMA_STREAM_IRQ);
}

/**
  * @brief  DMA conversion complete callback
  * @note   This function is executed when the transfer complete interrupt
  *         is generated
  * @retval None
  */
static void DMA_TransferComplete(DMA_HandleTypeDef *han)
{
    y_fill_act ++;

    if(y_fill_act > y2_fill) {
    	SCB_CleanInvalidateDCache();
    	SCB_InvalidateICache();
        lv_disp_flush_ready(display);
    } else {
    	uint32_t length = (x2_flush - x1_flush + 1);
        buf_to_flush += x2_flush - x1_flush + 1;
        /*##-7- Start the DMA transfer using the interrupt mode ####################*/
        /* Configure the source, destination and buffer size DMA fields and Start DMA Stream transfer */
        /* Enable All the DMA interrupts */
#if LV_COLOR_DEPTH == 24 || LV_COLOR_DEPTH == 32
        length *= 2; /* STM32 DMA uses 16-bit chunks so multiply by 2 for 32-bit color */
#endif
        if(HAL_DMA_Start_IT(han,(uint32_t)buf_to_flush, (uint32_t)&my_fb[y_fill_act * TFT_HOR_RES + x1_flush],
                            length) != HAL_OK)
        {
            while(1);	/*Halt on error*/
        }
    }
}

/**
  * @brief  DMA conversion error callback
  * @note   This function is executed when the transfer error interrupt
  *         is generated during DMA transfer
  * @retval None
  */
static void DMA_TransferError(DMA_HandleTypeDef *han)
{

}

/**
  * @brief  This function handles DMA Stream interrupt request.
  * @param  None
  * @retval None
  */
void CPY_BUF_DMA_STREAM_IRQHANDLER(void)
{
    /* Check the interrupt and clear flag */
    HAL_DMA_IRQHandler(&DmaHandle);
}
