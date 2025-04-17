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
  Name: ql_spi.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_spi.h"

#define LOG_TAG "ql_spi"
#define LOG_LVL QL_LOG_INFO
// #define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

//-----macro define -----
/*  func      common  lcx9h */
/* SPI_CS       PE4    PB7  */
/* SPI_SCK      PE2    PB6  */
/* SPI_MISO     PE5    PA1  */
/* SPI_MOSI     PE6    PA0  */
#define QL_SPI_CS_RCU                     RCU_GPIOE
#define QL_SPI_CS_PORT                    GPIOE
#define QL_SPI_CS_PIN                     GPIO_PIN_4
#define QL_SPI_SCK_RCU                    RCU_GPIOE
#define QL_SPI_SCK_PORT                   GPIOE
#define QL_SPI_SCK_PIN                    GPIO_PIN_2
#define QL_SPI_MISO_RCU                   RCU_GPIOE
#define QL_SPI_MISO_PORT                  GPIOE
#define QL_SPI_MISO_PIN                   GPIO_PIN_5
#define QL_SPI_MOSI_RCU                   RCU_GPIOE
#define QL_SPI_MOSI_PORT                  GPIOE
#define QL_SPI_MOSI_PIN                   GPIO_PIN_6
#define QL_SPI_CS_HIGH()                  gpio_bit_set(QL_SPI_CS_PORT, QL_SPI_CS_PIN)
#define QL_SPI_CS_LOW()                   gpio_bit_reset(QL_SPI_CS_PORT, QL_SPI_CS_PIN)
#define QL_SPI_CLK(x)                     gpio_bit_write(QL_SPI_SCK_PORT, QL_SPI_SCK_PIN, x ? SET : RESET)
#define QL_SPI_MOSI(x)                    gpio_bit_write(QL_SPI_MOSI_PORT, QL_SPI_MOSI_PIN, x ? SET : RESET)
#define QL_SPI_MISO()                     gpio_input_bit_get(QL_SPI_MISO_PORT, QL_SPI_MISO_PIN)


#define QL_SPI_LCx9H_CS_RCU                RCU_GPIOB
#define QL_SPI_LCx9H_CS_PORT               GPIOB
#define QL_SPI_LCx9H_CS_PIN                GPIO_PIN_7
#define QL_SPI_LCx9H_SCK_RCU               RCU_GPIOB
#define QL_SPI_LCx9H_SCK_PORT              GPIOB
#define QL_SPI_LCx9H_SCK_PIN               GPIO_PIN_6
#define QL_SPI_LCx9H_MISO_RCU              RCU_GPIOA
#define QL_SPI_LCx9H_MISO_PORT             GPIOA
#define QL_SPI_LCx9H_MISO_PIN              GPIO_PIN_1
#define QL_SPI_LCx9H_MOSI_RCU              RCU_GPIOA
#define QL_SPI_LCx9H_MOSI_PORT             GPIOA
#define QL_SPI_LCx9H_MOSI_PIN              GPIO_PIN_0
#define QL_SPI_LCx9H_CS_HIGH()             gpio_bit_set(QL_SPI_LCx9H_CS_PORT, QL_SPI_LCx9H_CS_PIN)
#define QL_SPI_LCx9H_CS_LOW()              gpio_bit_reset(QL_SPI_LCx9H_CS_PORT, QL_SPI_LCx9H_CS_PIN)
#define QL_SPI_LCx9H_CLK(x)                gpio_bit_write(QL_SPI_LCx9H_SCK_PORT, QL_SPI_LCx9H_SCK_PIN, x ? SET : RESET)
#define QL_SPI_LCx9H_MOSI(x)               gpio_bit_write(QL_SPI_LCx9H_MOSI_PORT, QL_SPI_LCx9H_MOSI_PIN, x ? SET : RESET)
#define QL_SPI_LCx9H_MISO()                gpio_input_bit_get(QL_SPI_LCx9H_MISO_PORT, QL_SPI_LCx9H_MISO_PIN)

typedef struct
{
    SPI_Status_TypeDef (*SPI_InitPtr)(void);
    SPI_Status_TypeDef (*SPI_LowLevelInitPtr)(void);
    SPI_Status_TypeDef (*SPI_DeInitPtr)(void);
    SPI_Status_TypeDef (*SPI_WritePtr)(uint8_t *pData, uint16_t Size, uint32_t Timeout);
    SPI_Status_TypeDef (*SPI_ReadPtr)(uint8_t *pData, uint16_t Size, uint32_t Timeout);
    SPI_Status_TypeDef (*SPI_ReadWritePtr)(uint8_t *sendbuf, uint8_t *recvbuf, uint16_t length, uint32_t Timeout);
} SPI_DeviceInterface_TypeDef;

