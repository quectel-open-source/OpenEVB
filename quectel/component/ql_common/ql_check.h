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
  Name: ql_check.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_CHECK_H__
#define __QL_CHECK_H__

#include <stdint.h>

uint8_t Ql_CheckXOR(const uint8_t *Data, const uint32_t Length);
uint16_t Ql_Check_Fletcher(const uint8_t *Data, const uint32_t Length);
unsigned int Ql_Check_CRC32(unsigned int InitVal, const unsigned char *pData, const unsigned int Length);

#endif /* _QL_CHECK_H_ */
