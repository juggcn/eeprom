/**
  ******************************************************************************
  * @file    EEPROM_Emulation/src/eeprom.c 
  * @author  MCD Application Team
  * @version V1.5.0
  * @date    14-April-2017
  * @brief   This file provides all the EEPROM emulation firmware functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/** @addtogroup EEPROM_Emulation
  * @{
  */

/* Includes ------------------------------------------------------------------*/
#include "eeprom.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Global variable used to store variable value in read sequence */
static uint16_t DataVar = 0;
static uint16_t usValidpage = 0xffff;
static uint32_t ulAddress = 0xffffffff;
#define USE_ADDR_OPTIMIZATION 0
/* Virtual address defined by the user: 0xFFFF value is prohibited */
// extern uint16_t VirtAddVarTab[NB_OF_VAR];

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static HAL_StatusTypeDef EE_Format(void);
static uint16_t EE_FindValidPage(uint8_t Operation);
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_VerifyPageFullyErased(uint32_t Address);

#include "STMFlash.h"
#include "includes.h"

uint16_t EE_FlashErase(uint32_t addr, size_t size)
{
  uint16_t Result = 0;
#if 1
  Result = ucSTMFlashErase(addr, size);
#else
  sfud_err sfud_result = SFUD_SUCCESS;
  const sfud_flash *flash = sfud_get_device_table() + SFUD_MX25_DEVICE_INDEX;

  sfud_result = sfud_erase(flash, addr, size);

  if (sfud_result != SFUD_SUCCESS)
  {
    Result = 1;
  }
#endif
  return Result;
}

uint16_t EE_FLASHWrite(uint32_t addr, uint8_t *pData, size_t size)
{
  uint16_t Result = 0;
#if 1
  HAL_FLASH_Unlock();
  Result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, *(uint16_t *)pData);
  HAL_FLASH_Lock();
#else
  sfud_err sfud_result = SFUD_SUCCESS;
  const sfud_flash *flash = sfud_get_device_table() + SFUD_MX25_DEVICE_INDEX;

  sfud_result = sfud_write(flash, addr, size, (const uint8_t *)pData);

  if (sfud_result != SFUD_SUCCESS)
  {
    Result = 1;
  }
#endif
  return Result;
}
uint16_t EE_FLASHRead(uint32_t addr, uint8_t *pData, size_t size)
{
  uint16_t Result = 0;
#if 1
  if (size == 2)
  {
    uint16_t usData = (*(__IO uint16_t *)addr);
    *pData = usData & 0x00ff;
    *(pData + 1) = (usData >> 8) & 0x00ff;
  }
  else if (size == 4)
  {
    uint32_t ulData = (*(__IO uint32_t *)addr);
    *pData = ulData & 0x00ff;
    *(pData + 1) = (ulData >> 24) & 0x00ff;
    *(pData + 2) = (ulData >> 16) & 0x00ff;
    *(pData + 3) = (ulData >> 8) & 0x00ff;
  }
#else
  sfud_err sfud_result = SFUD_SUCCESS;
  const sfud_flash *flash = sfud_get_device_table() + SFUD_MX25_DEVICE_INDEX;

  sfud_result = sfud_read(flash, addr, size, (uint8_t *)pData);

  if (sfud_result != SFUD_SUCCESS)
  {
    Result = 1;
  }
#endif
  return Result;
}

/**
  * @brief  Restore the pages to a known good state in case of page's status
  *   corruption after a power loss.
  * @param  None.
  * @retval - Flash error code: on write Flash error
  *         - FLASH_COMPLETE: on success
  */
