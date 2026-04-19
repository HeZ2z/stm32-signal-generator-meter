#include "signal_gen/signal_gen_dac.h"

#include <string.h>

#include "board/board_config.h"
#include "main.h"
#include "signal_gen/signal_gen_dac_logic.h"

static DAC_HandleTypeDef dac_handle;
static TIM_HandleTypeDef dac_trigger_timer;
static signal_gen_dac_status_t dac_status;

static uint32_t dual_square_wave[2];

static void configure_tim6(uint32_t output_frequency_hz) {
  TIM_MasterConfigTypeDef master_config = {0};
  uint32_t timer_clock = tim_apb1_clock_hz();
  uint32_t update_hz = 0U;
  uint32_t period_ticks;

  if (!signal_gen_dac_compute_tim6_update_hz(output_frequency_hz, &update_hz)) {
    Error_Handler();
  }

  period_ticks = timer_clock / update_hz;
  if (period_ticks < 2U || period_ticks > 65536U) {
    Error_Handler();
  }

  dac_trigger_timer.Instance = APP_DAC_TRIGGER_TIM_INSTANCE;
  dac_trigger_timer.Init.Prescaler = 0U;
  dac_trigger_timer.Init.Period = period_ticks - 1U;
  dac_trigger_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  dac_trigger_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  dac_trigger_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&dac_trigger_timer) != HAL_OK) {
    Error_Handler();
  }

  master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&dac_trigger_timer, &master_config) !=
      HAL_OK) {
    Error_Handler();
  }
}

static void start_dual_dma(void) {
  uint32_t target = (uint32_t)&dac_handle.Instance->DHR12RD;

  if (dac_handle.DMA_Handle1 == NULL) {
    Error_Handler();
  }

  /* 当前 circular DMA 路径不依赖 half/complete/error 回调。 */
  dac_handle.DMA_Handle1->XferCpltCallback = NULL;
  dac_handle.DMA_Handle1->XferHalfCpltCallback = NULL;
  dac_handle.DMA_Handle1->XferErrorCallback = NULL;

  if (HAL_DMA_Start(dac_handle.DMA_Handle1, (uint32_t)dual_square_wave, target,
                    (uint32_t)(sizeof(dual_square_wave) /
                               sizeof(dual_square_wave[0]))) != HAL_OK) {
    Error_Handler();
  }

  SET_BIT(dac_handle.Instance->CR, DAC_CR_DMAEN1);
  if (HAL_TIM_Base_Start(&dac_trigger_timer) != HAL_OK) {
    Error_Handler();
  }
}

void signal_gen_dac_init(void) {
  DAC_ChannelConfTypeDef channel_config = {0};

  (void)memset(&dac_status, 0, sizeof(dac_status));

  dac_handle.Instance = APP_DAC_INSTANCE;
  if (HAL_DAC_Init(&dac_handle) != HAL_OK) {
    Error_Handler();
  }

  dual_square_wave[0] = signal_gen_dac_pack_dual_12b(0U, 0U);
  dual_square_wave[1] = signal_gen_dac_pack_dual_12b(4095U, 4095U);

  channel_config.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&dac_handle, &channel_config, DAC_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_DAC_ConfigChannel(&dac_handle, &channel_config, DAC_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_DAC_Start(&dac_handle, DAC_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_DAC_Start(&dac_handle, DAC_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
}

bool signal_gen_dac_apply(const signal_gen_dac_config_t *config) {
  if (!signal_gen_dac_config_valid(config)) {
    return false;
  }

  __HAL_TIM_DISABLE(&dac_trigger_timer);
  CLEAR_BIT(dac_handle.Instance->CR, DAC_CR_DMAEN1);
  if (dac_handle.DMA_Handle1 != NULL) {
    (void)HAL_DMA_Abort(dac_handle.DMA_Handle1);
  }

  configure_tim6(config->frequency_hz);
  start_dual_dma();

  dac_status.active = true;
  dac_status.frequency_hz = config->frequency_hz;
  dac_status.waveform = config->waveform;
  return true;
}

const signal_gen_dac_status_t *signal_gen_dac_current(void) {
  return &dac_status;
}

void signal_gen_dac_dma_irq_handler(void) {
  if (dac_handle.DMA_Handle1 != NULL) {
    HAL_DMA_IRQHandler(dac_handle.DMA_Handle1);
  }
}

void signal_gen_dac_poll(uint32_t now_ms) {
  /* TIM6 触发 + circular DMA 模式下无需软件轮询。 */
  (void)now_ms;
}
