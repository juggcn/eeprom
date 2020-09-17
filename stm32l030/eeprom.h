#ifndef __EEPROM_H
#define __EEPROM_H

#include <Arduino.h>

class eeprom
{
public:
    void init(void);
    void read(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen);
    void Write(uint32_t ulAdd, uint8_t *pucDat, uint32_t ulLen);
    // private:
};

#endif /* __EEPROM_H */