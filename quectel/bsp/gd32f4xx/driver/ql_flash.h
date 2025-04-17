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
  Name: ql_flash.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_FLASH_H_
#define __QL_FLASH_H_

#include "gd32f4xx.h"

int32_t Ql_Flash_Read(uint32_t Addr, uint8_t *Buf, uint32_t Size);
int32_t Ql_Flash_Write(uint32_t Addr, const uint8_t *Buf, uint32_t Size);
int32_t Ql_Flash_Erase(uint32_t Addr, uint32_t Size);

#endif