//-----static global variable-----
static SPI_Ins_TypeDef SPI_Ins;
static SPI_DeviceInterface_TypeDef SPI_DeviceInterface;
//-----static function declare-----
static void Ql_SPI3_PinInit(void);
static void Ql_SPI3_SetConfig(void);

static void Ql_SPI_SwSimulateCs(uint8_t cs)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {
        (0 == cs)?QL_SPI_CS_LOW():QL_SPI_CS_HIGH();
    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {
        (0 == cs)?QL_SPI_LCx9H_CS_LOW():QL_SPI_LCx9H_CS_HIGH();
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
}
static void Ql_SPI_SwSimulateCLK(uint8_t clk)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {
        QL_SPI_CLK(clk);
    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {
        QL_SPI_LCx9H_CLK(clk);
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
}

static void Ql_SPI_SwSimulateMOSI(uint8_t data)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {
        QL_SPI_MOSI(data);
    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {
        QL_SPI_LCx9H_MOSI(data);
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
}

static uint8_t Ql_SPI_SwSimulateMISO(void)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {
        return QL_SPI_MISO();
    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {
        return QL_SPI_LCx9H_MISO();
    }
    spi_ins->Status = SPI_STATUS_MODE_ERROR;
    return 0;
}

static SPI_Status_TypeDef Ql_SPI_SwSimulateInit(void)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    SPI_Status_TypeDef ret = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {

    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {
        rcu_periph_clock_enable(QL_SPI_LCx9H_CS_RCU);
        rcu_periph_clock_enable(QL_SPI_LCx9H_SCK_RCU);
        rcu_periph_clock_enable(QL_SPI_LCx9H_MISO_RCU);
        rcu_periph_clock_enable(QL_SPI_LCx9H_MOSI_RCU);
        
        gpio_mode_set(QL_SPI_LCx9H_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, QL_SPI_LCx9H_CS_PIN);
        gpio_output_options_set(QL_SPI_LCx9H_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, QL_SPI_LCx9H_CS_PIN);
        QL_SPI_LCx9H_CS_HIGH();
        
        gpio_mode_set(QL_SPI_LCx9H_SCK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, QL_SPI_LCx9H_SCK_PIN);
        gpio_output_options_set(QL_SPI_LCx9H_SCK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,QL_SPI_LCx9H_SCK_PIN);

        gpio_mode_set(QL_SPI_LCx9H_MOSI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, QL_SPI_LCx9H_MOSI_PIN);
        gpio_output_options_set(QL_SPI_LCx9H_MOSI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,QL_SPI_LCx9H_MOSI_PIN);
        
        gpio_mode_set(QL_SPI_LCx9H_MISO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, QL_SPI_LCx9H_MISO_PIN);
        gpio_output_options_set(QL_SPI_LCx9H_MISO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, QL_SPI_LCx9H_MISO_PIN);
    }
    else
    {
        ret = SPI_STATUS_MODE_ERROR;
    }
    spi_ins->Status = ret;
    return ret;
}
static SPI_Status_TypeDef Ql_SPI_SwSimulateDeInit(void)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_MODE_SW_SIMULATE == spi_ins->Mode)
    {

    }
    else if(SPI_MODE_SW_LCx9H == spi_ins->Mode)
    {

    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }

    return spi_ins->Status;
}
static uint8_t Ql_SPI_SwSimulateWriteReadByte(uint8_t data)
{
    uint8_t i = 0;
    uint8_t tmp = 0;
    uint8_t read_byte = 0;

    for(i = 0; i < 8; i++)
    {
        tmp = ((data&0x01)==0x01)? 1:0;                              //LSB
        data = data>>1;
        Ql_SPI_SwSimulateCLK(0);                                     //CPOL=0
        Ql_SPI_SwSimulateMOSI(tmp);
        if(Ql_SPI_SwSimulateMISO())
        {
            read_byte = read_byte|(0x01<<i);
        }
        Ql_SPI_SwSimulateCLK(1);                                     //CPHA=0
        delay_us(1); 
    }
    Ql_SPI_SwSimulateMOSI(0);
    Ql_SPI_SwSimulateCLK(0);
    
    return read_byte;
}

static void Ql_SPI_HwInit(void)
{
    spi_parameter_struct spi_init_struct;

    /* enable SPI clock */
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_SPI3);

    /* set SPI3_NSS as GPIO*/
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO_PIN_4);
    
    /* SPI3 GPIO pin configuration(CLK/MISO/MOSI) */
    gpio_af_set(GPIOE, GPIO_AF_5, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);

    /* configure SPI parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI3, &spi_init_struct);

    /* enable SPI */
    spi_enable(SPI3);
}