uint16_t EE_Init(void)
{
  uint16_t pagestatus0 = 6, pagestatus1 = 6;
  uint16_t varidx = 0;
  uint16_t eepromstatus = 0, readstatus = 0;
  int16_t x = -1;
  HAL_StatusTypeDef flashstatus;
  uint32_t page_error = 0;
  FLASH_EraseInitTypeDef s_eraseinit;

  /* Get Page0 status */
  EE_FLASHRead(PAGE0_BASE_ADDRESS, (uint8_t *)&pagestatus0, 2);
  /* Get Page1 status */
  EE_FLASHRead(PAGE1_BASE_ADDRESS, (uint8_t *)&pagestatus1, 2);

  /* Fill EraseInit structure*/
  s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
  s_eraseinit.PageAddress = PAGE0_ID;
  s_eraseinit.NbPages = 1;

  /* Check for invalid header states and repair if necessary */
  switch (pagestatus0)
  {
  case ERASED:
    if (pagestatus1 == VALID_PAGE) /* Page0 erased, Page1 valid */
    {
      /* Erase Page0 */
      if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
    }
    else if (pagestatus1 == RECEIVE_DATA) /* Page0 erased, Page1 receive */
    {
      /* Erase Page0 */
      if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
      /* Mark Page1 as valid */
      uint16_t WData = VALID_PAGE;
      flashstatus = EE_FLASHWrite(PAGE1_BASE_ADDRESS, (uint8_t *)&WData, 2);
      /* If program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
    }
    else /* First EEPROM access (Page0&1 are erased) or invalid state -> format EEPROM */
    {
      /* Erase both Page0 and Page1 and set Page0 as valid page */
      flashstatus = EE_Format();
      /* If erase/program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
    }
    break;

  case RECEIVE_DATA:
    if (pagestatus1 == VALID_PAGE) /* Page0 receive, Page1 valid */
    {
      /* Transfer data from Page1 to Page0 */
      for (varidx = 0; varidx < NB_OF_VAR; varidx++)
      {
        uint16_t RData;
        EE_FLASHRead(PAGE0_BASE_ADDRESS + 6, (uint8_t *)&RData, 2);
        if (RData == varidx)
        {
          x = varidx;
        }
        if (varidx != x)
        {
          /* Read the last variables' updates */
          readstatus = EE_ReadVariable(varidx, &DataVar);
          /* In case variable corresponding to the virtual address was found */
          if (readstatus != 0x1)
          {
            /* Transfer the variable to the Page0 */
            eepromstatus = EE_VerifyPageFullWriteVariable(varidx, DataVar);
            /* If program operation was failed, a Flash error code is returned */
            if (eepromstatus != HAL_OK)
            {
              return eepromstatus;
            }
          }
        }
      }
      /* Mark Page0 as valid */
      uint16_t WData = VALID_PAGE;
      flashstatus = EE_FLASHWrite(PAGE0_BASE_ADDRESS, (uint8_t *)&WData, 2);
      /* If program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
      s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
      s_eraseinit.PageAddress = PAGE1_ID;
      s_eraseinit.NbPages = 1;
      /* Erase Page1 */
      if (!EE_VerifyPageFullyErased(PAGE1_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
    }
    else if (pagestatus1 == ERASED) /* Page0 receive, Page1 erased */
    {
      s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
      s_eraseinit.PageAddress = PAGE1_ID;
      s_eraseinit.NbPages = 1;
      /* Erase Page1 */
      if (!EE_VerifyPageFullyErased(PAGE1_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
      /* Mark Page0 as valid */
      uint16_t WData = VALID_PAGE;
      flashstatus = EE_FLASHWrite(PAGE0_BASE_ADDRESS, (uint8_t *)&WData, 2);
      /* If program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
    }
    else /* Invalid state -> format eeprom */
    {
      /* Erase both Page0 and Page1 and set Page0 as valid page */
      flashstatus = EE_Format();
      /* If erase/program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
    }
    break;

  case VALID_PAGE:
    if (pagestatus1 == VALID_PAGE) /* Invalid state -> format eeprom */
    {
      /* Erase both Page0 and Page1 and set Page0 as valid page */
      flashstatus = EE_Format();
      /* If erase/program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
    }
    else if (pagestatus1 == ERASED) /* Page0 valid, Page1 erased */
    {
      s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
      s_eraseinit.PageAddress = PAGE1_ID;
      s_eraseinit.NbPages = 1;
      /* Erase Page1 */
      if (!EE_VerifyPageFullyErased(PAGE1_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
    }
    else /* Page0 valid, Page1 receive */
    {
      /* Transfer data from Page0 to Page1 */
      for (varidx = 0; varidx < NB_OF_VAR; varidx++)
      {
        uint16_t RData;
        EE_FLASHRead(PAGE1_BASE_ADDRESS + 6, (uint8_t *)&RData, 2);
        if (RData == varidx)
        {
          x = varidx;
        }
        if (varidx != x)
        {
          /* Read the last variables' updates */
          readstatus = EE_ReadVariable(varidx, &DataVar);
          /* In case variable corresponding to the virtual address was found */
          if (readstatus != 0x1)
          {
            /* Transfer the variable to the Page1 */
            eepromstatus = EE_VerifyPageFullWriteVariable(varidx, DataVar);
            /* If program operation was failed, a Flash error code is returned */
            if (eepromstatus != HAL_OK)
            {
              return eepromstatus;
            }
          }
        }
      }
      /* Mark Page1 as valid */
      uint16_t WData = VALID_PAGE;
      flashstatus = EE_FLASHWrite(PAGE1_BASE_ADDRESS, (uint8_t *)&WData, 2);
      /* If program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
      s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
      s_eraseinit.PageAddress = PAGE0_ID;
      s_eraseinit.NbPages = 1;
      /* Erase Page0 */
      if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS))
      {
        flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
        /* If erase operation was failed, a Flash error code is returned */
        if (flashstatus != HAL_OK)
        {
          return flashstatus;
        }
      }
    }
    break;

  default: /* Any other state -> format eeprom */
    /* Erase both Page0 and Page1 and set Page0 as valid page */
    flashstatus = EE_Format();
    /* If erase/program operation was failed, a Flash error code is returned */
    if (flashstatus != HAL_OK)
    {
      return flashstatus;
    }
    break;
  }

  return HAL_OK;
}

/**
  * @brief  Verify if specified page is fully erased.
  * @param  Address: page address
  *   This parameter can be one of the following values:
  *     @arg PAGE0_BASE_ADDRESS: Page0 base address
  *     @arg PAGE1_BASE_ADDRESS: Page1 base address
  * @retval page fully erased status:
  *           - 0: if Page not erased
  *           - 1: if Page erased
  */
uint16_t EE_VerifyPageFullyErased(uint32_t Address)
{
  uint32_t readstatus = 1;
  uint16_t addressvalue = 0x5555;

  /* Check each active page address starting from end */
  while (Address <= PAGE0_END_ADDRESS)
  {
    /* Get the current location content to be compared with virtual address */
    EE_FLASHRead(Address, (uint8_t *)&addressvalue, 2);

    /* Compare the read address with the virtual address */
    if (addressvalue != ERASED)
    {

      /* In case variable value is read, reset readstatus flag */
      readstatus = 0;

      break;
    }
    /* Next address location */
    Address = Address + 4;
  }

  /* Return readstatus value: (0: Page not erased, 1: Page erased) */
  return readstatus;
}

/**
  * @brief  Returns the last stored variable data, if found, which correspond to
  *   the passed virtual address
  * @param  VirtAddress: Variable virtual address
  * @param  Data: Global variable contains the read variable value
  * @retval Success or error status:
  *           - 0: if variable was found
  *           - 1: if the variable was not found
  *           - NO_VALID_PAGE: if no valid page was found.
  */
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t *Data)
{
  uint16_t validpage = PAGE0;
  uint16_t addressvalue = 0x5555, readstatus = 1;
  uint32_t address = EEPROM_START_ADDRESS, PageStartAddress = EEPROM_START_ADDRESS;

  /* Get active Page for read operation */
  validpage = EE_FindValidPage(READ_FROM_VALID_PAGE);

  /* Check if there is no valid page */
  if (validpage == NO_VALID_PAGE)
  {
    return NO_VALID_PAGE;
  }

  /* Get the valid Page start Address */
  PageStartAddress = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(validpage * PAGE_SIZE));

  /* Get the valid Page end Address */
#if (USE_ADDR_OPTIMIZATION == 0)
  address = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + validpage) * PAGE_SIZE));
#else
  if (ulAddress == 0xffffffff)
  {
    address = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + validpage) * PAGE_SIZE));
  }
  else
  {
    address = ulAddress - 2;
  }
#endif
  /* Check each active page address starting from end */
  while (address > (PageStartAddress + 2))
  {
    /* Get the current location content to be compared with virtual address */
    EE_FLASHRead(address, (uint8_t *)&addressvalue, 2);

    /* Compare the read address with the virtual address */
    if (addressvalue == VirtAddress)
    {
      /* Get content of Address-2 which is variable value */
      EE_FLASHRead(address - 2, (uint8_t *)Data, 2);

      /* In case variable value is read, reset readstatus flag */
      readstatus = 0;

      break;
    }
    else
    {
      /* Next address location */
      address = address - 4;
    }
  }

  /* Return readstatus value: (0: variable exist, 1: variable doesn't exist) */
  return readstatus;
}

/**
  * @brief  Writes/upadtes variable data in EEPROM.
  * @param  VirtAddress: Variable virtual address
  * @param  Data: 16 bit data to be written
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
  uint16_t Status = 0;

  /* Write the variable virtual address and value in the EEPROM */
  Status = EE_VerifyPageFullWriteVariable(VirtAddress, Data);

  /* In case the EEPROM active page is full */
  if (Status == PAGE_FULL)
  {
    /* Perform Page transfer */
    Status = EE_PageTransfer(VirtAddress, Data);
  }

  /* Return last operation status */
  return Status;
}

