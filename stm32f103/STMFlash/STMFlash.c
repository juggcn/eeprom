#include "STMFlash.h"

uint8_t ucSTMFlashErase(uint32_t addr, size_t size)
{
    uint8_t ucResult = 0;
    size_t erase_pages, i;
    /* make sure the start address is a multiple of FLASH_ERASE_MIN_SIZE */
    assert_param(addr % FLASH_PAGE_SIZE == 0);
    /* calculate pages */
    erase_pages = size / FLASH_PAGE_SIZE;
    if (size % FLASH_PAGE_SIZE != 0)
    {
        erase_pages++;
    }
    /* start erase */
    uint32_t PageError = 0;
    FLASH_EraseInitTypeDef EraseInitStruct;

    HAL_FLASH_Unlock();
    for (i = 0; i < erase_pages; i++)
    {
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
        EraseInitStruct.PageAddress = addr + (FLASH_PAGE_SIZE * i);
        EraseInitStruct.NbPages = 1;

        if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
            ucResult = 1;
    }
    HAL_FLASH_Lock();
    return ucResult;
}

uint8_t ucSTMFlashRead(uint32_t addr, uint32_t *buf, size_t size)
{
    assert_param(size % 4 == 0);
    /*copy from flash to ram */
    for (; size > 0; size -= 4, addr += 4, buf++)
    {
        *buf = *(uint32_t *)addr;
    }
    return 0;
}

uint8_t ucSTMFlashWrite(uint32_t addr, const uint32_t *buf, size_t size)
{
    uint8_t ucResult = 0;
    size_t i;

    assert_param(size % 4 == 0);

    HAL_FLASH_Unlock();

    for (i = 0; i < size; i += 4, buf++, addr += 4)
    {
        /* write data */
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, *buf);

        uint32_t read_data = *(uint32_t *)addr;
        /* check data */
        if (read_data != *buf)
        {
            ucResult = 1;
            break;
        }
    }
    HAL_FLASH_Lock();
    return ucResult;
}
