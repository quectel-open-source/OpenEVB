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
  Name: ql_cjson_object.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_cjson_object.h"

#define LOG_TAG "cJSON"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

static void * CJSON_CDECL PlatformMalloc(size_t size)
{
    return (void *)pvPortMalloc(size);
}

static void CJSON_CDECL PlatformFree(void *pointer)
{
    vPortFree(pointer);
}

static cJSON_Hooks PlatformHooks = {
    PlatformMalloc,
    PlatformFree
};

const cJSON_Hooks *Ql_cJSON_GetContext(void)
{
    return &PlatformHooks;
}


