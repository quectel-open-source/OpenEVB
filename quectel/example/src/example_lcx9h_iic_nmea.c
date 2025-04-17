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
  Name: example_lcx9h_iic_nmea.c
  Current support: LC29HAA, LC29HAI, LC29HBA, LC29HCA, LC29HDA, LC79HAL, L89(HA), L89(HD)
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hayden   Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_LCx9H_IIC_NMEA__
//-----include header file-----
#include "FreeRTOS.h"
#include "task.h"
#include "ql_delay.h"
#include "ql_iic.h"
#include "stdbool.h"

#define LOG_TAG "iic_nmea"
#define LOG_LVL QL_LOG_INFO
// #define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

#define NMEA_BUF_SIZE          (4096U)

//-----macro define -----
#define QL_LCx9H_IIC_SLAVE_CR_CMD                0xAA51ul
#define QL_LCx9H_IIC_SLAVE_CW_CMD                0xAA53ul
#define QL_LCx9H_IIC_SLAVE_CMD_LEN               8
#define QL_LCx9H_IIC_SLAVE_TX_LEN_REG_OFFSET     0x08
#define QL_LCx9H_IIC_SLAVE_TX_BUF_REG_OFFSET     0x2000
#define QL_LCx9H_IIC_SLAVE_RX_LEN_REG_OFFSET     0x04
#define QL_LCx9H_IIC_SLAVE_RX_BUF_REG_OFFSET     0x1000
#define QL_LCx9H_IIC_SLAVE_ADDR_CR_OR_CW         0x50
#define QL_LCx9H_IIC_SLAVE_ADDR_R                0x54
#define QL_LCx9H_IIC_SLAVE_ADDR_W                0x58

#define MAX_ERROR_NUMBER                         3
#define QL_LCx9H_IIC_DELAY                       15                            //unit ms

#define QL_LCx29H_IIC_MODE                           (IIC_MODE_HW0_POLLING)
#define QL_LCx29H_IIC_SPEED                          (IIC_SPEED_FAST)
#define QL_LCx29H_IIC_TX_SIZE                        (2 * 1024U)
#define QL_LCx29H_IIC_RX_SIZE                        (2 * 1024U)

//-----static global variable-----
static  SemaphoreHandle_t iic_port_sem;

//-----static function declare-----
static void Ql_LCx9H_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state);
static int32_t Ql_LCx9H_IIC_ReadData(uint8_t *Data, uint32_t *Size,uint32_t timeout);
static int32_t Ql_LCx9H_IIC_WriteData(uint8_t *pData, uint16_t dataLength,uint32_t timeout);

