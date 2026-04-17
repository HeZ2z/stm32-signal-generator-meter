#include "signal_capture/signal_capture_adc.h"

#include <string.h>

#include "main.h"

static ADC_HandleTypeDef scope_adc;
static uint16_t scope_dma_buffer[APP_SCOPE_DMA_BUFFER_SIZE];
static volatile scope_capture_snapshot_t latest_snapshot;

static void update_snapshot_from_offset(uint16_t offset) {
  uint32_t sum = 0U;
  uint16_t min_raw = 0xFFFFU;
  uint16_t max_raw = 0U;
  scope_capture_snapshot_t next = {0};

  if ((offset + APP_SCOPE_SAMPLE_COUNT) > APP_SCOPE_DMA_BUFFER_SIZE) {
    return;
  }

  for (uint16_t i = 0; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    uint16_t sample = scope_dma_buffer[offset + i];
    next.samples[i] = sample;
    if (sample < min_raw) {
      min_raw = sample;
    }
    if (sample > max_raw) {
      max_raw = sample;
    }
    sum += sample;
  }

  next.sample_count = APP_SCOPE_SAMPLE_COUNT;
  next.min_raw = min_raw;
  next.max_raw = max_raw;
  next.mean_raw = (uint16_t)(sum / APP_SCOPE_SAMPLE_COUNT);
  next.flat_signal = (uint16_t)(max_raw - min_raw) < APP_SCOPE_SIGNAL_MIN_SPAN;
  next.valid = !next.flat_signal;
  next.latest_update_ms = HAL_GetTick();
  latest_snapshot = next;
}

void signal_capture_adc_init(void) {
  ADC_ChannelConfTypeDef channel = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();

  scope_adc.Instance = APP_SCOPE_ADC_INSTANCE;
  scope_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  scope_adc.Init.Resolution = ADC_RESOLUTION_12B;
  scope_adc.Init.ScanConvMode = DISABLE;
  scope_adc.Init.ContinuousConvMode = ENABLE;
  scope_adc.Init.DiscontinuousConvMode = DISABLE;
  scope_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  scope_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  scope_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  scope_adc.Init.NbrOfConversion = 1;
  scope_adc.Init.DMAContinuousRequests = ENABLE;
  scope_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

  if (HAL_ADC_Init(&scope_adc) != HAL_OK) {
    Error_Handler();
  }

  channel.Channel = APP_SCOPE_ADC_CHANNEL;
  channel.Rank = 1;
  channel.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  channel.Offset = 0;

  if (HAL_ADC_ConfigChannel(&scope_adc, &channel) != HAL_OK) {
    Error_Handler();
  }

  (void)memset((void *)scope_dma_buffer, 0, sizeof(scope_dma_buffer));
  (void)memset((void *)&latest_snapshot, 0, sizeof(latest_snapshot));

  if (HAL_ADC_Start_DMA(&scope_adc,
                        (uint32_t *)scope_dma_buffer,
                        APP_SCOPE_DMA_BUFFER_SIZE) != HAL_OK) {
    Error_Handler();
  }
}

void signal_capture_adc_poll(uint32_t now_ms) {
  if (latest_snapshot.valid &&
      (now_ms - latest_snapshot.latest_update_ms) >= APP_SCOPE_SIGNAL_TIMEOUT_MS) {
    latest_snapshot.valid = false;
  }
}

void signal_capture_adc_dma_irq_handler(void) {
  if (scope_adc.DMA_Handle != NULL) {
    HAL_DMA_IRQHandler(scope_adc.DMA_Handle);
  }
}

const scope_capture_snapshot_t *signal_capture_adc_latest(void) {
  return (const scope_capture_snapshot_t *)&latest_snapshot;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  update_snapshot_from_offset(0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  update_snapshot_from_offset((uint16_t)(APP_SCOPE_DMA_BUFFER_SIZE - APP_SCOPE_SAMPLE_COUNT));
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance != APP_SCOPE_ADC_INSTANCE) {
    return;
  }

  latest_snapshot.valid = false;
}