/**
  * @brief  Erases PAGE and PAGE1 and writes VALID_PAGE header to PAGE
  * @param  None
  * @retval Status of the last operation (Flash write or erase) done during
  *         EEPROM formating
  */
static HAL_StatusTypeDef EE_Format(void)
{
  HAL_StatusTypeDef flashstatus = HAL_OK;
  uint32_t page_error = 0;
  FLASH_EraseInitTypeDef s_eraseinit;

  s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
  s_eraseinit.PageAddress = PAGE0_ID;
  s_eraseinit.NbPages = 1;
  /* Erase Page0 */
  if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS))
  {
    flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
    /* If erase operation was failed, a Flash error code is returned */
    if (flashstatus != HAL_OK)
    {
      return flashstatus;
    }
  }
  /* Set Page0 as valid page: Write VALID_PAGE at Page0 base address */
  static uint16_t WData = VALID_PAGE;
  flashstatus = EE_FLASHWrite(PAGE0_BASE_ADDRESS, (uint8_t *)&WData, 2);
  /* If program operation was failed, a Flash error code is returned */
  if (flashstatus != HAL_OK)
  {
    return flashstatus;
  }

  s_eraseinit.PageAddress = PAGE1_ID;
  /* Erase Page1 */
  if (!EE_VerifyPageFullyErased(PAGE1_BASE_ADDRESS))
  {
    flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
    /* If erase operation was failed, a Flash error code is returned */
    if (flashstatus != HAL_OK)
    {
      return flashstatus;
    }
  }

  return HAL_OK;
}

