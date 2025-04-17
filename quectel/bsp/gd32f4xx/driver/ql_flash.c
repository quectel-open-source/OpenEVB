/*
Copyright (c) 2025, Quectel Wireless Solutions Co., Ltd.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************
  Name: ql_flash.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_flash.h"

/* base address of the flash sectors */
#define ADDR_FLASH_SECTOR_0      ((uint32_t)0x08000000) /* Base address of Sector 0,   16 K bytes */
#define ADDR_FLASH_SECTOR_1      ((uint32_t)0x08004000) /* Base address of Sector 1,   16 K bytes */
#define ADDR_FLASH_SECTOR_2      ((uint32_t)0x08008000) /* Base address of Sector 2,   16 K bytes */
#define ADDR_FLASH_SECTOR_3      ((uint32_t)0x0800C000) /* Base address of Sector 3,   16 K bytes */
#define ADDR_FLASH_SECTOR_4      ((uint32_t)0x08010000) /* Base address of Sector 4,   64 K bytes */
#define ADDR_FLASH_SECTOR_5      ((uint32_t)0x08020000) /* Base address of Sector 5,  128 K bytes */
#define ADDR_FLASH_SECTOR_6      ((uint32_t)0x08040000) /* Base address of Sector 6,  128 K bytes */
#define ADDR_FLASH_SECTOR_7      ((uint32_t)0x08060000) /* Base address of Sector 7,  128 K bytes */
#define ADDR_FLASH_SECTOR_8      ((uint32_t)0x08080000) /* Base address of Sector 8,  128 K bytes */
#define ADDR_FLASH_SECTOR_9      ((uint32_t)0x080A0000) /* Base address of Sector 9,  128 K bytes */
#define ADDR_FLASH_SECTOR_10     ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 K bytes */
#define ADDR_FLASH_SECTOR_11     ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 K bytes */

#define ADDR_FLASH_SECTOR_12     ((uint32_t)0x08100000) /* Base address of Sector 12,  16 K bytes */
#define ADDR_FLASH_SECTOR_13     ((uint32_t)0x08104000) /* Base address of Sector 13,  16 K bytes */
#define ADDR_FLASH_SECTOR_14     ((uint32_t)0x08108000) /* Base address of Sector 14,  16 K bytes */
#define ADDR_FLASH_SECTOR_15     ((uint32_t)0x0810C000) /* Base address of Sector 15,  16 K bytes */
#define ADDR_FLASH_SECTOR_16     ((uint32_t)0x08110000) /* Base address of Sector 16,  64 K bytes */
#define ADDR_FLASH_SECTOR_17     ((uint32_t)0x08120000) /* Base address of Sector 17, 128 K bytes */
#define ADDR_FLASH_SECTOR_18     ((uint32_t)0x08140000) /* Base address of Sector 18, 128 K bytes */
#define ADDR_FLASH_SECTOR_19     ((uint32_t)0x08160000) /* Base address of Sector 19, 128 K bytes */
#define ADDR_FLASH_SECTOR_20     ((uint32_t)0x08180000) /* Base address of Sector 20, 128 K bytes */
#define ADDR_FLASH_SECTOR_21     ((uint32_t)0x081A0000) /* Base address of Sector 21, 128 K bytes */
#define ADDR_FLASH_SECTOR_22     ((uint32_t)0x081C0000) /* Base address of Sector 22, 128 K bytes */
#define ADDR_FLASH_SECTOR_23     ((uint32_t)0x081E0000) /* Base address of Sector 23, 128 K bytes */

#define ADDR_FLASH_SECTOR_24     ((uint32_t)0x08200000) /* Base address of Sector 24, 256 K bytes */
#define ADDR_FLASH_SECTOR_25     ((uint32_t)0x08240000) /* Base address of Sector 25, 256 K bytes */
#define ADDR_FLASH_SECTOR_26     ((uint32_t)0x08280000) /* Base address of Sector 26, 256 K bytes */
#define ADDR_FLASH_SECTOR_27     ((uint32_t)0x082C0000) /* Base address of Sector 27, 256 K bytes */

