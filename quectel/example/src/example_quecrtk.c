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
  Name: example_quecrtk.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Sonia    Create file
*/

#include "example_def.h"

#ifdef __EXAMPLE_QUECRTK__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gd32f4xx_usart.h"
#include "using_plaintext.h"
#include "using_mbedtls.h"
#include "http_parser.h"
#include "core_http_client.h"
#include "libqntrip.h"
#include "ql_ntrip_error.h"
#include "sockets_wrapper.h"
#include "ql_uart.h"
#include "ql_application.h"

#include "ql_log_undef.h"
#define LOG_TAG          "QuecRTK"
#define LOG_LVL          QL_LOG_INFO
#include "ql_log.h"

#define QL_QNTRIP_TASK_PRIO             (tskIDLE_PRIORITY + 9)
#define QL_QNTRIP_STK_SIZE              (configMINIMAL_STACK_SIZE * 20)
#define QNTRIP_HTTP_DISABLE_SNI         (false)
#define QNTRIP_REQ_BUFF_SIZE            (1024U)

#define QNTRIP_ROOT_CA_PEM   \
    "-----BEGIN CERTIFICATE-----\n"\
    "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh"\
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3"\
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD"\
    "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT"\
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j"\
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG"\
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB"\
    "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97"\
    "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt"\
    "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P"\
    "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4"\
    "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO"\
    "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR"\
    "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw"\
    "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr"\
    "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg"\
    "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF"\
    "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls"\
    "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk"\
    "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4="\
    "-----END CERTIFICATE-----"

enum NTRIP_STETA
{
    ENTRIP_DATACALL_STATE  = 0,
    ENTRIP_INIT_STATE,
    ENTRIP_BS_STATUS,
    ENTRIP_AUTH_STATUS,
    ENTRIP_GGA_STATE,
    ENTRIP_NET_STATE,
    ENTRIP_RST_STATE,
    ENTRIP_EXIT_STATE
};

struct NetworkContext
{
    void * pParams;
};

static QueueHandle_t Ntrip_GGA_QueueHandle;
static TransportInterface_t QNtrip_TransInterface;
static NetworkContext_t QNtrip_NetContext = { 0 };
static PlaintextTransportParams_t QNtrip_PlaintextTransParams = { 0 };
static uint8_t  QNtrip_RspBuff[1550 + 1];
static char QNtrip_RtcmBuff[2048] = {0};
static int QNtrip_RtcmBuffSize = sizeof(QNtrip_RtcmBuff);

int socket_link_server(char *server_ip, int port);
int socket_send_msg(int fd, char *msg, int msg_len);
int socket_recv_msg(int fd, char *msg, int msg_len,int timeout);
void socket_close(int fd);
bool Ql_HTTP_Process(char * url,char * req_body,int req_body_len,char *rsp_body,int rsp_body_len);

void *my_malloc(unsigned int size)
{
    return pvPortMalloc(size);
}

void my_free(void *ptr)
{
    vPortFree(ptr);
}

int my_delay(unsigned int microsecond)
{
    vTaskDelay(microsecond);
    return microsecond;
}

int socket_link_server(char *server_ip, int port)
{
    QNtrip_NetContext.pParams = &QNtrip_PlaintextTransParams;

    PlaintextTransportStatus_t xNetworkStatus;

    xNetworkStatus = Plaintext_FreeRTOS_Connect(&QNtrip_NetContext,
                                                 server_ip,
                                                 port,
                                                 3 * 1000,
                                                 3 * 1000);
    LogInfo((" connection to %s:%d,ret[%d]",server_ip,port,xNetworkStatus));

    if(xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS)
    {
        QNtrip_TransInterface.pNetworkContext = &QNtrip_NetContext;
        QNtrip_TransInterface.send = Plaintext_FreeRTOS_send;
        QNtrip_TransInterface.recv = Plaintext_FreeRTOS_recv;

        return 1;
    }
    else
    {
        return -1;
    }
}

int socket_send_msg(int fd, char *msg, int msg_len)
{
    int ret;

    ret = Plaintext_FreeRTOS_send((QNtrip_TransInterface.pNetworkContext),msg,msg_len);

    return ret;
}

int socket_recv_msg(int fd, char *msg, int msg_len,int timeout)
{
    int ret = 0;

    ret = Ql_RecvTcpData(&QNtrip_TransInterface,msg,msg_len);
    QL_LOG_D("TcpRecvMsg:%s,ret:%d\r\n",msg,ret);

    return ret;
}

