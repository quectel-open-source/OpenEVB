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
  Name: example_ntrip_client.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Sonia    Create file
*/

#include "example_def.h"

#ifdef __EXAMPLE_NTRIP_CLIENT__
#include "using_plaintext.h"
#include "sockets_wrapper.h"
#include "ql_trng.h"
#include "ql_uart.h"
#include "ql_application.h"

#include "ql_log_undef.h"
#define LOG_TAG "NClt"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

#define NTRIP_TASK_PRIO                        (tskIDLE_PRIORITY + 9)
#define NTRIP_STK_SIZE                         (configMINIMAL_STACK_SIZE * 8)

#define HTTP_USER_AGENT_VALUE                  "QNTRIP Quectel-GNSS"

// ntrip server info
#define NTRIP_SERVER_HOST                      "xxx.xxx.xxx.xxx" // IP address
#define NTRIP_SERVER_PORT                      (0000)            // port
#define NTRIP_SERVER_USERNAME                  "XXXXXX"          // username
#define NTRIP_SERVER_PWD                       "XXXXXX"          // password
#define NTRIP_SERVER_MOUNTPOINT                "XXXX"            // mountpoint


/* timeout for transport send and receive */
#define NTRIP_CLI_TRANSPORT_SEND_TIMEOUT_MS    (2000U)
#define NTRIP_CLI_TRANSPORT_RECV_TIMEOUT_MS    (5000U)

struct NetworkContext
{
    void * pParams;
};

typedef struct
{
    char     Host[BUFFSIZE32 + SIZEOF_CHAR_NUL];
    uint32_t Port;
    char     Username[BUFFSIZE32];
    char     Pwd[BUFFSIZE32];
    char     MountPoint[BUFFSIZE32];
} Ql_NtripClient_TypeDef;

static uint8_t NtripClientBuffer[BUFFSIZE1550 + SIZEOF_CHAR_NUL];
static QueueHandle_t Ntrip_GGA_QueueHandle = NULL;
static EventGroupHandle_t Ql_NtripClientEvent = NULL;

static bool Ql_ConnectRtkServer(NetworkContext_t * NetworkContextPtr,Ql_NtripClient_TypeDef *NtripClientPtr)
{
    int32_t ret = true;
    PlaintextTransportStatus_t status = PLAINTEXT_TRANSPORT_SUCCESS;

    if(NULL == NetworkContextPtr)
    {
        QL_LOG_E("NetworkContextPtr is NULL");
        return false;
    }

    QL_LOG_I("connect to [%s:%d]",NtripClientPtr->Host,NtripClientPtr->Port);

    status = Plaintext_FreeRTOS_Connect(NetworkContextPtr,
                                        NtripClientPtr->Host,
                                        NtripClientPtr->Port,
                                        NTRIP_CLI_TRANSPORT_RECV_TIMEOUT_MS,
                                        NTRIP_CLI_TRANSPORT_SEND_TIMEOUT_MS);

    if(PLAINTEXT_TRANSPORT_SUCCESS != status)
    {
        ret = false;
        QL_LOG_E("connect failed");
    }

    return ret;
}

static void Ql_NtripClientCloseRtkLink(NetworkContext_t *NetContext)
{
    Ql_GetCellularActiveStatus();

    QL_LOG_I(("close the connection"));
    Plaintext_FreeRTOS_Disconnect(NetContext);

    return;
}

static bool Ql_NtripClientLogin(TransportInterface_t *TransportInterfacePtr,Ql_NtripClient_TypeDef *NtripClientPtr)
{
    bool ret = false;
    int32_t length = 0;
    char basic[BUFFSIZE64] = {0};

    if((strlen(NtripClientPtr->Username) <= 0)
        || (strlen(NtripClientPtr->Pwd) <= 0)
        || (strlen(NtripClientPtr->MountPoint) <= 0))
    {
        QL_LOG_E("params error,User[%s],Pwd[%s],Mnt[%s]",
            NtripClientPtr->Username,NtripClientPtr->Pwd,NtripClientPtr->MountPoint);
        return false;
    }

    Ql_Base64_Encode(basic, sizeof(basic),NtripClientPtr->Username, NtripClientPtr->Pwd);

    length = snprintf((char *)NtripClientBuffer, sizeof(NtripClientBuffer),
                "GET /%s HTTP/1.0\r\n"
                "User-Agent: %s\r\n"
                "Host: %s:%u\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n"
                "Authorization: Basic %s\r\n\r\n",
                NtripClientPtr->MountPoint,
                HTTP_USER_AGENT_VALUE, 
                NtripClientPtr->Host,NtripClientPtr->Port,
                basic);

    QL_LOG_I("login buff:\r\n%.*s", length,NtripClientBuffer);
    ret = Ql_SendTcpData(TransportInterfacePtr, NtripClientBuffer, length);
    if(ret == true)
    {
        ret = false;
        length = Ql_RecvTcpData(TransportInterfacePtr, NtripClientBuffer,sizeof(NtripClientBuffer));
        if(length > 0)
        {
            QL_LOG_I("rsp:\r\n%.*s",length,NtripClientBuffer);
            if(NULL != strstr((const char *)NtripClientBuffer, (const char *)"ICY 200 OK"))
            {
                QL_LOG_I("login OK");
                ret = true;
            }
            else
            {
                QL_LOG_E("login failed");
            }
        }
        else
        {
            QL_LOG_E("No rsp,check the ntrip caster config");
        }
    }
    else
    {
        QL_LOG_E("send login req faild");
    }

    return ret;
}

