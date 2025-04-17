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
  Name: ql_delay.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_DELAY_H__
#define __QL_DELAY_H__

#include "gd32f4xx.h"

/** 
  * @brief  QL Status structures definition  
  */  
typedef enum 
{
  QL_OK       = 0x00,
  QL_ERROR    = 0x01,
  QL_BUSY     = 0x02,
  QL_TIMEOUT  = 0x03
} Ql_StatusTypeDef;

void Ql_IncTick(void);
uint32_t Ql_GetTick(void);
void Ql_DelayUS(uint32_t Millisecond);

void softdelay_calibrate(void);
uint64_t getus(void);
void delay_us(uint32_t us);

#endif

