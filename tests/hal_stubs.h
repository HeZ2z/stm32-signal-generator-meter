/*
 * Minimal HAL declarations for HOST_TEST compilation.
 * Provides stub prototypes that ui_ctrl.c and its transitive includes need,
 * replacing what stm32f4xx_hal.h would normally supply on ARM.
 */
#ifndef HAL_STUBS_H
#define HAL_STUBS_H

#include <stdint.h>

uint32_t HAL_GetTick(void);

#endif