static void NtripClient_Task(void * Paras)
{
    (void)Paras;

    PlaintextTransportParams_t plaintext_transport_params = {0};
    NetworkContext_t net_context = {0};
    TransportInterface_t transport_interface = {0};

    EventBits_t wait_bits = 0;
    char gga_msg[BUFFSIZE256 + 1];
    bool ret = true;
    int32_t length = 0;
    Ql_NtripClient_TypeDef ntripclient_info = {0};

    /* Set the pParams member of the network context with desired transport. */
    net_context.pParams = &plaintext_transport_params;

    strcpy(ntripclient_info.Host,       NTRIP_SERVER_HOST);
    ntripclient_info.Port =             NTRIP_SERVER_PORT;
    strcpy(ntripclient_info.Username,   NTRIP_SERVER_USERNAME);
    strcpy(ntripclient_info.Pwd,        NTRIP_SERVER_PWD);
    strcpy(ntripclient_info.MountPoint, NTRIP_SERVER_MOUNTPOINT);

    for(;;)
    {
        wait_bits = xEventGroupWaitBits(Ql_NtripClientEvent,
                                        (NTRIP_RTK_EVENT_CONN | NTRIP_RTK_EVENT_RECONN | NTRIP_RTK_EVENT_CLOSE),
                                        pdTRUE,
                                        pdFALSE,
                                        portMAX_DELAY);
        if((wait_bits & (NTRIP_RTK_EVENT_CONN | NTRIP_RTK_EVENT_RECONN  | NTRIP_RTK_EVENT_CLOSE)) == 0)
        {
            QL_LOG_E("EventGroup Wait Bits fails[0x%02x]",wait_bits);
            continue;
        }

        if((wait_bits & NTRIP_RTK_EVENT_RECONN) != 0)
        {
            wait_bits = NTRIP_RTK_EVENT_RECONN;
        }

        QL_LOG_I("----- NtripCLI WaitBits[0x%x] -----",wait_bits);
        switch(wait_bits)
        {
            case NTRIP_RTK_EVENT_CONN:
            case NTRIP_RTK_EVENT_RECONN:
            {
                if(NTRIP_RTK_EVENT_RECONN == wait_bits)
                {
                    Ql_NtripClientCloseRtkLink(&net_context);
                }

                while (false == Ql_SystemPtr->CellularNetReg)
                {
                    QL_LOG_I("Waiting for Network");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }

                if((strlen(ntripclient_info.Host) <= 0) || (ntripclient_info.Port <= 0))
                {
                    QL_LOG_E("params error,host [%s],port [%d]",ntripclient_info.Host,ntripclient_info.Port);

                    xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_CLOSE);
                    break;
                }

                ret = Ql_ConnectRtkServer(&net_context,&ntripclient_info);
                if(ret == true)
                {
                    /* Define the transport interface. */
                    transport_interface.pNetworkContext = &net_context;
                    transport_interface.send = Plaintext_FreeRTOS_send;
                    transport_interface.recv = Plaintext_FreeRTOS_recv;
                }
                else
                {
                    QL_LOG_E("Failed to connect to ntrip caster");
                    vTaskDelay(pdMS_TO_TICKS(3000));

                    xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_RECONN);
                    break;
                }

                ret = Ql_NtripClientLogin(&transport_interface,&ntripclient_info);
                if(true == ret)
                {
                    QL_LOG_I("Receiving rtcm & sending GGA...");
                    while(1)
                    {
                        do
                        {
                            length = Ql_RecvTcpData(&transport_interface, NtripClientBuffer,sizeof(NtripClientBuffer));
                            if(length > 0)
                            {
                                ret = true;

                                Ql_Uart_Open(UART3, portMAX_DELAY);
                                Ql_Uart_Write(UART3, NtripClientBuffer, length, 0);
                                Ql_Uart_Release(UART3);

                                QL_LOG_I("Get Rtcm data,len: %d", length);

                                if (Ql_SystemPtr->Debug)
                                {
                                    for (uint32_t i = 0; i < length; i++)
                                    {
                                        if (i > 0 && i % 32 == 0)
                                        {
                                            Ql_Printf("\r\n");
                                        }
                                        Ql_Printf("%02X ", NtripClientBuffer[i]);
                                    }
                                    Ql_Printf("\r\n");
                                }
                            }
                            else if(length == 0)
                            {
                                ret = true;
                                QL_LOG_I("No Rtcm data was read");
                            }
                            else
                            {
                                ret = false;
                                QL_LOG_W("Read data failed, err[%d]", length);
                            }
                        } while (CELLULAR_MAX_RECV_DATA_LEN == length);

                        if(true != ret)
                        {
                            QL_LOG_E("NtripClient recv failed!");
                            xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_RECONN);
                            break;
                        }

                        ret = xQueueReceive(Ntrip_GGA_QueueHandle, gga_msg, pdMS_TO_TICKS(200));
                        if(true == ret)
                        {
                            QL_LOG_I("GGA send: %s", gga_msg);

                            length = sprintf((char *)NtripClientBuffer, "%s\r\n", gga_msg);
                            ret = Ql_SendTcpData(&transport_interface, NtripClientBuffer, length);
                            if(true != ret)
                            {
                                QL_LOG_E("send gga failed,exit!");
                                xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_RECONN);
                                break;
                            }
                        }
                        else
                        {
                            QL_LOG_I("no gga data,waiting");
                        }
                    }
                }
                else
                {
                    //login failed
                    xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_CLOSE);
                }
            }
            break;
            case NTRIP_RTK_EVENT_CLOSE:
            {
                Ql_NtripClientCloseRtkLink(&net_context);
                QL_LOG_I("Close the ntrip client,del the task");

                vTaskDelete(NULL);
            }
            break;
            default:
            break;
        }
    }
}

