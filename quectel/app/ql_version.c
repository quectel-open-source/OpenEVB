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
  Name: ql_version.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include <stdio.h>

#include "ql_version.h"
#include "ql_log.h"

static const int8_t Ql_Version[] = QL_VERSION_MAIN;
static const int8_t Ql_Version_Beta[] = QL_VERSION_BETA;
static int8_t Ql_Ver[124] = "";

Ql_Boot_Ver_TypeDef Ql_Boot_Version __attribute__((at(0x20000000)));
static int8_t Ql_Boot_Version_Str[32] = "";

Ql_BuildInfo_TypeDef Ql_BuildInfo =
{
    .Date = __DATE__,
    .Time = __TIME__,
};

#ifdef __QL_SUPPORT_SUB_VER__
__weak const int8_t* Ql_SubVer_Get(void)
{
    return NULL;
}
#endif // __QL_SUPPORT_SUB_VER__

const int8_t *Ql_Ver_Get(void)
{
    char *p = (char *)Ql_Version;

    p = strstr(p + 1, ".");
    p = strstr(p + 1, ".");
    memcpy((char *)Ql_Ver, (char *)Ql_Version, p - (char *)Ql_Version);
    Ql_Ver[p - (char *)Ql_Version] = '\0';

    if (strlen((char *)Ql_Version_Beta) > 0)
    {
        sprintf((char *)Ql_Ver + (p - (char *)Ql_Version), "_%s", Ql_Version_Beta);
    }

#ifdef __QL_SUPPORT_SUB_VER__
extern const int8_t* Ql_SubVer_Get(void);
    if(Ql_SubVer_Get() != NULL)
    {
        sprintf((char *)Ql_Ver + strlen((char *)Ql_Ver), "_SUB_%s", Ql_SubVer_Get());
    }
#endif // __QL_SUPPORT_SUB_VER__


    return Ql_Ver;
}

const int8_t *Ql_Version_Get(void)
{
    if (strlen((char *)Ql_Version_Beta) > 0)
    {
        sprintf((char *)Ql_Ver, "%s_%s", (char *)Ql_Version, Ql_Version_Beta);
    }
    else
    {
        sprintf((char *)Ql_Ver, "%s", Ql_Version);
    }

    return Ql_Ver;
}

const int8_t *Ql_Bootloader_Version_Get(void)
{
    sprintf(Ql_Boot_Version_Str, "GNSS_MODULE_EVB_BOOT_V%d.%d.%d",
            Ql_Boot_Version.Major,
            Ql_Boot_Version.Minor,
            Ql_Boot_Version.Build);
    
    return Ql_Boot_Version_Str;
}

