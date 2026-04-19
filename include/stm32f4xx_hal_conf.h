#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* 只打开当前项目真实使用到的 HAL 模块，减少无关驱动编译开销。 */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* 当前固件时钟以 HSI 为起点，HSE 常量仅保留模板兼容值。 */
#define HSI_VALUE 16000000U
#define HSE_VALUE 8000000U
#define HSE_STARTUP_TIMEOUT 100U
#define LSE_STARTUP_TIMEOUT 5000U
#define EXTERNAL_CLOCK_VALUE 12288000U
#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY 0x0FU
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE 1U

/* 本项目未使用 HAL 动态回调注册机制。 */
#define USE_HAL_TIM_REGISTER_CALLBACKS 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U

/* 仅包含当前启用模块需要的 HAL 头文件。 */
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rcc_ex.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_dac.h"
#include "stm32f4xx_hal_dac_ex.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_adc_ex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_ltdc.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_pwr_ex.h"
#include "stm32f4xx_hal_sdram.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_tim_ex.h"
#include "stm32f4xx_hal_uart.h"

#include <assert.h>
/* 调试构建下继续保留 HAL 参数断言。 */
#define assert_param(expr) assert(expr)

#ifdef __cplusplus
}
#endif

#endif
