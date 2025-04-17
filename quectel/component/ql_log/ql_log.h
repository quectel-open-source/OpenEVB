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
  Name: ql_log.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_LOG_H__
#define __QL_LOG_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define QL_LOG_ERROR             (1U)
#define QL_LOG_WARN              (2U)
#define QL_LOG_INFO              (3U)
#define QL_LOG_DEBUG             (4U)

#define QL_LOG_PRINTF            Ql_Printf



#define QL_LOG_HDR(level)        QL_LOG_PRINTF("[" level "/" LOG_TAG "] ")
#define QL_LOG_END               QL_LOG_PRINTF("\r\n")

int Ql_Log_MutexTake(void);
int Ql_Log_MutexGive(void);

#define QL_LOG_LINE(Lvl, Fmt, ...)                                  \
        do {                                                        \
            if (Ql_Log_MutexTake()) break;                         \
            QL_LOG_HDR(Lvl);                                        \
            QL_LOG_PRINTF(Fmt, ##__VA_ARGS__);                      \
            QL_LOG_END;                                             \
            Ql_Log_MutexGive();                                    \
        } while (0)



void Ql_Printf(const int8_t *format, ...);
int32_t Ql_Log_FuncInit(void);

#endif /*__QL_LOG_H__*/

#ifndef QL_LOG_LVL
#define QL_LOG_LVL               QL_LOG_INFO
#endif

#ifndef QL_LOG_TAG
#define QL_LOG_TAG               "LOG"
#endif

#if (QL_LOG_LVL >= QL_LOG_DEBUG)
#define QL_LOG_D(Fmt, ...)  QL_LOG_LINE("D", Fmt, ##__VA_ARGS__)
#else
#define QL_LOG_D(...)
#endif

#if (QL_LOG_LVL >= QL_LOG_INFO)
#define QL_LOG_I(Fmt, ...)  QL_LOG_LINE("I", Fmt, ##__VA_ARGS__)
#else
#define QL_LOG_I(...)
#endif

#if (QL_LOG_LVL >= QL_LOG_WARN)
#define QL_LOG_W(Fmt, ...)  QL_LOG_LINE("W", Fmt, ##__VA_ARGS__)
#else
#define QL_LOG_W(...)
#endif

#if (QL_LOG_LVL >= QL_LOG_ERROR)
#define QL_LOG_E(Fmt, ...)  QL_LOG_LINE("E", Fmt, ##__VA_ARGS__)
#else
#define QL_LOG_E(...)
#endif