//-----function define-----
/*******************************************************************************
* Name: static void Ql_LCx9H_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
* Brief: lcx9h serial module iic port error handle
* Input: 
*   Param: iic status
* Output:
*   void
* Return:
*   void
*******************************************************************************/
static void Ql_LCx9H_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
{
    switch(iic_state)
    {
        case QL_IIC_STATUS_OK:
            QL_LOG_I("lcx9h module iic port r/w ok.");
            break;
        case QL_IIC_STATUS_BUSY:
            Ql_IIC_CheckBusSatus();
            Ql_IIC_DeInit();
            QL_LOG_E("lcx9h module iic port busy and re-init.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
            Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);
            break;
        case QL_IIC_STATUS_TIMEOUT:
            QL_LOG_E("lcx9h module iic port r/w timeout.");
            break;
        case QL_IIC_STATUS_NOACK:
            QL_LOG_E("lcx9h module iic port r/w no ack.");
            Ql_IIC_CheckBusSatus();
            Ql_IIC_DeInit();
            vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY*100));
            Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);
            break;
        case QL_IIC_STATUS_SEM_ERROR:
            QL_LOG_E("lcx9h module iic port semaphore error.");
            break;
        case QL_IIC_STATUS_MODE_ERROR:
            QL_LOG_E("lcx9h module iic port mode error.");
            break;
        case QL_IIC_STATUS_MEMORY_ERROR:
            QL_LOG_E("lcx9h module iic port memory error.");
            break;
        case QL_IIC_STATUS_ADDSEND_ERROR:
            QL_LOG_E("lcx9h module iic port address send error.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
            break;
        default:
            QL_LOG_W("lcx9h module iic port unknown state:%d",iic_state);
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
}
/*******************************************************************************
* Name: int32_t Ql_LCx9H_IIC_ReadData(uint8_t *Data, uint32_t *Size,uint32_t timeout)
* Brief: read data from lcx9h module iic port
* Input: 
*   Param: 
*       Data: data buffer
*       Size: data buffer size
*       timeout: timeout
* Output:
*   void
* Return:
*   sucess: read date size.
*   fail: -1
*******************************************************************************/
static int32_t Ql_LCx9H_IIC_ReadData(uint8_t *Data, uint32_t *Size,uint32_t timeout)
{
    uint32_t request_cmd[2];
    uint32_t read_size = *Size;
    int32_t ret = -1;
    uint32_t valid_data_size = 0;
    uint32_t error_number = 0;
    Ql_IIC_Status_TypeDef iic_state = QL_IIC_STATUS_OK;
    BaseType_t xReturn;
    
    *Size = 0;

    xReturn = xSemaphoreTake(iic_port_sem, timeout);
    if (xReturn == pdFAIL)
    {
        QL_LOG_W("lcx9h iic read semaphore take timeout!");
        xSemaphoreGive(iic_port_sem);
        return ret;
    }

    while(1)
    {

        //step 1_a
        request_cmd[0] = (QL_LCx9H_IIC_SLAVE_CR_CMD << 16) | QL_LCx9H_IIC_SLAVE_TX_LEN_REG_OFFSET;
        request_cmd[1] = 4;
        
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        iic_state = Ql_IIC_Write(QL_LCx9H_IIC_SLAVE_ADDR_CR_OR_CW << 1, 0,(uint8_t *)request_cmd, QL_LCx9H_IIC_SLAVE_CMD_LEN, timeout);
        if(QL_IIC_STATUS_OK == iic_state)
        {
            error_number = 0;
        }
        else
        {
            QL_LOG_E("read data step 1_a fail.");
            Ql_LCx9H_IIC_ErrHandle(iic_state);
            if(error_number++ > MAX_ERROR_NUMBER)
            {
                break;
            }
            continue;
        }

        //step 1_b
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        iic_state = Ql_IIC_Read(QL_LCx9H_IIC_SLAVE_ADDR_R << 1, 0,(uint8_t*)&valid_data_size, 4, timeout);
        if(QL_IIC_STATUS_OK == iic_state)
        {
            QL_LOG_D("read data step 1_b valid data size:%d",valid_data_size);
            if(valid_data_size == 0)
            {
                ret = 0;
                break;
            }
        }
        else
        {
            QL_LOG_E("read data step 1_b fail.");
            Ql_LCx9H_IIC_ErrHandle(iic_state);
            break;
        }

        if (read_size > valid_data_size)
        {
            read_size = valid_data_size;
        }

        //step 2_a
        request_cmd[0] = (QL_LCx9H_IIC_SLAVE_CR_CMD << 16) | QL_LCx9H_IIC_SLAVE_TX_BUF_REG_OFFSET;
        request_cmd[1] = read_size;
       
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        iic_state = Ql_IIC_Write(QL_LCx9H_IIC_SLAVE_ADDR_CR_OR_CW << 1, 0,(uint8_t *)request_cmd, QL_LCx9H_IIC_SLAVE_CMD_LEN, timeout);
        if(QL_IIC_STATUS_OK == iic_state)
        {
            QL_LOG_D("read data step 2_a success, request cmd:0x%08x, read size: %d",request_cmd[0],request_cmd[1]);
        }
        else 
        {
            QL_LOG_E("read data step 2_a fail.");
            Ql_LCx9H_IIC_ErrHandle(iic_state);
            break;
        }
        
        //step 2_b
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        iic_state = Ql_IIC_Read(QL_LCx9H_IIC_SLAVE_ADDR_R << 1,0, Data, read_size, timeout);
        if(QL_IIC_STATUS_OK == iic_state)
        {
            *Size = read_size;
            ret = read_size;
            break;
        }
        else
        {
            QL_LOG_E("read data step 2_b fail.");
            Ql_LCx9H_IIC_ErrHandle(iic_state);
            break;
        }
    }
    xSemaphoreGive(iic_port_sem);
    return ret;
}
/*******************************************************************************
* Name: int32_t Ql_LCx9H_IIC_WriteData(uint8_t *pData, uint16_t dataLength,uint32_t timeout)
* Brief: write data to lcx9h module iic port
* Input: 
*   Param: 
*       pData: data buffer
*       dataLength: data buffer length
*       timeout: timeout
* Output:
*   void
* Return:
*   sucess: write data size.
*   fail: -1
*******************************************************************************/
static int32_t Ql_LCx9H_IIC_WriteData(uint8_t *pData, uint16_t dataLength,uint32_t timeout)
{
    uint32_t request_cmd[2];
    uint16_t rxBuffLength = 0;
    int32_t ret = -1;
    uint8_t *p = pData;
    uint16_t len = dataLength;
    bool is_divided = false;
    Ql_IIC_Status_TypeDef status;
    BaseType_t xReturn;

    xReturn = xSemaphoreTake(iic_port_sem, timeout);
    if (xReturn == pdFAIL)
    {
        QL_LOG_W("lcx9h iic write semaphore take timeout!");
        xSemaphoreGive(iic_port_sem);
        return ret;
    }
    
    while(1)
    {
        //step 1_a
        request_cmd[0] = (QL_LCx9H_IIC_SLAVE_CR_CMD << 16) | QL_LCx9H_IIC_SLAVE_RX_LEN_REG_OFFSET;
        request_cmd[1] = 4;
        
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        status = Ql_IIC_Write(QL_LCx9H_IIC_SLAVE_ADDR_CR_OR_CW << 1, 0, (uint8_t *)request_cmd, QL_LCx9H_IIC_SLAVE_CMD_LEN,timeout);
        if(QL_IIC_STATUS_OK == status)
        {
            //QL_LOG_D("write data step 1_a success, request cmd:0x%08x, write size: %d",request_cmd[0],request_cmd[1]);
        }
        else 
        {
            QL_LOG_E("write data step 1_a fail.");
            Ql_LCx9H_IIC_ErrHandle(status);
            break;
        }
        
        //step 1_b
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        status = Ql_IIC_Read(QL_LCx9H_IIC_SLAVE_ADDR_R << 1, 0, (uint8_t*)&rxBuffLength, 4,timeout);
        if(QL_IIC_STATUS_OK == status)
        {
            QL_LOG_D("write length:%d,module receive buffer size:%d",dataLength,rxBuffLength);
        }
        else
        {
            QL_LOG_E("write data step 1_b fail.");
            Ql_LCx9H_IIC_ErrHandle(status);
            break;
        }

        if(len > rxBuffLength)
        {
            len = rxBuffLength;
            is_divided = true;
        }
        else
        {
            is_divided = false;
        }
        
 continue_write:   
        //step 2_a
        request_cmd[0] = (QL_LCx9H_IIC_SLAVE_CW_CMD << 16) | QL_LCx9H_IIC_SLAVE_RX_BUF_REG_OFFSET;
        request_cmd[1] = len;
         vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY)); 
        status = Ql_IIC_Write(QL_LCx9H_IIC_SLAVE_ADDR_CR_OR_CW << 1, 0, (uint8_t *)request_cmd, QL_LCx9H_IIC_SLAVE_CMD_LEN,timeout);
        if(QL_IIC_STATUS_OK == status)
        {
            //QL_LOG_D("write data step 2_a success, request cmd:0x%08x, write size: %d",request_cmd[0],request_cmd[1]);
        }
        else
        {
            QL_LOG_E("write data step 2_a fail.");
            Ql_LCx9H_IIC_ErrHandle(status);
            break;
        }

        //step 2_b
        vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
        status = Ql_IIC_Write(QL_LCx9H_IIC_SLAVE_ADDR_W << 1, 0, p, len,timeout);
        if(QL_IIC_STATUS_OK == status)
        {
            if(true == is_divided)
            {
                p += len;
                len = dataLength - rxBuffLength;
                QL_LOG_D("remain length:%d",len);
                is_divided = false;
                goto continue_write;
            }
            else 
            {
                ret = 0;
                break;
            }
        }
        else if(QL_IIC_STATUS_BUSY == status)
        {
            QL_LOG_E("write data step 2_b fail.");
            Ql_LCx9H_IIC_ErrHandle(status);
            break;
        }
    }
    xSemaphoreGive(iic_port_sem);
    return ret;
}

