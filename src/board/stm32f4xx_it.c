#include "main.h"
#include "display/display_backend.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"
#include "signal_measure/signal_measure.h"
#include "stm32f4xx_it.h"

/* 以下异常处理保持最小实现，便于在严重错误时停留现场。 */
void NMI_Handler(void) {}

void HardFault_Handler(void) {
  while (1) {
  }
}

void MemManage_Handler(void) {
  while (1) {
  }
}

void BusFault_Handler(void) {
  while (1) {
  }
}

void UsageFault_Handler(void) {
  while (1) {
  }
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {
  HAL_IncTick();
}

void DMA1_Stream5_IRQHandler(void) {
  signal_gen_dac_dma_irq_handler();
}

void DMA2_Stream0_IRQHandler(void) {
  signal_capture_adc_dma_irq_handler();
}

/* TIM3 中断仅转发给测量模块处理。 */
void TIM3_IRQHandler(void) {
  signal_measure_irq_handler();
}

/* LTDC 中断统一交给显示后端。 */
void LTDC_IRQHandler(void) {
  display_lcd_irq_handler();
}
