#ifndef STM32F4XX_IT_H
#define STM32F4XX_IT_H

/* Cortex-M 异常和项目实际用到的外设中断入口声明。 */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void DMA2_Stream0_IRQHandler(void);
void TIM3_IRQHandler(void);
void LTDC_IRQHandler(void);

#endif
