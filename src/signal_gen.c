#include "signal_gen.h"

#include "main.h"

static TIM_HandleTypeDef pwm_timer;
static signal_gen_config_t current_config;

static uint32_t timer_clock_hz(void) {
  uint32_t pclk = HAL_RCC_GetPCLK1Freq();
  uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1);

  if (ppre == RCC_HCLK_DIV1) {
    return pclk;
  }

  return pclk * 2U;
}

static void pwm_gpio_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio_init.Pin = APP_PWM_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = APP_PWM_GPIO_AF;
  HAL_GPIO_Init(APP_PWM_GPIO_PORT, &gpio_init);
}

void signal_gen_init(void) {
  __HAL_RCC_TIM4_CLK_ENABLE();
  pwm_gpio_init();

  pwm_timer.Instance = APP_PWM_TIM_INSTANCE;
  pwm_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  pwm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  pwm_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
}

bool signal_gen_apply(const signal_gen_config_t *config) {
  TIM_OC_InitTypeDef pwm_config = {0};
  uint32_t target_ticks = APP_TIMER_TICK_HZ / config->frequency_hz;
  uint32_t timer_clock = timer_clock_hz();
  uint32_t prescaler = (timer_clock / APP_TIMER_TICK_HZ) - 1U;
  uint32_t pulse = (target_ticks * config->duty_percent) / 100U;

  if (config->frequency_hz < APP_PWM_MIN_FREQ_HZ ||
      config->frequency_hz > APP_PWM_MAX_FREQ_HZ ||
      config->duty_percent < APP_PWM_MIN_DUTY_PERCENT ||
      config->duty_percent > APP_PWM_MAX_DUTY_PERCENT ||
      target_ticks < 2U ||
      target_ticks > 65535U ||
      timer_clock < APP_TIMER_TICK_HZ) {
    return false;
  }

  pwm_timer.Init.Prescaler = prescaler;
  pwm_timer.Init.Period = target_ticks - 1U;

  if (HAL_TIM_PWM_Init(&pwm_timer) != HAL_OK) {
    return false;
  }

  pwm_config.OCMode = TIM_OCMODE_PWM1;
  pwm_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  pwm_config.OCFastMode = TIM_OCFAST_DISABLE;
  pwm_config.Pulse = pulse;

  if (HAL_TIM_PWM_ConfigChannel(&pwm_timer, &pwm_config, APP_PWM_CHANNEL) != HAL_OK) {
    return false;
  }

  if (HAL_TIM_PWM_Start(&pwm_timer, APP_PWM_CHANNEL) != HAL_OK) {
    return false;
  }

  current_config = *config;
  return true;
}

const signal_gen_config_t *signal_gen_current(void) {
  return &current_config;
}