/**
  * @brief  Find valid Page for write or read operation
  * @param  Operation: operation to achieve on the valid page.
  *   This parameter can be one of the following values:
  *     @arg READ_FROM_VALID_PAGE: read operation from valid page
  *     @arg WRITE_IN_VALID_PAGE: write operation from valid page
  * @retval Valid page number (PAGE or PAGE1) or NO_VALID_PAGE in case
  *   of no valid page was found
  */
static uint16_t EE_FindValidPage(uint8_t Operation)
{
  uint16_t pagestatus0 = 6, pagestatus1 = 6;

  /* Get Page0 actual status */
  EE_FLASHRead(PAGE0_BASE_ADDRESS, (uint8_t *)&pagestatus0, 2);
  /* Get Page1 actual status */
  EE_FLASHRead(PAGE1_BASE_ADDRESS, (uint8_t *)&pagestatus1, 2);

  /* Write or read operation */
  switch (Operation)
  {
  case WRITE_IN_VALID_PAGE: /* ---- Write operation ---- */
    if (pagestatus1 == VALID_PAGE)
    {
      /* Page0 receiving data */
      if (pagestatus0 == RECEIVE_DATA)
      {
        return PAGE0; /* Page0 valid */
      }
      else
      {
        return PAGE1; /* Page1 valid */
      }
    }
    else if (pagestatus0 == VALID_PAGE)
    {
      /* Page1 receiving data */
      if (pagestatus1 == RECEIVE_DATA)
      {
        return PAGE1; /* Page1 valid */
      }
      else
      {
        return PAGE0; /* Page0 valid */
      }
    }
    else
    {
      return NO_VALID_PAGE; /* No valid Page */
    }

  case READ_FROM_VALID_PAGE: /* ---- Read operation ---- */
    if (pagestatus0 == VALID_PAGE)
    {
      return PAGE0; /* Page0 valid */
    }
    else if (pagestatus1 == VALID_PAGE)
    {
      return PAGE1; /* Page1 valid */
    }
    else
    {
      return NO_VALID_PAGE; /* No valid Page */
    }

  default:
    return PAGE0; /* Page0 valid */
  }
}