static void Ql_SPI_HwDeInit(void)
{
    spi_disable(SPI3);
}

static SPI_Status_TypeDef Ql_SPI_SwSimulateWriteReadBytes(uint8_t* sendbuf, uint8_t* recvbuf, uint16_t length, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    BaseType_t xReturn;

    if ((NULL == sendbuf && NULL == recvbuf) || 0 == length)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
        return SPI_STATUS_PARAM_ERROR;
    }

    spi_ins->Status = SPI_STATUS_OK;    
    xReturn = xSemaphoreTake(spi_ins->Sem, Timeout);
    if (xReturn == pdFAIL)
    {
        spi_ins->Status = SPI_STATUS_SEM_ERROR;
        return  SPI_STATUS_SEM_ERROR;
    }

    Ql_SPI_SwSimulateCs(0);
    
    for (int i = 0; i < length; i++)
    {
        if((NULL != recvbuf) && (NULL != sendbuf))
        {
            recvbuf[i] = Ql_SPI_SwSimulateWriteReadByte(sendbuf[i]);
            spi_ins->TxCnt++;
            spi_ins->RxCnt++;
        }
        else if (NULL != recvbuf)
        {
            recvbuf[i] = Ql_SPI_SwSimulateWriteReadByte(0xFF);
            spi_ins->RxCnt++;
        }
        else if (NULL != sendbuf)
        {
            Ql_SPI_SwSimulateWriteReadByte(sendbuf[i]);
            spi_ins->TxCnt++;
        }
    }
    Ql_SPI_SwSimulateCs(1);

    xSemaphoreGive(spi_ins->Sem);

    return spi_ins->Status;
}

SPI_Status_TypeDef Ql_SPI_SwSimulateTransmit(uint8_t *pTxData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    if (NULL == pTxData || 0 == Size)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
        return SPI_STATUS_PARAM_ERROR;
    }

    return Ql_SPI_SwSimulateWriteReadBytes(pTxData, NULL, Size,Timeout);
}

SPI_Status_TypeDef Ql_SPI_SwSimulateReceive(uint8_t *pRxData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    if (NULL == pRxData || 0 == Size)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
        return SPI_STATUS_PARAM_ERROR;
    }
    return Ql_SPI_SwSimulateWriteReadBytes(NULL, pRxData, Size,Timeout);
}

SPI_Status_TypeDef Ql_SPI_HwTransmit(uint8_t *pTxData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    BaseType_t xReturn;

    if (NULL == pTxData || 0 == Size)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
        return SPI_STATUS_PARAM_ERROR;
    }

    spi_ins->Status = SPI_STATUS_OK;    
    xReturn = xSemaphoreTake(spi_ins->Sem, Timeout);
    if (xReturn == pdFAIL)
    {
        return  SPI_STATUS_SEM_ERROR;
    }
    spi_ins->Status = (SPI_Status_TypeDef)Ql_SPI_Transmit(SPI3,pTxData, Size, Timeout);
    xSemaphoreGive(spi_ins->Sem);

    return spi_ins->Status;
}

SPI_Status_TypeDef Ql_SPI_HwReceive(uint8_t *pRxData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    BaseType_t xReturn;

    if (NULL == pRxData || 0 == Size)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
        return SPI_STATUS_PARAM_ERROR;
    }

    spi_ins->Status = SPI_STATUS_OK;    
    xReturn = xSemaphoreTake(spi_ins->Sem, Timeout);
    if (xReturn == pdFAIL)
    {
        return  SPI_STATUS_SEM_ERROR;
    }
    
    spi_ins->Status = (SPI_Status_TypeDef)Ql_SPI_Receive(SPI3,pRxData, Size, Timeout);
    xSemaphoreGive(spi_ins->Sem);

    return spi_ins->Status;
}
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void Ql_SPI3_PinInit(void)
{
    /* enable SPI clock */
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_SPI3);

    /* set SPI3_NSS as GPIO*/
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO_PIN_4);
    
    /* SPI3 GPIO pin configuration(CLK/MISO/MOSI) */
    gpio_af_set(GPIOE, GPIO_AF_5, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6);

}
/*-----------------------------------------------------------*/

