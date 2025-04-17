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
  Name: example_nmea_save.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hayden   Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_NMEA_SAVE__


#include "FreeRTOS.h"
#include "task.h"
#include "ql_uart.h"
#include "ql_nmea.h"
#include "time.h"
#include "ql_ff_user.h"

#define LOG_TAG "nmea_save"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

#define NMEA_BUF_SIZE          (4096U)
#define NMEA_PORT              UART3

void Ql_Example_Task(void *Param)
{
    static uint8_t rx_buf[NMEA_BUF_SIZE] = {0};
    int32_t Length = 0;
    char* nmea_file_path = "1:save_nmea_example.txt";
    uint32_t file_size = 0;
    int32_t ret = 0;

    (void)Param;
    
    QL_LOG_I("--->NMEA Sentence Save Example<---");

    Ql_Uart_Init("GNSS COM1", NMEA_PORT, 115200, NMEA_BUF_SIZE, NMEA_BUF_SIZE);
    ret = Ql_FatFs_Mount();
    if (0 != ret)
    {
        QL_LOG_E("FatFs Mount Failed, ret: %d", ret);
    }

    while (1)
    {
        Length = Ql_Uart_Read(NMEA_PORT, rx_buf, NMEA_BUF_SIZE, 100);
        QL_LOG_I("read data from uart, len: %d", Length);

        if (Length <= 0)
        {
            continue;
        }
        rx_buf[Length] = '\0';
        if (0 == ret ) 
        {
            ret = Ql_FatFs_Write(nmea_file_path, rx_buf, Length, &file_size);
            QL_LOG_I("write data to file, len: %d,file size:%d,ret = %d", Length,file_size,ret);
        }
        else
        {
            QL_LOG_E("FatFs Write Failed, ret: %d", ret);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif
