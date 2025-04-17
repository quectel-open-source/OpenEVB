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
  Name: ql_version.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef _QL_VERSION_H__
#define _QL_VERSION_H__

#include <stdint.h>

/*****************************************************

Release Version e.g.
#define QL_VERSION_MAIN "GNSS_MODULE_EVB_MCU_A_V1.0.7"
#define QL_VERSION_BETA ""

Beta Version e.g.
#define QL_VERSION_MAIN "GNSS_MODULE_EVB_MCU_A_V1.0.7"
#define QL_VERSION_BETA "BETA0315"

*****************************************************/

#define QL_VERSION_MAIN "GNSS_MODULE_EVB_MCU_A_V1.0.7"
#define QL_VERSION_BETA ""

/** 
 *  1. If you want to get the sub version, please define __QL_SUPPORT_SUB_VER__
 *  2. realize func: const int8_t* Ql_SubVer_Get(void);
*/

#define __QL_SUPPORT_SUB_VER__ 


#if defined (__GNUC__)                /* GNU GCC Compiler */
    #define __weak                     __attribute__((weak))
#else
    #define __weak
#endif

typedef struct
{
    char *Date;
    char *Time;
} Ql_BuildInfo_TypeDef;

typedef struct
{
    unsigned char Major;
    unsigned char Minor;
    unsigned char Build;
    unsigned char Reserve;
} Ql_Boot_Ver_TypeDef;

const int8_t *Ql_Ver_Get(void);
const int8_t *Ql_Version_Get(void);
const int8_t *Ql_Bootloader_Version_Get(void);

#endif
