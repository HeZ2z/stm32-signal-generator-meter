#include "signal_gen/signal_gen_dac.h"

#include <math.h>
#include <string.h>

#define _APP_PI_ 3.14159265358979f

#include "board/board_config.h"
#include "main.h"
#include "signal_gen/signal_gen_dac_logic.h"

const char *signal_gen_dac_waveform_short_name(dac_waveform_t waveform) {
  switch (waveform) {
    case APP_DAC_WAVE_TRIANGLE:
      return "TRI";
    case APP_DAC_WAVE_SINE:
      return "SIN";
    case APP_DAC_WAVE_SQUARE:
      return "SQR";
    default:
      return "UNK";
  }
}

static DAC_HandleTypeDef dac_handle;
static TIM_HandleTypeDef dac_trigger_timer;
static signal_gen_dac_status_t dac_status;

static uint32_t dual_waveform_table[APP_DAC_WAVE_TABLE_SIZE];

static uint16_t build_single_level(dac_waveform_t waveform, uint32_t idx) {
  if (waveform == APP_DAC_WAVE_TRIANGLE) {
    if (idx < (APP_DAC_WAVE_TABLE_SIZE / 2U)) {
      return (uint16_t)((4095U * idx) / ((APP_DAC_WAVE_TABLE_SIZE / 2U) - 1U));
    } else {
      uint32_t down = idx - (APP_DAC_WAVE_TABLE_SIZE / 2U);
      return (uint16_t)(4095U -
                         ((4095U * down) / ((APP_DAC_WAVE_TABLE_SIZE / 2U) - 1U)));
    }
  }
  if (waveform == APP_DAC_WAVE_SINE) {
    return (uint16_t)(2047.5f + 2047.0f *
      sinf((float)idx * (2.0f * _APP_PI_ / (float)APP_DAC_WAVE_TABLE_SIZE)));
  }
  return idx < (APP_DAC_WAVE_TABLE_SIZE / 2U) ? 0U : 4095U;
}

static uint32_t wrap_wave_table_index(uint32_t idx) {
  return idx % APP_DAC_WAVE_TABLE_SIZE;
}

static void rebuild_waveform_buffer(dac_waveform_t waveform,
                                    float ch_b_phase_offset_rad,
                                    uint8_t ch_b_frequency_ratio) {
  uint8_t ratio = (ch_b_frequency_ratio == 0U) ? 1U : ch_b_frequency_ratio;
  uint32_t ch_b_idx =
      (uint32_t)((ch_b_phase_offset_rad / (2.0f * _APP_PI_)) *
                 (float)APP_DAC_WAVE_TABLE_SIZE);
  ch_b_idx = wrap_wave_table_index(ch_b_idx);

  for (uint32_t i = 0U; i < APP_DAC_WAVE_TABLE_SIZE; ++i) {
    uint16_t ch_a_level = build_single_level(waveform, i);
    uint16_t ch_b_level = build_single_level(waveform, ch_b_idx);

    dual_waveform_table[i] = signal_gen_dac_pack_dual_12b(ch_a_level, ch_b_level);

    ch_b_idx = wrap_wave_table_index(ch_b_idx + ratio);
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

static bool start_dual_dma(void) {
  uint32_t target = (uint32_t)&dac_handle.Instance->DHR12RD;

  if (dac_handle.DMA_Handle1 == NULL) {
    return false;
  }

  dac_handle.DMA_Handle1->XferCpltCallback = NULL;
  dac_handle.DMA_Handle1->XferHalfCpltCallback = NULL;
  dac_handle.DMA_Handle1->XferErrorCallback = NULL;

  if (HAL_DMA_Start(dac_handle.DMA_Handle1,
                     (uint32_t)dual_waveform_table, target,
                     APP_DAC_WAVE_TABLE_SIZE) != HAL_OK) {
    return false;
  }

  SET_BIT(dac_handle.Instance->CR, DAC_CR_DMAEN1);

  if (HAL_TIM_Base_Start(&dac_trigger_timer) != HAL_OK) {
    CLEAR_BIT(dac_handle.Instance->CR, DAC_CR_DMAEN1);
    (void)HAL_DMA_Abort(dac_handle.DMA_Handle1);
    return false;
  }

  return true;
}

static bool stop_dual_dma(void) {
  if (dac_handle.DMA_Handle1 == NULL) {
    return true;
  }

  __HAL_TIM_DISABLE(&dac_trigger_timer);

  if ((dac_handle.Instance->CR & DAC_CR_DMAEN1) != 0U) {
    CLEAR_BIT(dac_handle.Instance->CR, DAC_CR_DMAEN1);
    for (uint32_t spin = 0U; spin < 1000U; ++spin) {
      if ((dac_handle.Instance->CR & DAC_CR_DMAEN1) == 0U) {
        break;
      }
    }
    if ((dac_handle.Instance->CR & DAC_CR_DMAEN1) != 0U) {
      return false;
    }
  }

  if (dac_handle.DMA_Handle1->State == HAL_DMA_STATE_BUSY) {
    HAL_StatusTypeDef abort_result = HAL_DMA_Abort(dac_handle.DMA_Handle1);
    if (abort_result != HAL_OK) {
      return false;
    }
  }

  return (dac_handle.Instance->CR & DAC_CR_DMAEN1) == 0U &&
         dac_handle.DMA_Handle1->State != HAL_DMA_STATE_BUSY;
}

void signal_gen_dac_init(void) {
  DAC_ChannelConfTypeDef channel_config = {0};

  (void)memset(&dac_status, 0, sizeof(dac_status));

  dac_handle.Instance = APP_DAC_INSTANCE;
  if (HAL_DAC_Init(&dac_handle) != HAL_OK) {
    Error_Handler();
  }

  rebuild_waveform_buffer(APP_DAC_WAVE_SQUARE, 0.0f, 0U);

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

  if (!stop_dual_dma()) {
    dac_status.active = false;
    return false;
  }

  rebuild_waveform_buffer(config->waveform,
                          config->ch_b_phase_offset_rad,
                          config->ch_b_frequency_ratio);
  configure_tim6(config->waveform, config->frequency_hz);

  if (!start_dual_dma()) {
    dac_status.active = false;
    return false;
  }

  dac_status.active = true;
  dac_status.frequency_hz = config->frequency_hz;
  dac_status.waveform = config->waveform;
  dac_status.ch_b_phase_offset_rad = config->ch_b_phase_offset_rad;
  dac_status.ch_b_frequency_ratio = config->ch_b_frequency_ratio;
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
