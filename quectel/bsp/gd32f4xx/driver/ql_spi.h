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
  Name: ql_spi.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_SPI_H__
#define __QL_SPI_H__

#include <gd32f4xx.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "ql_ctype.h"
#include "ql_delay.h"

#define DUMMY_BYTE       0xFF

#define SPI3_NSS_PORT                       GPIOE
#define SPI3_NSS_PIN                        GPIO_PIN_4

#define SENSOR_ACCEL_CS_LOW()               gpio_bit_write(SPI3_NSS_PORT, SPI3_NSS_PIN, RESET)
#define SENSOR_ACCEL_CS_HIGH()              gpio_bit_write(SPI3_NSS_PORT, SPI3_NSS_PIN, SET)


typedef enum
{
    SPI_MODE_HW0_INTERRUPT = 0,                                      //SPI3 hardware
    SPI_MODE_HW0_POLLING,                                            //SPI3 polling,
    SPI_MODE_SW_SIMULATE,                                            //software simulate
    SPI_MODE_SW_LCx9H,                                               //LCx9H SPI mode
    SPI_MODE_INVALID
} SPI_Mode_TypeDef;

typedef enum
{
    SPI_SPEED_STANDARD = 0,
    SPI_SPEED_FAST,
    SPI_SPEED_INVALID
} SPI_Speed_TypeDef;

typedef enum
{
    SPI_STATUS_OK                   = 0x00,
    SPI_STATUS_BUSY,
    SPI_STATUS_TIMEOUT,
    SPI_STATUS_NOACK,                                             //no acknowledge received
    SPI_STATUS_MEMORY_ERROR,
    SPI_STATUS_PARAM_ERROR,
    SPI_STATUS_MODE_ERROR,
    SPI_STATUS_SEM_ERROR,
    SPI_STATUS_ERROR,
} SPI_Status_TypeDef;


//----type define-----
typedef struct
{
    SPI_Mode_TypeDef  Mode;
    SPI_Speed_TypeDef Speed;
    QL_Name_TypeDef*  ModeName;
    QL_Name_TypeDef*  SpeedName;
} SPI_CFG_TypeDef;

typedef struct
{
    SPI_Mode_TypeDef      Mode;
    SPI_Speed_TypeDef     Speed;
    SPI_Status_TypeDef    Status;
    uint8_t               RW;
    uint32_t              RW_Size;
    uint32_t              RW_Nbytes;
    SemaphoreHandle_t     Sem;
    EventGroupHandle_t    Event;
    uint8_t*              TxBuf;
    uint32_t              TxSize;
    uint32_t              TxIndex;
    uint8_t*              RxBuf;
    uint32_t              RxSize;
    uint32_t              RxIndex;
    uint32_t              ErrEv;
    uint32_t              ErrEr;
    uint32_t              RxCnt;
    uint32_t              TxCnt;
    uint32_t              InitCnt;
} SPI_Ins_TypeDef;                                                   // SPI instance


extern void Ql_SPI3_PeriphInit(void);
extern Ql_StatusTypeDef Ql_SPI_Transmit(uint32_t spi_periph, uint8_t *pTxData, uint16_t Size, uint32_t Timeout);
extern Ql_StatusTypeDef Ql_SPI_Receive(uint32_t spi_periph, uint8_t *pRxData, uint16_t Size, uint32_t Timeout);

SPI_Status_TypeDef Ql_SPI_Init(SPI_Mode_TypeDef mode,uint32_t tx_size,uint32_t rx_size);
SPI_Status_TypeDef Ql_SPI_DeInit(void);
SPI_Ins_TypeDef* Ql_SPI_GetInstance(void);
SPI_Status_TypeDef  Ql_SPI_Write(uint8_t *pData, uint16_t Size, uint32_t Timeout);
SPI_Status_TypeDef  Ql_SPI_Read(uint8_t *pData, uint16_t Size, uint32_t Timeout);
SPI_Status_TypeDef  Ql_SPI_ReadWrite(uint8_t* sendbuf, uint8_t* recvbuf, uint16_t length, uint32_t Timeout);

#endif
