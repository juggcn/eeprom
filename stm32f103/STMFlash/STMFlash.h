#ifndef __STMFlash_H__
#define __STMFlash_H__

#include "stm32f1xx_hal.h"

extern uint8_t ucSTMFlashErase(uint32_t addr, size_t size);
extern uint8_t ucSTMFlashRead(uint32_t addr, uint32_t *buf, size_t size);
extern uint8_t ucSTMFlashWrite(uint32_t addr, const uint32_t *buf, size_t size);

#endif

