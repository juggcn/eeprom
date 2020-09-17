#include "eeprom.h"

void vEepromRead(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen)
{
    ulAdd = DATA_EEPROM_BASE + ulAdd;
    for (uint32_t i = 0; i < ulLen && ulAdd < DATA_EEPROM_END; i++, ulAdd++)
    {
        *(pucDat + i) = *(uint8_t *)ulAdd;
    }
}

void vEepromWrite(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen)
{
    HAL_FLASHEx_DATAEEPROM_Unlock();
    ulAdd = DATA_EEPROM_BASE + ulAdd;
    for (uint32_t i = 0; i < ulLen && ulAdd < DATA_EEPROM_END; i++, ulAdd++)
    {
        HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, ulAdd, *(pucDat + i));
    }
    HAL_FLASHEx_DATAEEPROM_Lock();
}
