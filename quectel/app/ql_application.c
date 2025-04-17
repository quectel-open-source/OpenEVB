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
  Name: ql_application.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_application.h"
#include "ql_delay.h"
#include "ql_rtc.h"
#include "ql_trng.h"
#include "ql_flash.h"
#include "ql_cjson_object.h"
#include "ql_uart.h"
#include "ql_version.h"
#include "ql_rtc.h"
#include "ql_utils.h"

#include "sockets_wrapper.h"

#include "ql_log_undef.h"
#define LOG_TAG "APP"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

#define CELLULAR_START_NETWORK            EVENTBIT(2)

#if defined(__EXAMPLE_NTRIP_CLIENT__) || defined(__EXAMPLE_QUECRTK__) || defined(__TEST_QUECRTK__)
TaskHandle_t Ql_Cellular_TaskHandle;
static TaskHandle_t Ql_Network_TaskHandle;
static EventGroupHandle_t Ql_CellularEvent;
StaticEventGroup_t StaticCellularEvent;
#endif

usart_cfg_t *Uart_Cfg[7];

Ql_Monitor_TypeDef Ql_Monitor = {

};

extern uint8_t Ql_EC600U_GnssComSelect;
extern CellularHandle_t CellularHandle;
extern bool IsCellSimReady;

extern bool setupCellular( void );
extern bool Ql_SetCellModleID(void);
extern CellularError_t Ql_Cellular_Init( void );
extern CellularError_t Cellular_Reboot( void );
extern CellularError_t Cellular_GetNetworkTime( CellularHandle_t cellularHandle,CellularTime_t * pNetworkTime );
#ifdef __EXAMPLE_GNSS__
extern void Ql_Example_Task(void *Param);
#else
extern void Ql_Test_Task(void *Param);
#endif

static void Ql_Param_Default(void)
{
    Uart_Cfg[0]->Reg.baudrate = 115200;
    Uart_Cfg[0]->Reg.databits = 8;
    Uart_Cfg[0]->Reg.stopbits = 1;
    Uart_Cfg[0]->Reg.parity = 0;
    Uart_Cfg[0]->Reg.flow_control = 0;
    Uart_Cfg[0]->Recv_Debug = 0;

    Uart_Cfg[1]->Reg.baudrate = 115200;
    Uart_Cfg[1]->Reg.databits = 8;
    Uart_Cfg[1]->Reg.stopbits = 1;
    Uart_Cfg[1]->Reg.parity = 0;
    Uart_Cfg[1]->Reg.flow_control = 0;
    Uart_Cfg[1]->Recv_Debug = 0;

    Uart_Cfg[2]->Reg.baudrate = 115200;
    Uart_Cfg[2]->Reg.databits = 8;
    Uart_Cfg[2]->Reg.stopbits = 1;
    Uart_Cfg[2]->Reg.parity = 0;
    Uart_Cfg[2]->Reg.flow_control = 0;
    Uart_Cfg[2]->Recv_Debug = 0;

    Uart_Cfg[3]->Reg.baudrate = 460800;
    Uart_Cfg[3]->Reg.databits = 8;
    Uart_Cfg[3]->Reg.stopbits = 1;
    Uart_Cfg[3]->Reg.parity = 0;
    Uart_Cfg[3]->Reg.flow_control = 0;
    Uart_Cfg[3]->Recv_Debug = 0;

    Uart_Cfg[4]->Reg.baudrate = 115200;
    Uart_Cfg[4]->Reg.databits = 8;
    Uart_Cfg[4]->Reg.stopbits = 1;
    Uart_Cfg[4]->Reg.parity = 0;
    Uart_Cfg[4]->Reg.flow_control = 0;
    Uart_Cfg[4]->Recv_Debug = 0;

    Uart_Cfg[6]->Reg.baudrate = 460800;
    Uart_Cfg[6]->Reg.databits = 8;
    Uart_Cfg[6]->Reg.stopbits = 1;
    Uart_Cfg[6]->Reg.parity = 0;
    Uart_Cfg[6]->Reg.flow_control = 0;
    Uart_Cfg[6]->Recv_Debug = 0;

    // System
    Ql_SystemPtr->WorkMode = ROVER_STATION_MODE;
    Ql_SystemPtr->Debug = 1; //Control the output of some logs

    Ql_EC600U_GnssComSelect = 1;
}