/**
  * @brief  Verify if active page is full and Writes variable in EEPROM.
  * @param  VirtAddress: 16 bit virtual address of the variable
  * @param  Data: 16 bit data to be written as variable value
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data)
{
  HAL_StatusTypeDef flashstatus = HAL_OK;
  uint16_t validpage = PAGE0;
  uint32_t address = EEPROM_START_ADDRESS, pageendaddress = EEPROM_START_ADDRESS + PAGE_SIZE;

  /* Get valid Page for write operation */
  validpage = EE_FindValidPage(WRITE_IN_VALID_PAGE);

  /* Check if there is no valid page */
  if (validpage == NO_VALID_PAGE)
  {
    return NO_VALID_PAGE;
  }

  /* Get the valid Page start address */
#if (USE_ADDR_OPTIMIZATION == 0)
  address = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(validpage * PAGE_SIZE));
#else
  if (usValidpage != validpage)
  {
    usValidpage = validpage;
    address = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(validpage * PAGE_SIZE));
  }
  else
  {
    address = ulAddress;
  }
#endif
  /* Get the valid Page end address */
  pageendaddress = (uint32_t)((EEPROM_START_ADDRESS - 1) + (uint32_t)((validpage + 1) * PAGE_SIZE));

  /* Check each active page address starting from begining */
  while (address < pageendaddress)
  {
    /* Verify if address and address+2 contents are 0xFFFFFFFF */
    uint32_t RData;
    EE_FLASHRead(address, (uint8_t *)&RData, 4);
    if (RData == 0xFFFFFFFF)
    {
      /* Set variable data */
      flashstatus = EE_FLASHWrite(address, (uint8_t *)&Data, 2);
      /* If program operation was failed, a Flash error code is returned */
      if (flashstatus != HAL_OK)
      {
        return flashstatus;
      }
      /* Set variable virtual address */
      flashstatus = EE_FLASHWrite(address + 2, (uint8_t *)&VirtAddress, 2);
#if (USE_ADDR_OPTIMIZATION == 0)
      NULL;
#else
      ulAddress = address + 4;
#endif
      /* Return program operation status */
      return flashstatus;
    }
    else
    {
      /* Next address location */
      address = address + 4;
    }
  }

  /* Return PAGE_FULL in case the valid page is full */
  return PAGE_FULL;
}

