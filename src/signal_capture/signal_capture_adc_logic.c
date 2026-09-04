#include "signal_capture/signal_capture_adc_logic.h"

#include <string.h>

static bool adc_logic_signal_present(const uint16_t *samples, uint16_t sample_count) {
  uint16_t min_raw = 0U;
  uint16_t max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t span;

  if (!snapshot_bounds(samples, sample_count, &min_raw, &max_raw, &sum)) {
    return false;
  }

  span = (uint16_t)(max_raw - min_raw);
  if (span < APP_SCOPE_SIGNAL_FLAT_THRESHOLD || span < APP_SCOPE_SIGNAL_MIN_SPAN) {
    return false;
  }

  return min_raw <= APP_SCOPE_SQUARE_LOW_LEVEL_MAX &&
         max_raw >= APP_SCOPE_SQUARE_HIGH_LEVEL_MIN;
}

bool adc_logic_build_snapshot(const uint16_t *samples,
                              uint16_t sample_count,
                              uint32_t now_ms,
                              scope_capture_snapshot_t *snapshot) {
  uint32_t sum = 0U;

  if (snapshot == NULL || samples == NULL || sample_count == 0U ||
      sample_count > APP_SCOPE_SAMPLE_COUNT) {
    return false;
  }

  (void)memset(snapshot, 0, sizeof(*snapshot));
  (void)memcpy(snapshot->samples, samples, sample_count * sizeof(samples[0]));
  snapshot->sample_count = sample_count;

  if (!snapshot_bounds(snapshot->samples, sample_count, &snapshot->min_raw,
                       &snapshot->max_raw, &sum)) {
    return false;
  }

  snapshot->mean_raw = (uint16_t)(sum / sample_count);
  snapshot->flat_signal =
      (uint16_t)(snapshot->max_raw - snapshot->min_raw) < APP_SCOPE_SIGNAL_MIN_SPAN;
  snapshot->valid = adc_logic_signal_present(snapshot->samples, sample_count);
  snapshot->latest_update_ms = now_ms;
  return true;
}

bool adc_logic_unpack_interleaved_frame(const uint16_t *interleaved,
                                        uint16_t raw_count,
                                        uint32_t now_ms,
                                        scope_capture_frame_t *frame) {
  uint16_t samples_a[APP_SCOPE_SAMPLE_COUNT];
  uint16_t samples_b[APP_SCOPE_SAMPLE_COUNT];
  uint16_t channel_samples;

  if (interleaved == NULL || frame == NULL || (raw_count % 2U) != 0U) {
    return false;
  }

  channel_samples = (uint16_t)(raw_count / 2U);
  if (channel_samples == 0U || channel_samples > APP_SCOPE_SAMPLE_COUNT) {
    return false;
  }

  for (uint16_t i = 0U; i < channel_samples; ++i) {
    /* Apollo F429 实板上双 rank DMA 序列表现为 B/A 交错，这里按实测顺序映射回
     * 约定的 CH-A=PA0、CH-B=PA6。 */
    samples_b[i] = interleaved[i * 2U];
    samples_a[i] = interleaved[i * 2U + 1U];
  }

  return adc_logic_build_snapshot(samples_a, channel_samples, now_ms, &frame->ch_a) &&
         adc_logic_build_snapshot(samples_b, channel_samples, now_ms, &frame->ch_b);
}
