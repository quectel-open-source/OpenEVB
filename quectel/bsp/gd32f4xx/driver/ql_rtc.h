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
  Name: ql_rtc.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef _QL_RTC_H__
#define _QL_RTC_H__
//-----include header file-----
#include "gd32f4xx.h"
#include <stdbool.h>
#include "time.h"

//-----macro define -----
#define BCD2DEC(bcd)    (((bcd) >> 4) * 10 + ((bcd) & 0x0f))
#define DEC2BCD(dec)    ((((dec) / 10) << 4) | ((dec) % 10))

//-----enum define-----


//----type define-----


//-----function declare-----
void Ql_RTC_Init(void);
void Ql_RTC_Calibration(struct tm *time);
int32_t Ql_RTC_Get(struct tm *time);

#endif
