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
  Name: example_lg69tap_iic_nmea.c
  Current support: LG69TAP
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Sem      Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_LG69TAP_IIC_NMEA__

#include "FreeRTOS.h"
#include "task.h"

//-----include header file-----
#include "ql_delay.h"
#include "ql_iic.h"

#define LOG_TAG "lg69tap"
#define LOG_LVL QL_LOG_INFO
// #define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

//-----macro define -----
#ifndef false
#define false                                      (0U)
#endif

#ifndef true
#define true                                       (0U)
#endif

#define NMEA_BUF_SIZE                              (4096U)
#define QL_LG69TAP_IIC_INVAILD_BYTE                (0xFF)
#define QL_LG69TAP_IIC_SLAVE_ADDR                  (0x50 >> 1)                    // 7 bits address 0101 000x, x=0(w) or 1(R)
#define QL_LG69TAP_IIC_MAX_READ_BUFFER             (1 * 256)                      // the iic max read buff, when the buff is full, the process should be return
#define QL_LG69TAP_IIC_DEFAULT_REG_ADDR            (0U)
#define MAX_ERROR_NUMBER                           (3U)
#define QL_LG69TAP_IIC_DELAY                       (15U)                          // unit ms
#define I2C_TX_SIZE                                (2 * 1024U)
#define I2C_RX_SIZE                                (2 * 1024U)

//-----static global variable-----
static SemaphoreHandle_t IIC_Port_Sem;

//-----static function declare-----
static void Ql_LG69TAP_IIC_ErrHandle(Ql_IIC_Status_TypeDef IIC_State);
static int32_t Ql_LG69TAP_IIC_ReadData(uint8_t *Data, uint32_t *Size, uint32_t Timeout);
static int32_t Ql_LG69TAP_IIC_WriteData(uint8_t *Data, uint16_t DataLength, uint32_t Timeout);
static int32_t Ql_LG69TAP_PortIicInit(void *Param);
static int32_t Ql_LG69TAP_PortIicDeInit(void);
static uint32_t Ql_LG69TAP_IIC_GetValidBuffer(uint8_t *Buffer, uint32_t Size, uint8_t Invalid_Char);

//-----function define-----

/*******************************************************************************
* Name: static void Ql_LG69TAP_IIC_ErrHandle(Ql_IIC_Status_TypeDef IIC_State)
* Brief: LG69TAP serial module iic port error handle
* Input: 
*   Param: iic status
* Output:
*   void
* Return:
*   void
*******************************************************************************/
static void Ql_LG69TAP_IIC_ErrHandle(Ql_IIC_Status_TypeDef IIC_State)
{
    switch(IIC_State)
    {
        case QL_IIC_STATUS_OK:
            QL_LOG_I("LG69TAP module iic port r/w ok.");
            break;
        case QL_IIC_STATUS_BUSY:
            Ql_IIC_DeInit();
            Ql_IIC_CheckBusSatus();
            vTaskDelay(pdMS_TO_TICKS(QL_LG69TAP_IIC_DELAY));
            Ql_IIC_Init(IIC_MODE_HW0_INTERRUPT, IIC_SPEED_FAST, I2C_TX_SIZE, I2C_RX_SIZE);
            QL_LOG_E("LG69TAP module iic port busy and re-init.");
            break;
        case QL_IIC_STATUS_TIMEOUT:
            QL_LOG_E("LG69TAP module iic port r/w timeout.");
            break;
        case QL_IIC_STATUS_NOACK:
            vTaskDelay(pdMS_TO_TICKS(QL_LG69TAP_IIC_DELAY));
            Ql_IIC_CheckBusSatus();
            QL_LOG_E("LG69TAP module iic port r/w no ack.");
            break;
        case QL_IIC_STATUS_SEM_ERROR:
            QL_LOG_E("LG69TAP module iic port semaphore error.");
            break;
        case QL_IIC_STATUS_MODE_ERROR:
            QL_LOG_E("LG69TAP module iic port mode error.");
            break;
        case QL_IIC_STATUS_MEMORY_ERROR:
            QL_LOG_E("LG69TAP module iic port memory error.");
            break;
        default:
            QL_LOG_W("LG69TAP module iic port unknown state:%d",IIC_State);
            break;
    }
}

/*******************************************************************************
* Name: int32_t Ql_LG69TAP_IIC_ReadData(uint8_t *Data, uint32_t *Size, uint32_t Timeout)
* Brief: read data from LG69TAP module iic port
* Input: 
*   Param: 
*       Data: data buffer
*       Size: data buffer size
*       Timeout: timeout
* Output:
*   void
* Return:
*   sucess: read date size.
*   fail: -1
*******************************************************************************/
static int32_t Ql_LG69TAP_IIC_ReadData(uint8_t *Data, uint32_t *Size, uint32_t Timeout)
{
    uint32_t read_size = *Size;
    int32_t ret = -1;
    uint32_t valid_data_size = 0;
    Ql_IIC_Status_TypeDef iic_state = QL_IIC_STATUS_OK;
    BaseType_t xReturn;

    *Size = 0;
    xReturn = xSemaphoreTake(IIC_Port_Sem, Timeout);
    if (xReturn == pdFAIL)
    {
        QL_LOG_E("LG69TAP iic read semaphore take timeout!");
        xSemaphoreGive(IIC_Port_Sem);
        return ret;
    }

    memset(Data, 0, read_size);
    read_size = QL_LG69TAP_IIC_MAX_READ_BUFFER;
    iic_state = Ql_IIC_Read(QL_LG69TAP_IIC_SLAVE_ADDR << 1, QL_LG69TAP_IIC_DEFAULT_REG_ADDR, Data, read_size, Timeout);
    if(QL_IIC_STATUS_OK == iic_state)
    {
        valid_data_size = Ql_LG69TAP_IIC_GetValidBuffer(Data, read_size, QL_LG69TAP_IIC_INVAILD_BYTE);

        *Size = valid_data_size;
        ret = valid_data_size;
    }
    else
    {
        QL_LOG_E("read data fail.");
        Ql_LG69TAP_IIC_ErrHandle(iic_state);
    }

    xSemaphoreGive(IIC_Port_Sem);
    return ret;
}

/*******************************************************************************
* Name: int32_t Ql_LG69TAP_IIC_WriteData(uint8_t *Data, uint16_t DataLength, uint32_t Timeout)
* Brief: write data to LG69TAP module iic port
* Input: 
*   Param: 
*       Data: data buffer
*       DataLength: data buffer length
*       Timeout: timeout
* Output:
*   void
* Return:
*   sucess: write data size.
*   fail: -1
*******************************************************************************/
static int32_t Ql_LG69TAP_IIC_WriteData(uint8_t *Data, uint16_t DataLength, uint32_t Timeout)
{
    int32_t ret = -1;
    Ql_IIC_Status_TypeDef status;
    BaseType_t xReturn;

    xReturn = xSemaphoreTake(IIC_Port_Sem, Timeout);
    if (xReturn == pdFAIL)
    {
        QL_LOG_W("LG69TAP iic write semaphore take timeout!");
        xSemaphoreGive(IIC_Port_Sem);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(QL_LG69TAP_IIC_DELAY));
    status = Ql_IIC_Write(QL_LG69TAP_IIC_SLAVE_ADDR << 1, QL_LG69TAP_IIC_DEFAULT_REG_ADDR, (uint8_t *)Data, DataLength, Timeout);
    if(QL_IIC_STATUS_OK == status)
    {
        QL_LOG_D("write data succeed.");
    }
    else 
    {
        QL_LOG_E("write data fail.");
        Ql_LG69TAP_IIC_ErrHandle(status);
    }

    xSemaphoreGive(IIC_Port_Sem);
    return ret;
}

/*******************************************************************************
* Name: static int32_t Ql_LG69TAP_PortIicInit(void *Param)
* Brief: Initialize the iic port of the LG69TAP GNSS module
* Input: 
*   Param: struct of GNSS_TEA_TypeDef
* Output:
*   void
* Return:
*   0: sucess    -1:fail
*******************************************************************************/
static int32_t Ql_LG69TAP_PortIicInit(void *Param)
{
    int32_t ret = -1;

    ret = Ql_IIC_Init(IIC_MODE_HW0_INTERRUPT, IIC_SPEED_FAST, I2C_TX_SIZE, I2C_RX_SIZE);

    IIC_Port_Sem  = xSemaphoreCreateBinary();
    if (IIC_Port_Sem == NULL)
    {
        return -1;
    }
    xSemaphoreGive(IIC_Port_Sem);

    QL_LOG_D("IicInit %d\r\n", ret);
    return ret;
}

/*******************************************************************************
* Name: static int32_t Ql_LG69TAP_PortIicDeInit(void)
* Brief: Deinitialize the iic port of the GNSS module
* Input: 
*   void
* Output:
*   void
* Return:
*   int32_t: 0: sucess    -1:fail
*******************************************************************************/
static int32_t Ql_LG69TAP_PortIicDeInit(void)
{
    int32_t ret = -1;

    if(NULL != IIC_Port_Sem)
    {
        vSemaphoreDelete(IIC_Port_Sem);
        IIC_Port_Sem = NULL;
    }

    ret = 0;
    return ret;
}

/*******************************************************************************
* Name: static uint32_t Ql_LG69TAP_IIC_GetValidBuffer(uint8_t *Buffer, uint32_t Size, uint8_t Invalid_Char)
* Brief: Get valid data and size from the iic buffer
* Input: 
*   Buffer: the iic buffer
*   Size: the size of iic buffer
*   Invalid_Char: the invalid char that need to be remove
* Output:
*   Buffer: valid data
* Return:
*   valid data size
*******************************************************************************/
static uint32_t Ql_LG69TAP_IIC_GetValidBuffer(uint8_t *Buffer, uint32_t Size, uint8_t Invalid_Char)
{
    uint32_t valid_data_size = 0;

    for(int i = 0; i < Size; i++)
    {
        if(Buffer[i] == Invalid_Char)
        {
            // ingore invalid byte
        }
        else
        {
            Buffer[valid_data_size] = Buffer[i];
            valid_data_size++;
        }
    }

    return valid_data_size;
}

void Ql_Example_Task(void *Param)
{
    static uint8_t rx_buf[NMEA_BUF_SIZE] = {0};
    char* tx_data = "$PQTMVERNO*58\r\n";
    int32_t Length = 0;
    int count = 0;
    (void)Param;
    
    QL_LOG_I("--->LG69TAP iic nmea<---");

    Ql_LG69TAP_PortIicInit(NULL);

    while (1)
    {
        Length = NMEA_BUF_SIZE;
        Ql_LG69TAP_IIC_ReadData(rx_buf, &Length, 100);
        QL_LOG_I("read data from iic, len: %d", Length);

        if (Length <= 0)
        {
            continue;
        }
        rx_buf[Length] = '\0';

        QL_LOG_I("\r\n%s",rx_buf);

        vTaskDelay(pdMS_TO_TICKS(10));

        if (count >= 50)
        {
            QL_LOG_I("send cmd to iic port:%s",tx_data);
            Ql_LG69TAP_IIC_WriteData(tx_data, strlen(tx_data), 100);
            count = 0;
        }
        count++;
    }
}
#endif
