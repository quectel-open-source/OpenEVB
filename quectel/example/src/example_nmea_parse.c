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
  Name: example_nmea_parse.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hayden   Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_NMEA_PARSE__

#include "FreeRTOS.h"
#include "task.h"
#include "ql_uart.h"
#include "ql_nmea.h"
#include "time.h"
#include "ql_rtc.h" 

#define LOG_TAG "nmea_parse"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

#define NMEA_BUF_SIZE          (4096U)
#define NMEA_PORT              UART3

Ql_NMEA_Handle_TypeDef  NMEA_Handle;

void Ql_Example_Task(void *Param)
{
    static uint8_t rx_buf[NMEA_BUF_SIZE] = {0};
    int32_t Length = 0;

    (void)Param;
    
    QL_LOG_I("--->NMEA Sentence Parse<---");

    Ql_Uart_Init("GNSS COM1", NMEA_PORT, 115200, NMEA_BUF_SIZE, NMEA_BUF_SIZE);

    // NMEA Handle
    extern const Ql_NMEA_Table_TypeDef NMEA_Table[];
    Ql_NMEA_Init(&NMEA_Handle,
                 (Ql_NMEA_Table_TypeDef *)NMEA_Table,
                 NULL,
                 NMEA_BUF_SIZE);

    while (1)
    {
        Length = Ql_Uart_Read(NMEA_PORT, rx_buf, NMEA_BUF_SIZE, 100);
        QL_LOG_D("read data from uart, len: %d", Length);

        if (Length <= 0)
        {
            continue;
        }
        rx_buf[Length] = '\0';

        Ql_NMEA_Parse(&NMEA_Handle, (const char *)rx_buf, Length);
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

static void Ql_NMEA_GGA_Frame(const char *Str, uint32_t Len)
{
    char GGA_Msg[256 + 1];
    int quality = -1;//initial value
    // Latitude in degrees, Positive = North, Negative = South.
    double   Latitude;
    // Longitude in degrees, Positive = East, Negative = West.
    double   Longitude;
    // In meters, meas sea level
    double   Altitude;

    int agrc = 15;
    char *argv[15];
   
    QL_LOG_D("gga sentence:%s", Str);

    memcpy(GGA_Msg, Str, Len);
    GGA_Msg[Len] = '\0';
    
    /* $GNGGA,034056.000,3149.300743,N,11706.920011,E,1,40,0.48,87.6,M,-0.3,M,,*5F */
    if (Ql_NMEA_Option_Parse((char *)Str, Len, &agrc, argv) == 0)
    {
        
        if ((*argv[2] != NULL) && (*argv[4] != NULL) && (*argv[9] != NULL))
        {
            Latitude  = atof(argv[2]);
            Longitude = atof(argv[4]);
            Altitude  = atof(argv[9]);
        }
        
        if (*argv[6] != NULL)
        {
            quality = atoi(argv[6]);
        }
        QL_LOG_I("Quality:%d, Latitude:%f, Longitude:%f, Altitude:%f", quality, Latitude, Longitude, Altitude);
    }
}
static void Ql_NMEA_RMC_Frame(const char *Str, uint32_t Len)
{
    uint32_t utc = 0;
    uint32_t date = 0;
    struct tm time = {0};
    static char buf[128] = {0};
    char *p;    
    int agrc = 14;
    char *argv[14];
    char location_status = 'V';

    
    /* $GNRMC,050748.000,A,3149.303735,N,11706.919772,E,0.046,312.46,050424,,,A,V*3F */
    if (Ql_NMEA_Option_Parse((char *)Str, Len, &agrc, argv) == 0)
    {
        if ((*argv[1] != NULL) && (*argv[9] != NULL))
        {
            utc  = strtoul(argv[1], &p, 10);
            date = strtoul(argv[9], &p, 10);
            location_status = *argv[2];

            time.tm_year = (date % 100 + (((date % 100) < 70) ? 2000 : 1900)) - 1900;
            time.tm_mon  = (date / 100 % 100) - 1;
            time.tm_mday = date / 10000;
            time.tm_hour = utc / 10000;
            time.tm_min  = utc / 100 % 100;
            time.tm_sec  = utc % 100;
            time.tm_isdst = -1;
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",&time);
            QL_LOG_I("RMC UTC: %s, Locaction status: %C\r\n",buf, location_status);
            Ql_RTC_Calibration(&time);
            
            memset(&time,0,sizeof(time));
            Ql_RTC_Get(&time);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",&time);
            QL_LOG_I("location rtc time:%s\r\n",buf);

            
        }
    }
}


const Ql_NMEA_Table_TypeDef NMEA_Table[] =
{
    { "GGA",       Ql_NMEA_GGA_Frame},
    { "RMC",       Ql_NMEA_RMC_Frame},
    {  NULL,       NULL             }
};

#endif