/*******************************************************************************
* Name: static int32_t Ql_LCx9H_PortIicInit(void *Param)
* Brief: Initialize the iic port of the LC29H GNSS module
* Input: 
*   Param: struct of GNSS_TEA_TypeDef
* Output:
*   void
* Return:
*   0: sucess    -1:fail
*******************************************************************************/
int32_t Ql_LCx9H_PortIicInit( void *Param)
{

    int32_t ret = -1;

    // Initializing iic port

    ret = Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);
 
    iic_port_sem  = xSemaphoreCreateBinary();
    if (iic_port_sem == NULL)
    {
        return -1;
    }
    xSemaphoreGive(iic_port_sem);
    return ret;
}


/*******************************************************************************
* Name: static int32_t Ql_LCx9H_PortIicRead(uint8_t *buffer, uint32_t size,int32_t timeout)
* Brief: Read data from the iic port of the GNSS module
* Input: 
*   port: GNSS IIC
*   buffer: receive buffer
*   size: the size of buffer
*   timeout: GNSS port read timeout
* Output:
*   buffer: received data
* Return:
*   int32_t: received data length
*******************************************************************************/
int32_t Ql_LCx9H_PortIicRead(uint8_t *buffer, uint32_t size,int32_t timeout)
{
    int32_t ret = -1;
    uint32_t len = size;

    if (NULL == buffer)
    {
        QL_LOG_E("invalid param.");
        return -1;
    }
    
    ret = Ql_LCx9H_IIC_ReadData(buffer, &len,timeout);
    
    QL_LOG_D("port iic read data len %d,ret%d", len,ret);
    return ret;
}
/*******************************************************************************
* Name: static int32_t Ql_LCx9H_PortIicWrite(uint8_t *data, uint32_t len)
* Brief: Write data to the iic port of the GNSS module
* Input: 
*   port£ºGNSS IIC
*   data£ºsend to the GNSS module
*   len : send data length
* Output: 
*   void
* Return:
*   int32_t : The number of bytes successfully sent.
*******************************************************************************/
int32_t Ql_LCx9H_PortIicWrite( uint8_t *data, uint32_t len)
{
    int32_t ret = 0;

    if (NULL == data)
    {
        QL_LOG_E("invalid param.");
        return -1;
    }
    
    QL_LOG_D("port iic write data len %d,%s", len,data);
    ret = Ql_LCx9H_IIC_WriteData(data, len, 1000);
    return ret;
}
/*******************************************************************************
* Name: static int32_t Ql_LCx9H_PortIicDeInit(void)
* Brief: Deinitialize the iic port of the GNSS module
* Input: 
*   port: GNSS IIC
* Output:
*   void
* Return:
*   int32_t: 0: sucess    -1:fail
*******************************************************************************/
int32_t Ql_LCx9H_PortIicDeInit(void)
{
    int32_t ret = -1;

    if(NULL != iic_port_sem)
    {
        vSemaphoreDelete(iic_port_sem);
        iic_port_sem = NULL;
    }

    Ql_IIC_DeInit();

    return ret;
}


void Ql_Example_Task(void *Param)
{
    static uint8_t rx_buf[NMEA_BUF_SIZE] = {0};
    char* tx_data = "$PQTMVERNO*58\r\n";
    int32_t Length = 0;

    (void)Param;
    
    QL_LOG_I("--->lcx9h iic nmea<---");

    Ql_LCx9H_PortIicInit(NULL);

    while (1)
    {
        Length = NMEA_BUF_SIZE;
        Ql_LCx9H_IIC_ReadData(rx_buf, &Length,100);
        QL_LOG_I("read data from iic, len: %d", Length);

        if (Length <= 0)
        {
            continue;
        }
        rx_buf[Length] = '\0';

        QL_LOG_I("\r\n%s",rx_buf);

        vTaskDelay(pdMS_TO_TICKS(100));

        QL_LOG_I("send cmd to iic port:%s",tx_data);
        Ql_LCx9H_IIC_WriteData(tx_data, strlen(tx_data), 100);
        
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}
#endif
