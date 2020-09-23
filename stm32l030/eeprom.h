#ifndef __EEPROM_H
#define __EEPROM_H

#include "stm32l0xx_HAL.h"
#include "stdint.h"

#ifdef __cplusplus
 extern "C" {
#endif 

extern void vEepromRead(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen);
extern void vEepromWrite(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen);

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_H */