/**
  ******************************************************************************
  * @file    system_stm32f4xx.c
  * @brief   CMSIS Cortex-M4 Device Peripheral Access Layer system source file.
  *
  * This file is derived from the official STM32CubeF4 system template and kept
  * local so the project can be built without STM32CubeIDE.
  ******************************************************************************
  */

#include "stm32f4xx.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE ((uint32_t)8000000U)
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE ((uint32_t)16000000U)
#endif

#define VECT_TAB_OFFSET 0x00U

uint32_t SystemCoreClock = 16000000U;
const uint8_t AHBPrescTable[16U] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U, 0U, 0U, 0U, 0U};
const uint8_t APBPrescTable[8U] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/* 上电后的最小系统初始化保持模板默认行为，主时钟切换在 app.c 中完成。 */
void SystemInit(void) {
  SCB->CPACR |= ((3UL << 10U * 2U) | (3UL << 11U * 2U));

  RCC->CR |= (uint32_t)0x00000001U;
  RCC->CFGR = 0x00000000U;
  RCC->CR &= (uint32_t)0xFEF6FFFFU;
  RCC->PLLCFGR = 0x24003010U;
  RCC->CR &= (uint32_t)0xFFFBFFFFU;
  RCC->CIR = 0x00000000U;

  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
}

/* 供 HAL 和调试输出在需要时回读当前 SystemCoreClock。 */
void SystemCoreClockUpdate(void) {
  uint32_t tmp = 0U;
  uint32_t pllvco = 0U;
  uint32_t pllp = 2U;
  uint32_t pllsource = 0U;
  uint32_t pllm = 2U;

  tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp) {
    case 0x00U:
      SystemCoreClock = HSI_VALUE;
      break;
    case 0x04U:
      SystemCoreClock = HSE_VALUE;
      break;
    case 0x08U:
      pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22U;
      pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;

      if (pllsource != 0U) {
        pllvco = (HSE_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U);
      } else {
        pllvco = (HSI_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U);
      }

      pllp = ((((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 16U) + 1U) * 2U);
      SystemCoreClock = pllvco / pllp;
      break;
    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }

  tmp = AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> 4U];
  SystemCoreClock >>= tmp;
}
