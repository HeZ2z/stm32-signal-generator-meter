#include "signal_gen/signal_gen_dac.h"

#include <string.h>

#include "board/board_config.h"
#include "main.h"
#include "signal_gen/signal_gen_dac_logic.h"

const char *signal_gen_dac_waveform_short_name(dac_waveform_t waveform) {
  switch (waveform) {
    case APP_DAC_WAVE_TRIANGLE:
      return "TRI";
    case APP_DAC_WAVE_SQUARE:
      return "SQR";
    case APP_DAC_WAVE_SINE:
    default:
      return "UNK";
  }
}

static DAC_HandleTypeDef dac_handle;
static TIM_HandleTypeDef dac_trigger_timer;
static signal_gen_dac_status_t dac_status;

static uint32_t dual_waveform_table[APP_DAC_WAVE_TABLE_SIZE];

static void rebuild_waveform_buffer(dac_waveform_t waveform) {
  for (uint32_t i = 0U; i < APP_DAC_WAVE_TABLE_SIZE; ++i) {
    uint16_t level = 0U;

    if (waveform == APP_DAC_WAVE_TRIANGLE) {
      if (i < (APP_DAC_WAVE_TABLE_SIZE / 2U)) {
        level = (uint16_t)((4095U * i) / ((APP_DAC_WAVE_TABLE_SIZE / 2U) - 1U));
      } else {
        uint32_t down = i - (APP_DAC_WAVE_TABLE_SIZE / 2U);
        level = (uint16_t)(4095U -
                           ((4095U * down) /
                            ((APP_DAC_WAVE_TABLE_SIZE / 2U) - 1U)));
      }
    } else {
      level = i < (APP_DAC_WAVE_TABLE_SIZE / 2U) ? 0U : 4095U;
    }

    dual_waveform_table[i] = signal_gen_dac_pack_dual_12b(level, level);
  }
}

static void configure_tim6(dac_waveform_t waveform, uint32_t output_frequency_hz) {
  TIM_MasterConfigTypeDef master_config = {0};
  uint32_t timer_clock = tim_apb1_clock_hz();
  signal_gen_dac_tim6_config_t tim6_config = {0};

  if (!signal_gen_dac_compute_tim6_config(timer_clock, waveform,
                                          output_frequency_hz, &tim6_config)) {
    Error_Handler();
  }

  dac_trigger_timer.Instance = APP_DAC_TRIGGER_TIM_INSTANCE;
  dac_trigger_timer.Init.Prescaler = tim6_config.prescaler;
  dac_trigger_timer.Init.Period = tim6_config.period;
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

  if (HAL_DMA_Start(dac_handle.DMA_Handle1, (uint32_t)dual_waveform_table, target,
                    APP_DAC_WAVE_TABLE_SIZE) != HAL_OK) {
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

  rebuild_waveform_buffer(APP_DAC_WAVE_SQUARE);

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

  rebuild_waveform_buffer(config->waveform);
  configure_tim6(config->waveform, config->frequency_hz);
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