static void Ql_SPI3_SetConfig(void)
{
    spi_parameter_struct spi_init_struct;

    /* configure SPI parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI3, &spi_init_struct);

    /* enable SPI */
    spi_enable(SPI3);
}
/*-----------------------------------------------------------*/

void Ql_SPI3_PeriphInit(void)
{
    Ql_SPI3_PinInit();
    SENSOR_ACCEL_CS_HIGH();
    Ql_SPI3_SetConfig();
}
/*-----------------------------------------------------------*/

Ql_StatusTypeDef Ql_SPI_Transmit(uint32_t spi_periph, uint8_t *pTxData, uint16_t Size, uint32_t Timeout)
{
    uint32_t tickstart = 0;
    while(Size--)
    {
        /* Get tick */ 
        tickstart = Ql_GetTick();
        /* loop while data register in not emplty */
        while(RESET == spi_i2s_flag_get(spi_periph, SPI_FLAG_TBE))
        {
            if((Timeout == 0)||((Ql_GetTick() - tickstart ) > Timeout))
            {
                return QL_TIMEOUT;
            }
        }
        /* send byte through the SPI peripheral */
        spi_i2s_data_transmit(spi_periph, *pTxData);
        pTxData++;

        /* Get tick */ 
        tickstart = Ql_GetTick();
        /* wait to receive a byte */
        while(RESET == spi_i2s_flag_get(spi_periph, SPI_FLAG_RBNE))
        {
            if((Timeout == 0)||((Ql_GetTick() - tickstart ) > Timeout))
            {
                return QL_TIMEOUT;
            }
        }
        /* return the byte read from the SPI bus */
        spi_i2s_data_receive(spi_periph);
    }
    return QL_OK;
}

Ql_StatusTypeDef Ql_SPI_Receive(uint32_t spi_periph, uint8_t *pRxData, uint16_t Size, uint32_t Timeout)
{
    uint32_t tickstart = 0;
    
    while(Size--)
    {
        /* Get tick */ 
        tickstart = Ql_GetTick();
        /* loop while data register in not emplty */
        while(RESET == spi_i2s_flag_get(spi_periph, SPI_FLAG_TBE))
        {
            if((Timeout == 0)||((Ql_GetTick() - tickstart ) > Timeout))
            {
                return QL_TIMEOUT;
            }
        }
        /* send byte through the SPI peripheral */
        spi_i2s_data_transmit(spi_periph, DUMMY_BYTE);

        /* Get tick */ 
        tickstart = Ql_GetTick();
        /* wait to receive a byte */
        while(RESET == spi_i2s_flag_get(spi_periph, SPI_FLAG_RBNE))
        {
            if((Timeout == 0)||((Ql_GetTick() - tickstart ) > Timeout))
            {
                return QL_TIMEOUT;
            }
        }
        /* return the byte read from the SPI bus */
        *pRxData = spi_i2s_data_receive(spi_periph);
        pRxData++;
    }
    return QL_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

static SPI_Status_TypeDef Ql_SPI_LowLevelInit(SPI_Mode_TypeDef mode,uint32_t tx_size,uint32_t rx_size)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    if (mode >= SPI_MODE_INVALID || 0 == tx_size || 0 == rx_size)
    {
        return SPI_STATUS_PARAM_ERROR;
    }
    spi_ins->Status = SPI_STATUS_OK;
    spi_ins->TxSize = tx_size;
    spi_ins->RxSize = rx_size;
    spi_ins->TxCnt = 0;
    spi_ins->RxCnt = 0;

    spi_ins->TxBuf = (uint8_t *)pvPortMalloc(spi_ins->TxSize);
    if (spi_ins->TxBuf == NULL)
    {
        return SPI_STATUS_PARAM_ERROR;
    }

    spi_ins->RxBuf = (uint8_t *)pvPortMalloc(spi_ins->RxSize);
    if (spi_ins->RxBuf == NULL)
    {
        vPortFree(spi_ins->TxBuf);
        return SPI_STATUS_PARAM_ERROR;
    }

    memset(&SPI_DeviceInterface, 0, sizeof(SPI_DeviceInterface));

    if(SPI_MODE_HW0_INTERRUPT == mode)
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
    }
    else if(SPI_MODE_HW0_POLLING == mode)
    {
        SPI_DeviceInterface.SPI_LowLevelInitPtr = Ql_SPI_HwInit;
        SPI_DeviceInterface.SPI_DeInitPtr = Ql_SPI_HwDeInit;
        SPI_DeviceInterface.SPI_WritePtr = Ql_SPI_HwTransmit;
        SPI_DeviceInterface.SPI_ReadPtr = Ql_SPI_HwReceive;
        SPI_DeviceInterface.SPI_ReadWritePtr = NULL;
    }
    else if (SPI_MODE_SW_SIMULATE == mode || SPI_MODE_SW_LCx9H == mode)
    {
        SPI_DeviceInterface.SPI_LowLevelInitPtr = Ql_SPI_SwSimulateInit;
        SPI_DeviceInterface.SPI_DeInitPtr = Ql_SPI_SwSimulateDeInit;
        SPI_DeviceInterface.SPI_WritePtr = Ql_SPI_SwSimulateTransmit;
        SPI_DeviceInterface.SPI_ReadPtr = Ql_SPI_SwSimulateReceive;
        SPI_DeviceInterface.SPI_ReadWritePtr = Ql_SPI_SwSimulateWriteReadBytes;
    }
    else
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
    }

    if(SPI_DeviceInterface.SPI_LowLevelInitPtr != NULL)
    {
        spi_ins->Status = SPI_DeviceInterface.SPI_LowLevelInitPtr();
    }
    else
    {
        spi_ins->Status = SPI_STATUS_PARAM_ERROR;
    }

    return spi_ins->Status;
}
SPI_Status_TypeDef Ql_SPI_Init(SPI_Mode_TypeDef mode,uint32_t tx_size,uint32_t rx_size)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;
    BaseType_t xReturn;

    if (mode >= SPI_MODE_INVALID || 0 == tx_size || 0 == rx_size)
    {
        return SPI_STATUS_PARAM_ERROR;
    }

    spi_ins->Mode = mode;
    spi_ins->Status = SPI_STATUS_OK;

    if(NULL == spi_ins->Sem)
    {
        spi_ins->Sem  = xSemaphoreCreateBinary();
        if (spi_ins->Sem == NULL)
        {
            spi_ins->Status = SPI_STATUS_MEMORY_ERROR;
            return SPI_STATUS_MEMORY_ERROR;
        }
    }
    else
    {
        xReturn = xSemaphoreTake(spi_ins->Sem, portMAX_DELAY);
        if (xReturn == pdFAIL)
        {
            QL_LOG_E("Error in Ql_SPI_DeInit.");
            spi_ins->Status = SPI_STATUS_MEMORY_ERROR;
            return SPI_STATUS_SEM_ERROR;
        }
    }
    xSemaphoreGive(spi_ins->Sem);

    return Ql_SPI_LowLevelInit(mode,tx_size,rx_size);
}

