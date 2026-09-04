#include "signal_capture/signal_capture_adc.h"
#include "signal_capture/signal_capture_adc_logic.h"

#include <string.h>

#include "main.h"

static ADC_HandleTypeDef scope_adc;
static TIM_HandleTypeDef scope_trigger_timer;
static uint16_t scope_dma_buffer[APP_SCOPE_DMA_BUFFER_SIZE];
static volatile scope_capture_frame_t latest_frame;

static void configure_trigger_timer(uint32_t update_hz) {
  TIM_MasterConfigTypeDef master_config = {0};
  uint32_t timer_clock = tim_apb1_clock_hz();
  uint32_t period_ticks = timer_clock / update_hz;

  if (period_ticks < 2U || period_ticks > 65536U) {
    Error_Handler();
  }

  scope_trigger_timer.Instance = APP_SCOPE_ADC_TRIGGER_TIM_INSTANCE;
  scope_trigger_timer.Init.Prescaler = 0U;
  scope_trigger_timer.Init.Period = period_ticks - 1U;
  scope_trigger_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  scope_trigger_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  scope_trigger_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&scope_trigger_timer) != HAL_OK) {
    Error_Handler();
  }

  master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&scope_trigger_timer, &master_config) !=
      HAL_OK) {
    Error_Handler();
  }

  if (HAL_TIM_Base_Start(&scope_trigger_timer) != HAL_OK) {
    Error_Handler();
  }
}

static void update_frame_from_offset(uint16_t offset) {
  scope_capture_frame_t next = {0};

  if ((offset + APP_SCOPE_ADC_CHUNK_RAW_COUNT) > APP_SCOPE_DMA_BUFFER_SIZE) {
    return;
  }

  if (!adc_logic_unpack_interleaved_frame(&scope_dma_buffer[offset],
                                          APP_SCOPE_ADC_CHUNK_RAW_COUNT,
                                          HAL_GetTick(), &next)) {
    return;
  }

  latest_frame = next;
}

void signal_capture_adc_init(void) {
  ADC_ChannelConfTypeDef channel = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();
  APP_SCOPE_ADC_TRIGGER_TIM_RCC_ENABLE();

  configure_trigger_timer(APP_SCOPE_ADC_TOTAL_RATE_HZ);

  scope_adc.Instance = APP_SCOPE_ADC_INSTANCE;
  scope_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  scope_adc.Init.Resolution = ADC_RESOLUTION_12B;
  scope_adc.Init.ScanConvMode = ENABLE;
  scope_adc.Init.ContinuousConvMode = DISABLE;
  scope_adc.Init.DiscontinuousConvMode = DISABLE;
  scope_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  scope_adc.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  scope_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  scope_adc.Init.NbrOfConversion = APP_SCOPE_ADC_SCAN_CHANNEL_COUNT;
  scope_adc.Init.DMAContinuousRequests = ENABLE;
  scope_adc.Init.EOCSelection = ADC_EOC_SEQ_CONV;

  if (HAL_ADC_Init(&scope_adc) != HAL_OK) {
    Error_Handler();
  }

  channel.Channel = APP_SCOPE_ADC_CHANNEL_A;
  channel.Rank = 1;
  channel.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  channel.Offset = 0;

  if (HAL_ADC_ConfigChannel(&scope_adc, &channel) != HAL_OK) {
    Error_Handler();
  }

  channel.Channel = APP_SCOPE_ADC_CHANNEL_B;
  channel.Rank = 2;

  if (HAL_ADC_ConfigChannel(&scope_adc, &channel) != HAL_OK) {
    Error_Handler();
  }

  (void)memset((void *)scope_dma_buffer, 0, sizeof(scope_dma_buffer));
  (void)memset((void *)&latest_frame, 0, sizeof(latest_frame));

  if (HAL_ADC_Start_DMA(&scope_adc,
                        (uint32_t *)scope_dma_buffer,
                        APP_SCOPE_DMA_BUFFER_SIZE) != HAL_OK) {
    Error_Handler();
  }
}

void signal_capture_adc_dma_irq_handler(void) {
  if (scope_adc.DMA_Handle != NULL) {
    HAL_DMA_IRQHandler(scope_adc.DMA_Handle);
  }
}

void signal_capture_adc_read_frame(scope_capture_frame_t *out, uint32_t now_ms) {
  if (out == NULL) {
    return;
  }

  __disable_irq();
  *out = latest_frame;
  __enable_irq();

  if (out->ch_a.valid &&
      (now_ms - out->ch_a.latest_update_ms) >= APP_SCOPE_SIGNAL_TIMEOUT_MS) {
    out->ch_a.valid = false;
  }
  if (out->ch_b.valid &&
      (now_ms - out->ch_b.latest_update_ms) >= APP_SCOPE_SIGNAL_TIMEOUT_MS) {
    out->ch_b.valid = false;
  }
}

uint32_t signal_capture_adc_channel_sample_rate_hz(void) {
  return APP_SCOPE_ADC_CHANNEL_RATE_HZ;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  update_frame_from_offset(0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  update_frame_from_offset(APP_SCOPE_ADC_CHUNK_RAW_COUNT);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  latest_frame.ch_a.valid = false;
  latest_frame.ch_b.valid = false;
}