static void Ql_NtripClient_TaskStart(void)
{
    static bool is_created = false;

    if(NULL == Ql_NtripClientEvent)
    {
        Ql_NtripClientEvent = xEventGroupCreate();
    }

    if(false == is_created)
    {
        xTaskCreate(NtripClient_Task,
                    "Ntrip Client",
                    NTRIP_STK_SIZE,
                    NULL,
                    NTRIP_TASK_PRIO,
                    NULL);

        is_created = true;
    }
}

void Ql_Example_Task(void *Param)
{
    uint8_t *Buffer = pvPortMalloc(8192);
    char GGA_Msg[256 + 1];
    int32_t Length = 0,size = 0;
    char* pgga_header = NULL;
    char* pgga_end = NULL;

    (void)Param;

    Ntrip_GGA_QueueHandle = xQueueCreate(1, 256 + 1);
    if (Ntrip_GGA_QueueHandle == NULL)
    {
        while(1)
        {
            QL_LOG_E("Ntrip GGA Queue Create Fail");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    Ql_Uart_Init("GNSS COM1", UART3, 115200, 8192, 2048);

    while (false == Ql_SystemPtr->CellularNetReg)
    {
        QL_LOG_I("Waiting for Cellular Network Registration");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Ql_NtripClient_TaskStart();
    xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_CONN);

    while(1)
    {
        Length = Ql_Uart_Read(UART3, Buffer, 8192, 500);
        Buffer[Length] = '\0';
        QL_LOG_D("received NMEA from UART3, length: %d", Length);

        pgga_header = strstr((char *)Buffer, "$GNGGA");
        if(NULL == pgga_header)
        {
            pgga_header = strstr((char *)Buffer, "$GPGGA");
        }

        if(NULL == pgga_header)
        {
            pgga_header = strstr((char *)Buffer, "$GBGGA");
        }

        if(NULL != pgga_header)
        {
            pgga_end = strstr(pgga_header, "\r\n");
            if(NULL != pgga_end)
            {
                pgga_end += 2;
                *pgga_end = '\0';
                size = (strlen(pgga_header) > sizeof(GGA_Msg)) ? sizeof(GGA_Msg) : strlen(pgga_header)+1;
                memcpy(GGA_Msg, pgga_header,size);
                xQueueOverwrite(Ntrip_GGA_QueueHandle, GGA_Msg);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}
#endif