/**
 * Get the sector of a given address
 *
 * @param address flash address
 *
 * @return The sector of a given address
 */
static uint32_t Ql_Flash_Sector(uint32_t Addr)
{
    uint32_t sector = 0;

    if (Addr < ADDR_FLASH_SECTOR_0) {  }
    else if (Addr < ADDR_FLASH_SECTOR_1)  { sector =  CTL_SECTOR_NUMBER_0;  }
    else if (Addr < ADDR_FLASH_SECTOR_2)  { sector =  CTL_SECTOR_NUMBER_1;  }
    else if (Addr < ADDR_FLASH_SECTOR_3)  { sector =  CTL_SECTOR_NUMBER_2;  }
    else if (Addr < ADDR_FLASH_SECTOR_4)  { sector =  CTL_SECTOR_NUMBER_3;  }
    else if (Addr < ADDR_FLASH_SECTOR_5)  { sector =  CTL_SECTOR_NUMBER_4;  }
    else if (Addr < ADDR_FLASH_SECTOR_6)  { sector =  CTL_SECTOR_NUMBER_5;  }
    else if (Addr < ADDR_FLASH_SECTOR_7)  { sector =  CTL_SECTOR_NUMBER_6;  }
    else if (Addr < ADDR_FLASH_SECTOR_8)  { sector =  CTL_SECTOR_NUMBER_7;  }
    else if (Addr < ADDR_FLASH_SECTOR_9)  { sector =  CTL_SECTOR_NUMBER_8;  }
    else if (Addr < ADDR_FLASH_SECTOR_10) { sector =  CTL_SECTOR_NUMBER_9;  }
    else if (Addr < ADDR_FLASH_SECTOR_11) { sector =  CTL_SECTOR_NUMBER_10; }
    else if (Addr < ADDR_FLASH_SECTOR_12) { sector =  CTL_SECTOR_NUMBER_11; }
    else if (Addr < ADDR_FLASH_SECTOR_13) { sector =  CTL_SECTOR_NUMBER_12; }
    else if (Addr < ADDR_FLASH_SECTOR_14) { sector =  CTL_SECTOR_NUMBER_13; }
    else if (Addr < ADDR_FLASH_SECTOR_15) { sector =  CTL_SECTOR_NUMBER_14; }
    else if (Addr < ADDR_FLASH_SECTOR_16) { sector =  CTL_SECTOR_NUMBER_15; }
    else if (Addr < ADDR_FLASH_SECTOR_17) { sector =  CTL_SECTOR_NUMBER_16; }
    else if (Addr < ADDR_FLASH_SECTOR_18) { sector =  CTL_SECTOR_NUMBER_17; }
    else if (Addr < ADDR_FLASH_SECTOR_19) { sector =  CTL_SECTOR_NUMBER_18; }
    else if (Addr < ADDR_FLASH_SECTOR_20) { sector =  CTL_SECTOR_NUMBER_19; }
    else if (Addr < ADDR_FLASH_SECTOR_21) { sector =  CTL_SECTOR_NUMBER_20; }
    else if (Addr < ADDR_FLASH_SECTOR_22) { sector =  CTL_SECTOR_NUMBER_21; }
    else if (Addr < ADDR_FLASH_SECTOR_23) { sector =  CTL_SECTOR_NUMBER_22; }
    else if (Addr < ADDR_FLASH_SECTOR_24) { sector =  CTL_SECTOR_NUMBER_23; }
    else if (Addr < ADDR_FLASH_SECTOR_25) { sector =  CTL_SECTOR_NUMBER_24; }
    else if (Addr < ADDR_FLASH_SECTOR_26) { sector =  CTL_SECTOR_NUMBER_25; }
    else if (Addr < ADDR_FLASH_SECTOR_27) { sector =  CTL_SECTOR_NUMBER_26; }
    else { sector =  CTL_SECTOR_NUMBER_27; }

    return sector;
}

