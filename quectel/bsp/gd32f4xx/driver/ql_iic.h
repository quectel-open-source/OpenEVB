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
  Name: ql_iic.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_IIC_H__
#define __QL_IIC_H__
//-----include header file-----
#include <gd32f4xx.h>
#include "ql_ctype.h"
#include "ql_delay.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

//-----macro define -----
#define QL_IIC_READ      0x10
#define QL_IIC_WRITE     0x11
#define QL_IIC_STOP      0x12

#define VN_IIC_IT_IRQ_PRI        (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1)

#define VN_IIC_IT_RX_COMPLETE    (1 << 0)                            //receive complete
#define VN_IIC_IT_TX_COMPLETE    (1 << 1)                            //transmit complete
#define VN_IIC_IT_RXTX_ERROR     (1 << 2)                            //receive or transmit error

//-----enum define-----
typedef enum
{
    QL_IIC_ACK = 0,
    QL_IIC_NACK
} Ql_IIC_Resp_TypeDef;

typedef enum
{
    IIC_MODE_HW0_INTERRUPT = 0,                                      //I2C0 hardware interrupt
    IIC_MODE_HW0_POLLING,                                            //I2C0 hardware polling
    IIC_MODE_SW_SIMULATE,                                            //software simulate
    IIC_MODE_INVALID
} IIC_Mode_TypeDef;

typedef enum
{
    IIC_SPEED_STANDARD = 0,
    IIC_SPEED_FAST,
    IIC_SPEED_INVALID
} IIC_Speed_TypeDef;

typedef enum
{
    QL_IIC_RESET_FORCE = 0,
    QL_IIC_RESET_RELEASE_SDA,
} Ql_IIC_Reset_TypeDef;

typedef enum
{
    QL_IIC_STATUS_OK                   = 0x00,
    QL_IIC_STATUS_BUSY,
    QL_IIC_STATUS_TIMEOUT,
    QL_IIC_STATUS_NOACK,                                             //no acknowledge received
    QL_IIC_STATUS_MEMORY_ERROR,
    QL_IIC_STATUS_PARAM_ERROR,
    QL_IIC_STATUS_MODE_ERROR,
    QL_IIC_STATUS_SEM_ERROR,
    QL_IIC_STATUS_OU_ERROR,                                          //over-run or under-run when SCL stretch is disabled
    QL_IIC_STATUS_LOSTARB_ERROR,                                     //arbitration lost
    QL_IIC_STATUS_BUS_ERROR,                                         //bus error
    QL_IIC_STATUS_CRC_ERROR,                                         //CRC value doesn't match
    QL_IIC_STATUS_SBSEND_ERROR,                                      //start condition sent out in master mode
    QL_IIC_STATUS_TBE_ERROR,                                         //I2C_DATA is empty during transmitting
    QL_IIC_STATUS_BTC_ERROR,                                         //byte transmission finishes
    QL_IIC_STATUS_ADDSEND_ERROR,                                     //address is sent in master mode or received and matches in slave mode
    QL_IIC_STATUS_RBNE_ERROR,                                        //I2C_DATA is not empty during receiving
    QL_IIC_STATUS_IT_ERROR,
    QL_IIC_STATUS_ERROR,
} Ql_IIC_Status_TypeDef;

//----type define-----
typedef struct
{
    IIC_Mode_TypeDef  mode;
    IIC_Speed_TypeDef speed;
    QL_Name_TypeDef*  mode_name;
    QL_Name_TypeDef*  speed_name;
} IIC_CFG_TypeDef;

typedef struct
{
    IIC_Mode_TypeDef      mode;
    IIC_Speed_TypeDef     speed;
    Ql_IIC_Status_TypeDef status;
    uint8_t               rw;
    uint32_t              rw_size;
    uint32_t              rw_nbytes;
    uint32_t              dev_addr;
    uint32_t              reg_addr;
    SemaphoreHandle_t     sem;
    EventGroupHandle_t    event;
    // SemaphoreHandle_t     rx_sem;
    // SemaphoreHandle_t     tx_sem;
    uint8_t*              tx_buf;
    uint32_t              tx_size;
    uint32_t              tx_index;
    uint8_t*              rx_buf;
    uint32_t              rx_size;
    uint32_t              rx_index;
    uint32_t              err_ev;
    uint32_t              err_er;
    uint32_t              err_nck;
    uint32_t              rx_cnt;
    uint32_t              tx_cnt;
    uint32_t              init_cnt;
} IIC_Ins_TypeDef;                                                   // IIC instance


//-----function declare-----
void Ql_IIC_I2C0_EV_IRQHandler(void);

void Ql_IIC_I2C0_ER_IRQHandler(void);

Ql_IIC_Status_TypeDef Ql_IIC_Init(IIC_Mode_TypeDef mode, IIC_Speed_TypeDef IIC_Mode,uint32_t tx_size,uint32_t rx_size);

void Ql_IIC_DeInit(void);

IIC_Ins_TypeDef* Ql_IIC_GetInstance(void);

void Ql_IIC_CheckBusSatus(void);

Ql_IIC_Status_TypeDef Ql_IIC_Write(uint8_t Sla_Addr, uint8_t Reg_Addr, uint8_t *pData, const uint32_t Length, uint32_t timeout);

Ql_IIC_Status_TypeDef Ql_IIC_Read(uint8_t Sla_Addr, uint8_t Reg_Addr, uint8_t *pData, const uint32_t Length, uint32_t timeout);


#endif /* __QL_IIC_H__ */
