#ifndef MAIN_H
#define MAIN_H

#include "board/board_config.h"
#include "stm32f4xx_hal.h"

/* 配置系统主时钟和 LTDC 像素时钟。 */
void SystemClock_Config(void);
/* 进入不可恢复错误处理路径。 */
void Error_Handler(void);

#endif