/**
 * Get the sector size
 *
 * @param sector sector
 *
 * @return sector size
 */
static uint32_t Ql_Flash_Sector_Size(uint32_t Sector)
{
    switch (Sector)
    {
        case CTL_SECTOR_NUMBER_0:  return  16 * 1024;
        case CTL_SECTOR_NUMBER_1:  return  16 * 1024;
        case CTL_SECTOR_NUMBER_2:  return  16 * 1024;
        case CTL_SECTOR_NUMBER_3:  return  16 * 1024;
        case CTL_SECTOR_NUMBER_4:  return  64 * 1024;
        case CTL_SECTOR_NUMBER_5:  return 128 * 1024;
        case CTL_SECTOR_NUMBER_6:  return 128 * 1024;
        case CTL_SECTOR_NUMBER_7:  return 128 * 1024;
        case CTL_SECTOR_NUMBER_8:  return 128 * 1024;
        case CTL_SECTOR_NUMBER_9:  return 128 * 1024;
        case CTL_SECTOR_NUMBER_10: return 128 * 1024;
        case CTL_SECTOR_NUMBER_11: return 128 * 1024;
        case CTL_SECTOR_NUMBER_12: return  16 * 1024;
        case CTL_SECTOR_NUMBER_13: return  16 * 1024;
        case CTL_SECTOR_NUMBER_14: return  16 * 1024;
        case CTL_SECTOR_NUMBER_15: return  16 * 1024;
        case CTL_SECTOR_NUMBER_16: return  64 * 1024;
        case CTL_SECTOR_NUMBER_17: return 128 * 1024;
        case CTL_SECTOR_NUMBER_18: return 128 * 1024;
        case CTL_SECTOR_NUMBER_19: return 128 * 1024;
        case CTL_SECTOR_NUMBER_20: return 128 * 1024;
        case CTL_SECTOR_NUMBER_21: return 128 * 1024;
        case CTL_SECTOR_NUMBER_22: return 128 * 1024;
        case CTL_SECTOR_NUMBER_23: return 128 * 1024;
        case CTL_SECTOR_NUMBER_24: return 256 * 1024;
        case CTL_SECTOR_NUMBER_25: return 256 * 1024;
        case CTL_SECTOR_NUMBER_26: return 256 * 1024;
        case CTL_SECTOR_NUMBER_27: return 256 * 1024;
        default: return 256 * 1024;
    }
}

int32_t Ql_Flash_Read(uint32_t Addr, uint8_t *Buf, uint32_t Size)
{
    if (Size == 0)
    {
        return 0;
    }
    
    for (uint32_t i = 0; i < Size; i++, Addr++, Buf++)
    {
        *Buf = *(uint8_t *)Addr;
    }
    return Size;
}

int32_t Ql_Flash_Write(uint32_t Addr, const uint8_t *Buf, uint32_t Size)
{
    uint8_t read_data;
    
    if (Size == 0)
    {
        return 0;
    }
    
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
    
    for (uint32_t i = 0; i < Size; i++, Addr++, Buf++)
    {
        /* write data */
        fmc_byte_program(Addr, *Buf);
        /* check data */
        read_data = *(uint8_t *)Addr;
        if (read_data != *Buf)
        {
            return -1;
        }
    }
    
    fmc_lock();
    
    return Size;
}

int32_t Ql_Flash_Erase(uint32_t Addr, uint32_t Size)
{
    uint32_t erased_size = 0;
    uint32_t cur_erase_sector;
    fmc_state_enum flash_status;
    
    if (Size == 0)
    {
        return 0;
    }
    
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
    
    /* it will stop when erased size is greater than setting size */
    while (erased_size < Size)
    {
        cur_erase_sector = Ql_Flash_Sector(Addr + erased_size);
        flash_status = fmc_sector_erase(cur_erase_sector);
        if (flash_status != FMC_READY)
        {
            return -1;
        }
        erased_size += Ql_Flash_Sector_Size(cur_erase_sector);
    }
    
    fmc_lock();
    
    return Size;
}
