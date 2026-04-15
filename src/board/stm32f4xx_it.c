#include "main.h"
#include "display/display_backend.h"
#include "signal_measure/signal_measure.h"
#include "stm32f4xx_it.h"

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

void TIM3_IRQHandler(void) {
  signal_measure_irq_handler();
}

void LTDC_IRQHandler(void) {
  display_lcd_irq_handler();
}
