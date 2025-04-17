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
  Name: ql_ctype.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_CTYPE_H__
#define __QL_CTYPE_H__

#include <stdint.h>

#define BUFFSIZE16                          (16U)
#define BUFFSIZE32                          (32U)
#define BUFFSIZE40                          (40U)
#define BUFFSIZE64                          (64U)
#define BUFFSIZE128                         (128U)
#define BUFFSIZE256                         (256U)
#define BUFFSIZE512                         (512U)
#define BUFFSIZE1024                        (1024U)
#define BUFFSIZE1550                        (1550U)
#define BUFFSIZE2048                        (2048U)

typedef struct 
{
    char* name;
    char* desc;
}QL_Name_TypeDef;

uint16_t Ql_Base64_Encode(int8_t *Buf, uint16_t Size, const int8_t *User, const int8_t *Pwd);
int Ql_Base64_Decode(const int8_t * base64, uint8_t * bindata);

#endif