SPI_Status_TypeDef Ql_SPI_DeInit(void)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    spi_ins->Status = SPI_STATUS_OK;

    if (NULL != spi_ins->TxBuf)
    {
        vPortFree(spi_ins->TxBuf);
        spi_ins->TxBuf = NULL;
    }
    if (NULL != spi_ins->RxBuf)
    {
        vPortFree(spi_ins->RxBuf);
        spi_ins->RxBuf = NULL;
    }

    if (SPI_DeviceInterface.SPI_DeInitPtr != NULL)
    {
        spi_ins->Status = SPI_DeviceInterface.SPI_DeInitPtr();
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
    return spi_ins->Status;
}

SPI_Ins_TypeDef* Ql_SPI_GetInstance(void)
{
    return &SPI_Ins;
}

SPI_Status_TypeDef  Ql_SPI_Write(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_DeviceInterface.SPI_WritePtr != NULL)
    {
        spi_ins->Status = SPI_DeviceInterface.SPI_WritePtr(pData, Size, Timeout);
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
    return spi_ins->Status;
}

SPI_Status_TypeDef  Ql_SPI_Read(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    spi_ins->Status = SPI_STATUS_OK;

    if (SPI_DeviceInterface.SPI_ReadPtr != NULL)
    {
        spi_ins->Status = SPI_DeviceInterface.SPI_ReadPtr(pData, Size, Timeout);
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
    return spi_ins->Status;
}

SPI_Status_TypeDef  Ql_SPI_ReadWrite(uint8_t* sendbuf, uint8_t* recvbuf, uint16_t length, uint32_t Timeout)
{
    SPI_Ins_TypeDef* spi_ins = &SPI_Ins;

    spi_ins->Status = SPI_STATUS_OK;
    
    if (SPI_DeviceInterface.SPI_ReadWritePtr != NULL)
    {
        spi_ins->Status = SPI_DeviceInterface.SPI_ReadWritePtr(sendbuf,recvbuf,length,Timeout);
    }
    else
    {
        spi_ins->Status = SPI_STATUS_MODE_ERROR;
    }
    return spi_ins->Status;
}