void Ql_Param_Init(void)
{
    Uart_Cfg[0] = Ql_Uart_Cfg(USART0);
    Uart_Cfg[1] = Ql_Uart_Cfg(USART1);
    Uart_Cfg[2] = Ql_Uart_Cfg(USART2);
    Uart_Cfg[3] = Ql_Uart_Cfg(UART3);
    Uart_Cfg[4] = Ql_Uart_Cfg(UART4);
    Uart_Cfg[6] = Ql_Uart_Cfg(UART6);

    Ql_Param_Default();
}

Ql_System_TypeDef       * const Ql_SystemPtr       = (Ql_System_TypeDef       *)&(Ql_Monitor.System);
Ql_CellularInfo_TypeDef * const Ql_CellularInfoPtr = (Ql_CellularInfo_TypeDef *)&(Ql_Monitor.CellularInfo);

#if defined(__EXAMPLE_NTRIP_CLIENT__) || defined(__EXAMPLE_QUECRTK__) || defined(__TEST_QUECRTK__)
void Ql_Cellular_Task(void * Params)
{
    (void) Params;

    CellularError_t status = CELLULAR_SUCCESS;
    static int cell_init_fail_cnt = 0;
    bool ret = true;

    status = Ql_Cellular_Init();
    if(CELLULAR_SUCCESS != status)
    {
        QL_LOG_E("Ql_Cellular_Init failed");
    }

    for( ;; )
    {
        QL_LOG_I("---------Initialize the cellular---------");

        do
        {
            ret = setupCellular();
            if( false == ret )
            {
                if(false == IsCellSimReady)
                {
                    QL_LOG_W("Failed to get simcard status, network services are not supported");
                    break;
                }

                cell_init_fail_cnt++;

                QL_LOG_W( "Cellular init failed (%d) times!", cell_init_fail_cnt );

                if(1 == cell_init_fail_cnt)
                {
                    QL_LOG_W("RESET Cellular!");

                    Cellular_Reboot();
                    vTaskDelay(pdMS_TO_TICKS( 5000 ));
                }
                else if(8 == cell_init_fail_cnt)
                {
                    QL_LOG_W("RESET Cellular!");

                    Cellular_Reboot();
                    vTaskDelay(pdMS_TO_TICKS( 5000 ));
                }
                else if(12 == cell_init_fail_cnt)
                {
                    QL_LOG_W("RESET Cellular!");

                    Cellular_Reboot();
                    vTaskDelay(pdMS_TO_TICKS( 5000 ));
                }
                else if(16 == cell_init_fail_cnt)
                {
                    QL_LOG_E("setupCellular failed!");

                    Cellular_Reboot();
                    vTaskDelay(pdMS_TO_TICKS( 5000 ));
                    cell_init_fail_cnt = 0;
                }
            }
            else
            {
                cell_init_fail_cnt = 0;
                QL_LOG_I("Cellular init OK");

                xEventGroupSetBits(Ql_CellularEvent, CELLULAR_START_NETWORK);
            }
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
        } while( false == ret );

        vTaskSuspend(NULL);
    }
}

void Ql_Network_Task(void * Params)
{
    (void) Params;
    struct tm time = {0};
    char buf[128] = {0};
    EventBits_t wait_bits = 0;
    CellularTime_t cell_time = {0};
    // Ql_GNSS_Cfg_TypeDef *GNSS_Cfg = Ql_GNSS_Cfg();

    wait_bits = xEventGroupWaitBits(Ql_CellularEvent,
                                        CELLULAR_START_NETWORK,
                                        pdTRUE,
                                        pdFALSE,
                                        portMAX_DELAY);

    if(CELLULAR_START_NETWORK & wait_bits)
    {
        if(CELLULAR_SUCCESS == Cellular_GetNetworkTime(CellularHandle,&cell_time))
        {
            cell_time.year += ((cell_time.year >= 70) ? 1900 : 2000);

            snprintf(Ql_SystemPtr->UTC_Time, sizeof(Ql_SystemPtr->UTC_Time),
                        "%0.4d-%0.2d-%0.2dT%0.2d:%0.2d:%0.2d.%d%d%dZ",
                        cell_time.year, cell_time.month, cell_time.day,
                        cell_time.hour, cell_time.minute, cell_time.second,0, 0, 0);

            QL_LOG_I("cell utc [%s]",Ql_SystemPtr->UTC_Time);

            time.tm_year = cell_time.year - 1900;
            time.tm_mon  = cell_time.month - 1;
            time.tm_mday = cell_time.day;
            time.tm_hour = cell_time.hour;
            time.tm_min  = cell_time.minute;
            time.tm_sec  = cell_time.second;
            time.tm_isdst = cell_time.dst;
        
            Ql_RTC_Calibration(&time);
            memset(&time,0,sizeof(time));
            Ql_RTC_Get(&time);
            if (sizeof(buf) > strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &time))
            {
                QL_LOG_I("location rtc time:%s\r\n",buf);
            }
        }
        else
        {
            Ql_SystemPtr->CellSyncRTC = RESET;
        }

        //for handing transport layer errors
        Ql_ManageMultiConnTaskCreate();

        vTaskDelete(NULL);
    }
}
#endif 

static void Ql_Log_Init(void)
{
#if 0
    Ql_Log_Uart_Init("Console", 921600);
#else
    Ql_Uart_Init("Console", USART5, 921600, 4096, 4096);
#endif
    Ql_Log_FuncInit();

    Ql_Printf("\r\n");
    Ql_Printf("   ____   __  __ ______ ______ ______ ______ __ \r\n");
    Ql_Printf("  / __ \\ / / / // ____// ____//_  __// ____// / \r\n");
    Ql_Printf(" / / / // / / // __/  / /      / /  / __/  / /  \r\n");
    Ql_Printf("/ /_/ // /_/ // /___ / /___   / /  / /___ / /___\r\n");
    Ql_Printf("\\___\\_\\\\____//_____/ \\____/  /_/  /_____//_____/\r\n");
    Ql_Printf("\r\n");
    QL_LOG_I("** %s **", Ql_Ver_Get());
}

void Ql_Main_Task(void *param)
{
    while (1)
    {
        Ql_Log_Init();
			
#ifdef __EXAMPLE_GNSS__
    xTaskCreate( Ql_Example_Task,
                 "example task",
                 configMINIMAL_STACK_SIZE * 6,
                 NULL,
                 tskIDLE_PRIORITY + 6,
                 NULL);
#else
    xTaskCreate( Ql_Test_Task,
                 "test task",
                 configMINIMAL_STACK_SIZE * 10,
                 NULL,
                 tskIDLE_PRIORITY + 6,
                 NULL);

#endif

        break;
			
    }

    vTaskDelete(NULL);
}

/*!
    \brief      init task
    \param[in]  pvParameters not used
    \param[out] none
    \retval     none
*/
void Ql_AppStart(void * pvParameters)
{
    Ql_Param_Init();
    Ql_TRNG_Init();
    Ql_RTC_Init();

#if defined(__EXAMPLE_NTRIP_CLIENT__) || defined(__EXAMPLE_QUECRTK__) || defined(__TEST_QUECRTK__)
    cJSON_InitHooks((cJSON_Hooks *)Ql_cJSON_GetContext());

    Ql_CellularEvent = xEventGroupCreateStatic(&StaticCellularEvent);

    xTaskCreate( Ql_Cellular_Task,
                 "cellular",
                 configMINIMAL_STACK_SIZE * 6,
                 NULL,
                 tskIDLE_PRIORITY + 13,
                 &Ql_Cellular_TaskHandle);

    xTaskCreate( Ql_Network_Task,
                 "Network",
                 configMINIMAL_STACK_SIZE * 5,
                 NULL,
                 tskIDLE_PRIORITY + 13,
                 &Ql_Network_TaskHandle);
#endif

    xTaskCreate( Ql_Main_Task,
                 "main task",
                 configMINIMAL_STACK_SIZE * 2,
                 NULL,
                 tskIDLE_PRIORITY + 10,
                 NULL);
}