void socket_close(int fd)
{
    PlaintextTransportStatus_t xNetworkStatus;

    xNetworkStatus = Plaintext_FreeRTOS_Disconnect(&QNtrip_NetContext);//((NetworkContext_t*)fd);
    if(xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS)
    {
        QL_LOG_D("disconnect tcp failed,err[%d]",xNetworkStatus);
    }
}

bool Ql_HTTP_Process(char * url,char * req_body,int req_body_len,char *rspbuff,int rspbuff_len)
{
    unsigned short port;
    char host[32] = {0};
    char path[64] = {0};
    int ret=0;

    NetworkContext_t QNtrip_NetContext = { 0 };
    TlsTransportStatus_t xNetworkStatus;
    TlsTransportParams_t xTlsTransportParams = { 0 };

    QNtrip_NetContext.pParams = &xTlsTransportParams;

    TransportInterface_t QNtrip_TransInterface;
    NetworkCredentials_t xNetworkCredentials = { 0 };

    xNetworkCredentials.disableSni = QNTRIP_HTTP_DISABLE_SNI;
    xNetworkCredentials.pRootCa = (const unsigned char *) QNTRIP_ROOT_CA_PEM;//ca_get();
    xNetworkCredentials.rootCaSize = sizeof(QNTRIP_ROOT_CA_PEM);

    struct http_parser_url u = {0};
    http_parser_url_init(&u);
    ret = http_parser_parse_url((char*)url, strlen(url), 0, &u);
    if(ret != 0)
    {
        QL_LOG_I("http_parser_parse_url failed\r\n");
        return false;
    }

    memset(host,0,32);
    memset(path,0,64);
    memcpy(path,&url[u.field_data[UF_PATH].off],u.field_data[UF_PATH].len);
    memcpy(host,&url[u.field_data[UF_HOST].off],u.field_data[UF_HOST].len);
    port = u.port;

    QL_LOG_D("path[%s],host[%s],port[%d]",path,host,port);

    xNetworkStatus = TLS_FreeRTOS_Connect(&QNtrip_NetContext,
                                           host,
                                           port,
                                           &xNetworkCredentials,
                                           10 * 1000,
                                           10 * 1000);

    if(xNetworkStatus != TLS_TRANSPORT_SUCCESS)
    {
        return false;
    }

    QNtrip_TransInterface.pNetworkContext = &QNtrip_NetContext;
    QNtrip_TransInterface.send = TLS_FreeRTOS_send;
    QNtrip_TransInterface.recv = TLS_FreeRTOS_recv;

    unsigned short RequestBuff[QNTRIP_REQ_BUFF_SIZE];

    HTTPRequestHeaders_t xRequestHeaders;
    (void) memset(&xRequestHeaders, 0, sizeof(xRequestHeaders));
    xRequestHeaders.pBuffer = RequestBuff;
    xRequestHeaders.bufferLen = sizeof(RequestBuff);

    HTTPRequestInfo_t xRequestInfo;
    (void) memset(&xRequestInfo, 0, sizeof(xRequestInfo));

    xRequestInfo.pHost = host;
    xRequestInfo.hostLen = strlen(host);
    xRequestInfo.pMethod = HTTP_METHOD_POST;
    xRequestInfo.methodLen = strlen(HTTP_METHOD_POST);
    xRequestInfo.pPath = path;
    xRequestInfo.pathLen = strlen(path);
    xRequestInfo.reqFlags |= HTTP_REQUEST_KEEP_ALIVE_FLAG;

    QL_LOG_D("Sending HTTP %.*s request to %.*s%.*s...",
               (int) xRequestInfo.methodLen, xRequestInfo.pMethod,
               (int) xRequestInfo.hostLen, xRequestInfo.pHost,
               (int) xRequestInfo.pathLen, xRequestInfo.pPath);

    HTTPResponse_t xResponse;
    (void) memset(&xResponse, 0, sizeof(xResponse));

    char * temp_rsp_buff = (char *)pvPortMalloc(QNTRIP_REQ_BUFF_SIZE);
    if(NULL == temp_rsp_buff)
    {
        TLS_FreeRTOS_Disconnect(&QNtrip_NetContext);
        return false;
    }
    memset(temp_rsp_buff,0,QNTRIP_REQ_BUFF_SIZE);

    xResponse.pBuffer = temp_rsp_buff;
    xResponse.bufferLen = QNTRIP_REQ_BUFF_SIZE;

    HTTPStatus_t xHTTPStatus = HTTPSuccess;
    xHTTPStatus = HTTPClient_InitializeRequestHeaders(&xRequestHeaders,&xRequestInfo);

    if(xHTTPStatus == HTTPSuccess)
    {
        HTTPClient_AddHeader(&xRequestHeaders,
                            "Content-Type",strlen("Content-Type"),
                            "application/json",strlen("application/json"));

        xHTTPStatus = HTTPClient_Send(&QNtrip_TransInterface,
                                       &xRequestHeaders,
                                       req_body,
                                       req_body_len,
                                       &xResponse,
                                       0);
    }

    TLS_FreeRTOS_Disconnect(&QNtrip_NetContext);

    if(xHTTPStatus == HTTPSuccess)
    {
        int copy_len = ((xResponse.bodyLen <= rspbuff_len) ? xResponse.bodyLen: rspbuff_len);

        memcpy(rspbuff,xResponse.pBody,copy_len);

        if(NULL != temp_rsp_buff)
        {
            vPortFree(temp_rsp_buff);
        }

        return true;
    }
    else
    {
        QL_LOG_E("Failed to send HTTP %.*s request to %.*s%.*s: Error=%s.",
                    (int) xRequestInfo.methodLen, xRequestInfo.pMethod,
                    (int) xRequestInfo.hostLen, xRequestInfo.pHost,
                    (int) xRequestInfo.pathLen, xRequestInfo.pPath,
                    HTTPClient_strerror(xHTTPStatus));

        if(NULL != temp_rsp_buff)
        {
            vPortFree(temp_rsp_buff);
        }

        return false;
    }

}

