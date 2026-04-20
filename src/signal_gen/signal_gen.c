#include "signal_gen/signal_gen.h"

#include "main.h"
#include "signal_gen/signal_gen_logic.h"

static TIM_HandleTypeDef pwm_timer;
static signal_gen_config_t current_config;

/* TIM4 挂在 APB1 上，共享时钟函数由 board_config.h 提供。 */

/* 初始化 PWM 输出引脚复用。 */
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

/* 准备 TIM4 的基础工作模式，真正的频率参数在 apply 阶段设置。 */
void signal_gen_init(void) {
  __HAL_RCC_TIM4_CLK_ENABLE();
  pwm_gpio_init();

  pwm_timer.Instance = APP_PWM_TIM_INSTANCE;
  pwm_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  pwm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  pwm_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
}

/* 计算并应用新的 PWM 输出参数。 */
bool signal_gen_apply(const signal_gen_config_t *config) {
  TIM_OC_InitTypeDef pwm_config = {0};
  uint32_t timer_clock = tim_apb1_clock_hz();
  uint32_t prescaler = 0U;
  uint32_t period = 0U;
  uint32_t pulse = 0U;

  if (!signal_gen_compute_params(config, timer_clock, &prescaler, &period, &pulse)) {
    return false;
  }

  /* 每次配置变更都重新初始化 TIM 和通道，确保 ARR/CCR 同步到硬件。 */
  pwm_timer.Init.Prescaler = prescaler;
  pwm_timer.Init.Period = period;

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

/* 返回当前已经成功下发到硬件的配置。 */
const signal_gen_config_t *signal_gen_current(void) {
  return &current_config;
}
