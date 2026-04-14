#include "main.h"

void HAL_MspInit(void) {
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim) {
  GPIO_InitTypeDef gpio_init = {0};

  if (htim->Instance != APP_MEASURE_TIM_INSTANCE) {
    return;
  }

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio_init.Pin = APP_MEASURE_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = APP_MEASURE_GPIO_AF;
  HAL_GPIO_Init(APP_MEASURE_GPIO_PORT, &gpio_init);

  HAL_NVIC_SetPriority(APP_MEASURE_TIM_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(APP_MEASURE_TIM_IRQn);
}
