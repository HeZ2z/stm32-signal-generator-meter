#include "main.h"

/* 某些外设 GPIO 口由 board_config 决定，运行时按端口动态打开 RCC 时钟。 */
static void enable_gpio_clock(GPIO_TypeDef *port) {
  if (port == GPIOA) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  } else if (port == GPIOB) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  } else if (port == GPIOC) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  } else if (port == GPIOD) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  } else if (port == GPIOE) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
  } else if (port == GPIOF) {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  } else if (port == GPIOG) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
  } else if (port == GPIOH) {
    __HAL_RCC_GPIOH_CLK_ENABLE();
  } else if (port == GPIOI) {
    __HAL_RCC_GPIOI_CLK_ENABLE();
  } else if (port == GPIOJ) {
    __HAL_RCC_GPIOJ_CLK_ENABLE();
  } else if (port == GPIOK) {
    __HAL_RCC_GPIOK_CLK_ENABLE();
  }
}

/* 初始化 HAL 公共底层能力。 */
void HAL_MspInit(void) {
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

/* 配置 TIM3 输入捕获所需 GPIO 和中断。 */
void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim) {
  GPIO_InitTypeDef gpio_init = {0};

  if (htim->Instance != APP_MEASURE_TIM_INSTANCE) {
    return;
  }

  __HAL_RCC_TIM3_CLK_ENABLE();
  enable_gpio_clock(APP_MEASURE_GPIO_PORT);

  gpio_init.Pin = APP_MEASURE_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = APP_MEASURE_GPIO_AF;
  HAL_GPIO_Init(APP_MEASURE_GPIO_PORT, &gpio_init);

  HAL_NVIC_SetPriority(APP_MEASURE_TIM_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(APP_MEASURE_TIM_IRQn);
}

/* 按 Apollo F429 RGB565 实际走线配置 LTDC 输出引脚。 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc) {
  GPIO_InitTypeDef gpio_init = {0};

  if (hltdc->Instance != LTDC) {
    return;
  }

  __HAL_RCC_LTDC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();

  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF14_LTDC;

  gpio_init.Pin = GPIO_PIN_10;
  HAL_GPIO_Init(GPIOF, &gpio_init);

  gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_11;
  HAL_GPIO_Init(GPIOG, &gpio_init);

  gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                  GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOH, &gpio_init);

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 |
                  GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOI, &gpio_init);

  HAL_NVIC_SetPriority(LTDC_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(LTDC_IRQn);
}

/* 释放 LTDC 时钟和中断。 */
void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc) {
  if (hltdc->Instance != LTDC) {
    return;
  }

  __HAL_RCC_LTDC_FORCE_RESET();
  __HAL_RCC_LTDC_RELEASE_RESET();
  HAL_NVIC_DisableIRQ(LTDC_IRQn);
}

/* 配置 Apollo 板载 SDRAM 使用的 FMC 管脚。 */
void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram) {
  GPIO_InitTypeDef gpio_init = {0};

  if (hsdram->Instance != FMC_SDRAM_DEVICE) {
    return;
  }

  __HAL_RCC_FMC_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();

  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF12_FMC;

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3;
  HAL_GPIO_Init(GPIOC, &gpio_init);

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                  GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &gpio_init);

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 |
                  GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                  GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &gpio_init);

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 |
                  GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOF, &gpio_init);

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
                  GPIO_PIN_8 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOG, &gpio_init);
}