/**
  * @brief  Transfers last updated variables data from the full Page to
  *   an empty one.
  * @param  VirtAddress: 16 bit virtual address of the variable
  * @param  Data: 16 bit data to be written as variable value
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data)
{
  HAL_StatusTypeDef flashstatus = HAL_OK;
  uint32_t newpageaddress = EEPROM_START_ADDRESS;
  uint32_t oldpageid = 0;
  uint16_t validpage = PAGE0, varidx = 0;
  uint16_t eepromstatus = 0, readstatus = 0;
  uint32_t page_error = 0;
  FLASH_EraseInitTypeDef s_eraseinit;

  /* Get active Page for read operation */
  validpage = EE_FindValidPage(READ_FROM_VALID_PAGE);

  if (validpage == PAGE1) /* Page1 valid */
  {
    /* New page address where variable will be moved to */
    newpageaddress = PAGE0_BASE_ADDRESS;

    /* Old page ID where variable will be taken from */
    oldpageid = PAGE1_ID;
  }
  else if (validpage == PAGE0) /* Page0 valid */
  {
    /* New page address  where variable will be moved to */
    newpageaddress = PAGE1_BASE_ADDRESS;

    /* Old page ID where variable will be taken from */
    oldpageid = PAGE0_ID;
  }
  else
  {
    return NO_VALID_PAGE; /* No valid Page */
  }

  /* Set the new Page status to RECEIVE_DATA status */
  uint16_t WData = RECEIVE_DATA;
  flashstatus = EE_FLASHWrite(newpageaddress, (uint8_t *)&WData, 2);
  /* If program operation was failed, a Flash error code is returned */
  if (flashstatus != HAL_OK)
  {
    return flashstatus;
  }

  /* Write the variable passed as parameter in the new active page */
  eepromstatus = EE_VerifyPageFullWriteVariable(VirtAddress, Data);
  /* If program operation was failed, a Flash error code is returned */
  if (eepromstatus != HAL_OK)
  {
    return eepromstatus;
  }

  /* Transfer process: transfer variables from old to the new active page */
  for (varidx = 0; varidx < NB_OF_VAR; varidx++)
  {
    if (varidx != VirtAddress) /* Check each variable except the one passed as parameter */
    {
      /* Read the other last variable updates */
      readstatus = EE_ReadVariable(varidx, &DataVar);
      /* In case variable corresponding to the virtual address was found */
      if (readstatus != 0x1)
      {
        /* Transfer the variable to the new active page */
        eepromstatus = EE_VerifyPageFullWriteVariable(varidx, DataVar);
        /* If program operation was failed, a Flash error code is returned */
        if (eepromstatus != HAL_OK)
        {
          return eepromstatus;
        }
      }
    }
  }

  s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
  s_eraseinit.PageAddress = oldpageid;
  s_eraseinit.NbPages = 1;

  /* Erase the old Page: Set old Page status to ERASED status */
  flashstatus = EE_FlashErase(s_eraseinit.PageAddress, PAGE_SIZE);
  /* If erase operation was failed, a Flash error code is returned */
  if (flashstatus != HAL_OK)
  {
    return flashstatus;
  }

  /* Set new Page status to VALID_PAGE status */
  WData = VALID_PAGE;
  flashstatus = EE_FLASHWrite(newpageaddress, (uint8_t *)&WData, 2);
  /* If program operation was failed, a Flash error code is returned */
  if (flashstatus != HAL_OK)
  {
    return flashstatus;
  }

  /* Return last operation flash status */
  return flashstatus;
}

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
uint16_t usReadRes;
uint16_t usWriteRes;

uint16_t usEE_Read(uint16_t usAdd, uint16_t *pusDat, uint16_t usLen)
{
  __disable_irq();
  assert_param(usLen % 2 == 0);
  usLen /= 2;
  for (uint16_t i = 0; i < usLen; i++)
  {
    usReadRes = EE_ReadVariable(usAdd + i, pusDat + i);
  }
  __enable_irq();
  return 0;
}
uint16_t usEE_Write(uint16_t usAdd, uint16_t *pusDat, uint16_t usLen)
{
  __disable_irq();
  assert_param(usLen % 2 == 0);
  usLen /= 2;
  HAL_FLASH_Unlock();
  for (uint16_t i = 0; i < usLen; i++)
  {
    usWriteRes = EE_WriteVariable(usAdd + i, *(pusDat + i));
  }
  HAL_FLASH_Lock();
  __enable_irq();
  return 0;
}
/**************************************************************************************************/