void Ql_Qntrip_Task(void)
{
    char gga_msg[BUFFSIZE256 + 1];
    int len;
    int ret = -1;
    int ql_ntrip_state = ENTRIP_INIT_STATE;//ENTRIP_DATACALL_STATE;

    //Please input correct deviceID and licenseKey
    char device_id[] = "XXXXXXXXXXXXXXX";
    char license_key[] = "XXXXXXXX";

    Mem_Hooks hook = {my_malloc,my_free};
    Time_Hooks time_hook = {my_delay};

    QL_LOG_I("get sdk version:%s\r\n",Ql_GetSDK_Version());

    while(1)
    {
        QL_LOG_I("current status:%d\r\n",ql_ntrip_state);
        switch (ql_ntrip_state)
        {
            case ENTRIP_DATACALL_STATE:
            {
                //
            }
            break;
            case ENTRIP_INIT_STATE:
            {
                /*init */
                Ql_Ntrip_SetSystemAPI(&hook,&time_hook,Ql_HTTP_Process);
                Ql_BS_Init("460",device_id,NULL,license_key);
                Ql_Ntrip_ClientInit(0,QL_SOCKET_REVRTCM_BUFFER_SIZE);

                QL_LOG_I("bootstrap init\r\n");
                ql_ntrip_state = ENTRIP_BS_STATUS;
            }
            break;
            case ENTRIP_BS_STATUS:
            {
                ret = Ql_BS_Auth();
                if (ret == QL_NTRIP_ERR_TIMEOUT)
                {
                    QL_LOG_I("error:bootstrap fail,ret:%d.retry...\r\n",ret);
                    vTaskDelay(2000);
                    continue;
                }
                else if(ret != QL_NTRIP_OK)
                {
                    ql_ntrip_state = ENTRIP_RST_STATE;
                    QL_LOG_I("error:bootstrap fail,ret:%d.Please input correct deviceID and licenseKey\r\n",ret);
                    break;
                }

                ret = Ql_Ntrip_SetSocketAPI(socket_link_server,socket_recv_msg,socket_send_msg,socket_close);
                if (ret != QL_NTRIP_OK)
                {
                    ql_ntrip_state = ENTRIP_EXIT_STATE;
                    QL_LOG_I("error:Ql_Ntrip_SetSocketAPI fail,ret:%d.exit...\r\n",ret);
                    break;
                }
                
                ret = Ql_Ntrip_ClientStart();
                if (ret != QL_NTRIP_OK)
                {
                    ql_ntrip_state = ENTRIP_EXIT_STATE;
                    QL_LOG_I("error:Ql_Ntrip_ClientStart fail,ret:%d.exit...\r\n",ret);
                    break;
                }
                ql_ntrip_state = ENTRIP_AUTH_STATUS;
            }
            break;
            case ENTRIP_AUTH_STATUS:
            {
                QL_LOG_I("[Enter] ENTRIP_AUTH_STATUS \r\n");
                ret = Ql_Ntrip_ClientAuth();
                if (ret != QL_NTRIP_OK)
                {
                    ql_ntrip_state = ENTRIP_RST_STATE;
                    QL_LOG_I("error:ENTRIP_AUTH_STATUS fail,ret:%d. go to data call state\r\n",ret);
                    break;
                }

                QL_LOG_I("Ql_Ntrip_ClientAuth,ret:%d.\r\n",ret);
                ql_ntrip_state = ENTRIP_GGA_STATE;
            }
                break;
            case ENTRIP_GGA_STATE:
            {
                if(1 == xQueueReceive(Ntrip_GGA_QueueHandle, gga_msg, 50 / portTICK_RATE_MS))
                {
                    QL_LOG_I("GGA frame:%s",gga_msg);

                    ret = Ql_Ntrip_UploadGGA(gga_msg, strlen(gga_msg));
                    if (ret == strlen(gga_msg))
                    {
                        QL_LOG_I("Upload GGA success\r\n");
                    }
                    else
                    {
                        QL_LOG_I("Upload GGA fail\r\n");
                        ql_ntrip_state = ENTRIP_RST_STATE;
                        break;
                    }
                }
                else
                {
                    QL_LOG_I("no gga");
                }

                ql_ntrip_state = ENTRIP_NET_STATE;
            }
            break;
            case ENTRIP_NET_STATE:
            {
                memset(QNtrip_RspBuff,0,sizeof(QNtrip_RspBuff));
                memset(QNtrip_RtcmBuff,0,sizeof(QNtrip_RtcmBuff));

                ret = Ql_Ntrip_RecvData(QNtrip_RspBuff,sizeof(QNtrip_RspBuff),100);
                if (ret == 0)
                {   //timeout or read nothing
                    ql_ntrip_state = ENTRIP_GGA_STATE;
                    break;
                }
                else if (ret < 0)
                {   //tcp disconnect,try again
                    ql_ntrip_state = ENTRIP_RST_STATE;
                    QL_LOG_I("error:tcp disconnect,ret:%d. go to ENTRIP_RST_STATE\r\n",ret);
                    break;
                }
    
                ret = Ql_Ntrip_GetRtcmData(QNtrip_RspBuff,ret,QNtrip_RtcmBuff,&QNtrip_RtcmBuffSize);
                if (ret == QL_NTRIP_ERR_VENDOR1_ACCOUNT_KICKING_ERROR)
                {
                    QL_LOG_I("login on other terminal :%x\n",ret);
                    ql_ntrip_state = ENTRIP_EXIT_STATE;
                    break;
                }
                else if(ret != QL_NTRIP_OK)
                {
                    QL_LOG_I("Ql_Ntrip_GetRtcmData error:%x\n",ret);
                }

                Ql_Uart_Open(UART3, portMAX_DELAY);
                len = Ql_Uart_Write(UART3, QNtrip_RtcmBuff, QNtrip_RtcmBuffSize, 0);
                Ql_Uart_Release(UART3);

                QL_LOG_I("write RTCM to uart:%d\r\n",len);

                ql_ntrip_state = ENTRIP_GGA_STATE;
            }
            break;
            case ENTRIP_RST_STATE:
            {
                Ql_Ntrip_ClientClose();
                ql_ntrip_state = ENTRIP_INIT_STATE;
                vTaskDelay(2000);
            }
            break;
            case ENTRIP_EXIT_STATE:
            {
                QL_LOG_I("QNTRIP_SDK exit succeed, byebye!\r\n");
                vTaskDelay(2000);
            }
            break;
            default:
                break;
        }
    }
}

void Ql_StartQNtrip_Task(void)
{
    static bool is_created = false;

    if(is_created == false)
    {
        xTaskCreate(Ql_Qntrip_Task,
                     "quec qntrip",
                     QL_QNTRIP_STK_SIZE,
                     NULL,
                     QL_QNTRIP_TASK_PRIO,
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

    Ql_StartQNtrip_Task();

    while(1)
    {
        Length = Ql_Uart_Read(UART3, Buffer, 8192, 500);
        QL_LOG_D("received NMEA from UART3, length: %d", Length);
        Buffer[Length] = '\0';

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
                if(1 != xQueueOverwrite(Ntrip_GGA_QueueHandle, GGA_Msg))
                {
                    QL_LOG_E("GGA Send to Queue Fail");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif
