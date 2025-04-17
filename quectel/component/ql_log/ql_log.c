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
  Name: ql_log.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#define LOG_TAG "log"
#define LOG_LVL QL_LOG_INFO
#include <ql_log.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ql_uart.h"

#define QL_PRINTF_BUF_SIZE    (1024*4)

static uint8_t Ql_Log_Buf[QL_PRINTF_BUF_SIZE] = {0};
static SemaphoreHandle_t Ql_Log_Mutex = NULL;

int32_t Ql_Log_MutexTake(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        if (Ql_Log_Mutex == NULL)
        {
            return -1;
        }

        xSemaphoreTake(Ql_Log_Mutex, portMAX_DELAY);
    }

    return 0;
}

int32_t Ql_Log_MutexGive(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        if (Ql_Log_Mutex == NULL)
        {
            return -1;
        }

        xSemaphoreGive(Ql_Log_Mutex);
    }

    return 0;
}

int32_t Ql_Log_FuncInit(void)
{
    if (Ql_Log_Mutex == NULL)
    {
        Ql_Log_Mutex = xSemaphoreCreateMutex();
        if (Ql_Log_Mutex == NULL)
        {
            return -1;
        }
    }

    return 0;
}

void Ql_Printf(const int8_t *format, ...)
{
    size_t log_len = 0;
    va_list args;
    int fmt_result;

    /* args point to the first variable parameter */
    va_start(args, format);
    /* lock output */
    /* package other log data to buffer. '\0' must be added in the end by vsnprintf. */
    fmt_result = vsnprintf(Ql_Log_Buf, sizeof(Ql_Log_Buf), format, args);
    va_end(args);

    /* calculate log length */
    if ((fmt_result <= sizeof(Ql_Log_Buf)) && (fmt_result > -1))
    {
        log_len = fmt_result;
    }
    else
    {
        /* using max length */
        log_len = sizeof(Ql_Log_Buf);
    }

    Ql_Log_Uart_Output((const uint8_t *)Ql_Log_Buf, log_len);

    return;
}

