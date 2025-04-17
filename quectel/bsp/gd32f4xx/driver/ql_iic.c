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
  Name: ql_iic.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

//-----include header file-----
#include "gd32f4xx.h"
#include "ql_iic.h"
#include "ql_delay.h"
#include <stdbool.h>

#define LOG_TAG "ql_iic"
#define LOG_LVL QL_LOG_INFO
// #define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

//-----macro define -----
#define QL_I2C0_RCU_PORT                        RCU_GPIOB
#define QL_I2C0_SCL_GPIO_PORT                   GPIOB
#define QL_I2C0_SDA_GPIO_PORT                   GPIOB
#define QL_I2C0_SCL_GPIO_PIN                    GPIO_PIN_6
#define QL_I2C0_SDA_GPIO_PIN                    GPIO_PIN_7
#define QL_I2C0_DMA0_CH0_IRQ_PRI                configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define I2C0_SLAVE_ADDRESS7                     0x50 >> 1            //address format is 7 bits 

#define QL_I2C_RETRY_TIMES                      5
#define QL_I2C_DELAY                            0x0000FFFF
#define QL_I2C_SEM_TIMEOUT                      1000                //unit = 1ms
#define VN_I2C_EVENT_TIMEOUT                    1000                //unit = 1ms

#define QL_IIC_READ_FLAG                        0x01                // Read Flag
#define QL_IIC_WRITE_FLAG                       0xFE                // Write Flag

#define I2C_SCL_L()                             gpio_bit_write(GPIOB, GPIO_PIN_6, RESET)
#define I2C_SCL_H()                             gpio_bit_write(GPIOB, GPIO_PIN_6, SET)
#define I2C_SDA_L()                             gpio_bit_write(GPIOB, GPIO_PIN_7, RESET)
#define I2C_SDA_H()                             gpio_bit_write(GPIOB, GPIO_PIN_7, SET)
#define I2C_SDA_READ()                          gpio_input_bit_get(GPIOB, GPIO_PIN_7)
#define I2C_SDA_SET_INPUT()                     gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_7)
#define I2C_SDA_SET_OUTPUT()                    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_7)

//-----static global variable-----
static uint32_t simmulate_delay = 5;                                 //IIC delay time (us)
static IIC_Ins_TypeDef iic_ins;                                      //IIC  instance
// static IIC_CFG_TypeDef iic_cfg;                                      //IIC  config

//-----static function declare-----


//-----function define-----
/*******************************************************************************
* Name: void Ql_IIC_SwSimulateStart(void)
* Brief: IIC start signal in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_SwSimulateStart(void)
{
    I2C_SDA_H();
    I2C_SCL_H(); 
    delay_us(simmulate_delay);
    I2C_SDA_L(); 
    delay_us(simmulate_delay);
    I2C_SCL_L();
}
/*******************************************************************************
* Name: void Ql_IIC_SwSimulateStop(void)
* Brief: IIC stop signal in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_SwSimulateStop(void)
{
    I2C_SDA_L();
    I2C_SCL_H(); 
    delay_us(simmulate_delay);
    I2C_SDA_H(); 
    delay_us(simmulate_delay);
}
/*******************************************************************************
* Name: uint8_t Ql_IIC_SwSimulateWaitAck(void)
* Brief: wait for IIC slave to send ACK signal in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   uint8_t
*******************************************************************************/
static uint8_t Ql_IIC_SwSimulateWaitAck(void)
{
    uint16_t ucErrTime = 0;
    
    I2C_SDA_H();
    I2C_SDA_SET_INPUT();
    I2C_SCL_H(); 
    delay_us(1);
    while (I2C_SDA_READ())
    {
        ucErrTime++;
        if (ucErrTime > 500)
        {
            I2C_SDA_SET_OUTPUT();
            return QL_IIC_NACK;
        }
    }
    
    I2C_SDA_SET_OUTPUT();
    I2C_SCL_H(); 
    delay_us(simmulate_delay);
    I2C_SCL_L();
    
    return QL_IIC_ACK;
}
/*******************************************************************************
* Name: void Ql_IIC_SwSimulateAck(void)
* Brief: send ACK signal in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_SwSimulateAck(void)
{
    I2C_SCL_L(); 
    delay_us(simmulate_delay);
    I2C_SDA_L(); 
    delay_us(simmulate_delay);
    I2C_SCL_H(); 
    delay_us(simmulate_delay);
    I2C_SCL_L(); 
    delay_us(simmulate_delay);
    I2C_SDA_H();
}
/*******************************************************************************
* Name: void Ql_IIC_SwSimulateNoAck(void)
* Brief: send NACK signal in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_SwSimulateNoAck(void)
{
    I2C_SCL_L(); 
    delay_us(simmulate_delay);
    I2C_SDA_H(); 
    delay_us(simmulate_delay);
    I2C_SCL_H(); 
    delay_us(simmulate_delay);
    I2C_SCL_L(); 
    delay_us(simmulate_delay);
    I2C_SDA_H();
}
/*******************************************************************************
* Name: void Ql_IIC_SwSimulateSendByte(uint8_t byte)
* Brief: send a byte in software simulation mode
* Input: 
*   uint8_t, data to send
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_SwSimulateSendByte(uint8_t byte)
{
    I2C_SDA_SET_OUTPUT();
    I2C_SCL_L();
    for (uint8_t i = 0; i < 8; i++)
    {
        byte & 0x80 ? I2C_SDA_H() : I2C_SDA_L(); 
        delay_us(simmulate_delay);
        I2C_SCL_H(); 
        delay_us(simmulate_delay);
        I2C_SCL_L(); 
        byte <<= 1;
    }
    I2C_SDA_H();
    I2C_SDA_SET_INPUT();
    delay_us(simmulate_delay);
}
/*******************************************************************************
* Name: uint8_t Ql_IIC_SwSimulateReceiveByte(void)
* Brief: receive a byte in software simulation mode
* Input: 
*   void
* Output:
*   void
* Return:
*   uint8_t
*******************************************************************************/
static uint8_t Ql_IIC_SwSimulateReceiveByte(void)
{
    uint8_t recv = 0;
    
    I2C_SDA_H();
    I2C_SCL_L();
    I2C_SDA_SET_INPUT();
    
    for (uint8_t i = 0; i < 8; i++)
    {
        recv <<= 1;
        I2C_SCL_L(); delay_us(simmulate_delay);
        I2C_SCL_H();
        
        recv = I2C_SDA_READ() ? (recv + 1) : recv; delay_us(simmulate_delay);
    }
    
    I2C_SDA_SET_OUTPUT();
    
    return recv;
}
/*******************************************************************************
* Name: Ql_IIC_Status_TypeDef Ql_IIC_SwSimulateInit(IIC_Mode_TypeDef mode)
* Brief: IIC software init.
* Input: 
*   mode: software i2c mode.
* Return:
*   none.
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_SwSimulateInit(IIC_Mode_TypeDef mode)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_MODE_ERROR;
    BaseType_t xReturn;

    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        return QL_IIC_STATUS_SEM_ERROR;
    }
    
    if(IIC_MODE_SW_SIMULATE == mode)
    {        
        rcu_periph_clock_enable(RCU_GPIOB);
        
        /* PB6 - I2C_SCL */
        gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_6);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_6);
        
        /* PB7 - I2C_SDA */
        gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_7);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_7);

        /* The default is high in the idle state */
        I2C_SCL_H();
        I2C_SDA_H();
        
        status = QL_IIC_STATUS_OK;
    }

    xSemaphoreGive(iic->sem);
    return status;
}
/******************************************************************************
 * Name: Ql_IIC_Status_TypeDef Ql_IIC_SwSimulateDeInit(Ql_IIC_Port_TypeDef I2C)
 * brief: deinit i2c software simulation
* Input: 
*   I2C: i2c port defined by Ql_IIC_Port_TypeDef
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_SwSimulateDeInit(IIC_Mode_TypeDef mode)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_MODE_ERROR;
    BaseType_t xReturn;
    
    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        return QL_IIC_STATUS_SEM_ERROR;
    }
    
    if(IIC_MODE_SW_SIMULATE == mode)
    {
        /* PB6 - I2C_SCL */
        gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_6);
        
        /* PB7 - I2C_SDA */
        gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_7);

        /* The default is high in the idle state */
        I2C_SCL_H();
        I2C_SDA_H();
        
        status = QL_IIC_STATUS_OK;
    }
    xSemaphoreGive(iic->sem);
    return status;
}
/*******************************************************************************
* Name: uint32_t Ql_IIC_HwGetPort(IIC_Mode_TypeDef mode)
* Brief: Get the i2c port
* Input: 
*   mode: i2c mode defined by IIC_Mode_TypeDef
* Return:
*   uint32_t: i2c port
*******************************************************************************/
static uint32_t Ql_IIC_HwGetPort(IIC_Mode_TypeDef mode)
{
    uint32_t iic_periph = 0;
    if ((IIC_MODE_HW0_INTERRUPT == mode) || (IIC_MODE_HW0_POLLING == mode))
    {
        iic_periph = I2C0;
    }
    else
    {
        iic_periph = 0;
    }
    return iic_periph;
}
/*******************************************************************************
* Name: void Ql_IIC_HwItEnable(void)
* Brief: Enable i2c interrupt
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_HwItEnable(void)
{
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    iic_periph = Ql_IIC_HwGetPort(iic->mode);
    
    if(0 == iic_periph)
    {
        return;
    }
    
    i2c_interrupt_enable(iic_periph, I2C_INT_ERR);
    i2c_interrupt_enable(iic_periph, I2C_INT_EV);
    i2c_interrupt_enable(iic_periph, I2C_INT_BUF);
}
/*******************************************************************************
* Name: void Ql_IIC_HwItDisable(void)
* Brief: Disable i2c interrupt
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_HwItDisable(void)
{
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    iic_periph = Ql_IIC_HwGetPort(iic->mode);
    
    if(0 == iic_periph)
    {
        return;
    }
    
    i2c_interrupt_disable(iic_periph, I2C_INT_ERR);
    i2c_interrupt_disable(iic_periph, I2C_INT_EV);
    i2c_interrupt_disable(iic_periph, I2C_INT_BUF);
}
/*******************************************************************************
* Name: void Ql_IIC_HwFlagClear(void)
* Brief: Clear i2c flag
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_HwFlagClear(void)
{
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    iic_periph = Ql_IIC_HwGetPort(iic->mode);
    
    if(0 == iic_periph)
    {
        return;
    }
    
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_ADDSEND);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_BERR);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_LOSTARB);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_AERR);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_OUERR);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_PECERR);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_SMBTO);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_SMBALT);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_TFF);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_TFR);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_RFF);
    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_RFR);

    i2c_flag_clear(iic_periph, I2C_FLAG_SMBALT);
    i2c_flag_clear(iic_periph, I2C_FLAG_SMBTO);
    i2c_flag_clear(iic_periph, I2C_FLAG_PECERR);
    i2c_flag_clear(iic_periph, I2C_FLAG_OUERR);
    i2c_flag_clear(iic_periph, I2C_FLAG_AERR);
    i2c_flag_clear(iic_periph, I2C_FLAG_LOSTARB);
    i2c_flag_clear(iic_periph, I2C_FLAG_BERR);
    i2c_flag_clear(iic_periph, I2C_FLAG_ADDSEND);
    i2c_flag_clear(iic_periph, I2C_FLAG_TFF);
    i2c_flag_clear(iic_periph, I2C_FLAG_TFR);
    i2c_flag_clear(iic_periph, I2C_FLAG_RFF);
    i2c_flag_clear(iic_periph, I2C_FLAG_RFR);
}
/*******************************************************************************
* Name: void Ql_IIC_HwInit(IIC_Mode_TypeDef mode, IIC_Speed_TypeDef speed_type)
* Brief: IIC hardware controller initialization.
* Input: 
*   mode: i2c mode defined by IIC_Mode_TypeDef
*   speed_type: i2c speed defined by IIC_Speed_TypeDef
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_HwInit(IIC_Mode_TypeDef mode, IIC_Speed_TypeDef speed_type)
{
    uint32_t speed = 100000;
    uint32_t iic_periph;
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    
    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        QL_LOG_E("wait iic port sem failed");
        return ;
    }

    if((IIC_MODE_HW0_INTERRUPT == mode) || (IIC_MODE_HW0_POLLING == mode))
    {
        rcu_periph_clock_enable(QL_I2C0_RCU_PORT);
        rcu_periph_clock_enable(RCU_I2C0);

        i2c_stop_on_bus(iic_periph);
        Ql_IIC_HwFlagClear();

        gpio_af_set(QL_I2C0_SCL_GPIO_PORT, GPIO_AF_4, QL_I2C0_SCL_GPIO_PIN);
        gpio_af_set(QL_I2C0_SDA_GPIO_PORT, GPIO_AF_4, QL_I2C0_SDA_GPIO_PIN);
        gpio_mode_set(QL_I2C0_SCL_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, QL_I2C0_SCL_GPIO_PIN);
        gpio_mode_set(QL_I2C0_SDA_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, QL_I2C0_SDA_GPIO_PIN);
        gpio_output_options_set(QL_I2C0_SCL_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, QL_I2C0_SCL_GPIO_PIN);
        gpio_output_options_set(QL_I2C0_SDA_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, QL_I2C0_SDA_GPIO_PIN);

        if(IIC_SPEED_STANDARD == speed_type)
        {
            speed = 100000;
        }
        else if (IIC_SPEED_FAST == speed_type)
        {
            speed = 400000;
        }

        iic_periph = Ql_IIC_HwGetPort(iic->mode);
        
        // i2c_software_reset_config(iic_periph, I2C_SRESET_SET);
        i2c_clock_config(iic_periph, speed, I2C_DTCY_2);
        i2c_mode_addr_config(iic_periph, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C0_SLAVE_ADDRESS7);

        //Ql_IIC_HwDmaConfig(&iic_ins);

        i2c_enable(iic_periph);
        i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
        
        //nvic_priority_group_set(NVIC_PRIGROUP_PRE1_SUB3);
        nvic_irq_enable(I2C0_EV_IRQn, VN_IIC_IT_IRQ_PRI, 0);
        nvic_irq_enable(I2C0_ER_IRQn, VN_IIC_IT_IRQ_PRI, 0);
    }

    xSemaphoreGive(iic->sem);
    return;
}
/*******************************************************************************
* Name: void Ql_IIC_HwDeInit(Ql_IIC_Port_TypeDef I2C)
* Brief: IIC hardware controller de-initialization.
* Input: 
*   mode: i2c mode defined by IIC_Mode_TypeDef
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_HwDeInit(IIC_Mode_TypeDef mode)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    uint32_t iic_periph;
    
    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        QL_LOG_E("wait iic port sem failed");
        return ;
    }
    if((IIC_MODE_HW0_INTERRUPT == mode) || (IIC_MODE_HW0_POLLING == mode))
    {
        iic_periph = Ql_IIC_HwGetPort(iic->mode);
        Ql_IIC_HwItDisable();
        Ql_IIC_HwFlagClear();
        i2c_software_reset_config(iic_periph, I2C_SRESET_SET);
        i2c_disable(iic_periph);
        i2c_deinit(iic_periph);
        rcu_periph_clock_disable(RCU_I2C0);
        gpio_mode_set(QL_I2C0_SCL_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, QL_I2C0_SCL_GPIO_PIN);
        gpio_mode_set(QL_I2C0_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, QL_I2C0_SDA_GPIO_PIN);
        gpio_output_options_set(QL_I2C0_SCL_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, QL_I2C0_SCL_GPIO_PIN);
        gpio_output_options_set(QL_I2C0_SDA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, QL_I2C0_SDA_GPIO_PIN);
    }
    xSemaphoreGive(iic->sem);
    return;
}

/******************************************************************************
 * Name: void Ql_IIC_ForceReset(void)
 * brief: force reset i2c bus
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_ForceReset(void)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    
    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        QL_LOG_E("wait iic port sem failed");
        return ;
    }
    
    GPIO_BC(GPIOB) |=  GPIO_PIN_6 | GPIO_PIN_7;

    /* PB6 - I2C_SCL */
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_6);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_6);
    
    /* PB7 - I2C_SDA */
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_7);

    delay_us(1);
    GPIO_BOP(GPIOB) |= GPIO_PIN_6;
    delay_us(1);
    GPIO_BOP(GPIOB) |= GPIO_PIN_7;

    gpio_af_set(QL_I2C0_SCL_GPIO_PORT, GPIO_AF_4, QL_I2C0_SCL_GPIO_PIN);
    gpio_af_set(QL_I2C0_SDA_GPIO_PORT, GPIO_AF_4, QL_I2C0_SDA_GPIO_PIN);
    gpio_mode_set(QL_I2C0_SCL_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, QL_I2C0_SCL_GPIO_PIN);
    gpio_mode_set(QL_I2C0_SDA_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, QL_I2C0_SDA_GPIO_PIN);
    gpio_output_options_set(QL_I2C0_SCL_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, QL_I2C0_SCL_GPIO_PIN);
    gpio_output_options_set(QL_I2C0_SDA_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, QL_I2C0_SDA_GPIO_PIN);
    
    xSemaphoreGive(iic->sem);
    return;
}

/******************************************************************************
 * Name: void Ql_IIC_ReleaseSda(void)
 * brief: release i2c bus
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_ReleaseSda(void)
{
    uint8_t i = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    uint32_t iic_periph;
    
    xReturn = xSemaphoreTake(iic->sem, QL_I2C_SEM_TIMEOUT);
    if (xReturn == pdFAIL)
    {
        QL_LOG_E("wait iic port sem failed");
        return ;
    }

    iic_periph = Ql_IIC_HwGetPort(iic->mode);

    /* PB6 - I2C_SCL */
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_6);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

    /* SCL output clock signal */
    for(i = 0; i < 10; i++)
    {
        gpio_bit_reset(GPIOB, GPIO_PIN_6);
        delay_us(2);
        gpio_bit_set(GPIOB, GPIO_PIN_6);
        delay_us(2);
    }
    /* reset I2C */
    if(iic_periph)
    {
        i2c_software_reset_config(iic_periph, I2C_SRESET_RESET);
        i2c_software_reset_config(iic_periph, I2C_SRESET_SET);
    }
    // gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_af_set(GPIOB, GPIO_AF_4, GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    
    xSemaphoreGive(iic->sem);
}
/******************************************************************************
 * Name: void Ql_IIC_BusReset(void)
 * brief: reset i2c bus
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_IIC_BusReset(void)
{
    int sda = gpio_input_bit_get(GPIOB, GPIO_PIN_7);  //sda
    int scl = gpio_input_bit_get(GPIOB, GPIO_PIN_6);  //scl

    if (scl > 0 &&  0 == sda)
    {
        Ql_IIC_ReleaseSda();
    }
    else 
    {
        Ql_IIC_ForceReset();
    }
}
/*******************************************************************************
* Name: Ql_IIC_Status_TypeDef Ql_IIC_WaitForFlagUntilTimeout(uint32_t I2C_Port, uint32_t Flag, FlagStatus Status, uint32_t Timeout)
* Brief: Obtain the IIC controller status flag within the timeout period
* Input: 
*   I2C_Port: i2c port
*   Flag: i2c status flag defined by i2c_flag_enum
*   Status: i2c status flag defined by FlagStatus
*   Timeout: timeout period 
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_TIMEOUT
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_WaitForFlagUntilTimeout(uint32_t I2C_Port, i2c_flag_enum Flag, FlagStatus Status, uint32_t Timeout)
{
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_OK;
    while(Status == i2c_flag_get(I2C_Port, Flag))
    {
        if(Timeout-- == 0)
        {
            status = QL_IIC_STATUS_TIMEOUT;
            break;
        }
    }
    return status;
}

/*******************************************************************************
* Name:  Ql_IIC_Status_TypeDef Ql_IIC_LowLevelInit(Ql_IIC_Port_TypeDef port, IIC_Speed_TypeDef IIC_Mode)
* Brief: This function initialize the configurations for an port. including SW IIC or HW IIC.
* Input: 
*   mode: IIC mode,
* Return:
*   none.
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_LowLevelInit(IIC_Mode_TypeDef mode, IIC_Speed_TypeDef speed,uint32_t tx_size,uint32_t rx_size)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    if (mode >= IIC_MODE_INVALID || tx_size == 0 || rx_size == 0)
    {
        return QL_IIC_STATUS_PARAM_ERROR;
    }

    iic->tx_size = tx_size;
    iic->rx_size = rx_size;
    iic->tx_cnt = 0;
    iic->rx_cnt = 0;

    iic->tx_buf = (uint8_t *)pvPortMalloc(iic->tx_size);
    if (iic->tx_buf == NULL)
    {
        return QL_IIC_STATUS_MEMORY_ERROR;
    }

    iic->rx_buf = (uint8_t *)pvPortMalloc(iic->rx_size);
    if (iic->rx_buf == NULL)
    {
        vPortFree(iic->tx_buf);
        return QL_IIC_STATUS_MEMORY_ERROR;
    }

    if(IIC_MODE_SW_SIMULATE == mode)
    {
        if(IIC_SPEED_STANDARD == iic->speed) 
        {
            simmulate_delay = 5;
        }
        else if(IIC_SPEED_FAST == iic->speed)
        {
            simmulate_delay = 1;
        } 
        else
        {
            simmulate_delay = 5;
        }
        Ql_IIC_SwSimulateInit(mode);
    }
    else if(IIC_MODE_HW0_INTERRUPT == mode)
    {
        if (NULL == iic->event)
        {
            iic->event = xEventGroupCreate();
        }
        Ql_IIC_HwInit(mode,iic->speed);
    }
    else if (IIC_MODE_HW0_POLLING == mode)
    {
        Ql_IIC_HwInit(mode,iic->speed);
    }
    else
    {
        return QL_IIC_STATUS_PARAM_ERROR;
    }
    return QL_IIC_STATUS_OK;
}
/*******************************************************************************
* Hayden @ 20241011
* Name: IIC_Ins_TypeDef* Ql_IIC_InstanceReset(void)
* Brief: 
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
static void Ql_IIC_InstanceReset(void)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;

    xReturn = xSemaphoreTake(iic->sem, portMAX_DELAY);
    if(xReturn == pdFAIL)
    {
        QL_LOG_E("Error in Ql_IIC_DeInit.");
        return;
    }
		
    iic->mode  = IIC_MODE_INVALID;
    iic->speed = IIC_SPEED_INVALID;
    iic->status = QL_IIC_STATUS_OK;

    if(NULL != iic->rx_buf)
    {
        vPortFree(iic->rx_buf);
        iic->rx_buf = NULL;
        iic->rx_size = 0;
    }
    if(NULL != iic->tx_buf)
    {
        vPortFree(iic->tx_buf);
        iic->tx_buf = NULL;
        iic->tx_size = 0;
    }
    // iic->err_er = 0;
    // iic->err_ev = 0;
    // iic->err_nck = 0;
    // iic->tx_cnt  = 0;
    // iic->rx_cnt  = 0;

    xSemaphoreGive(iic->sem);

    return;
}
/*******************************************************************************
* Name: Ql_IIC_SwMasterTransmit(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode transmit data through polling.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*   Data: data buffer
*   Size: data size
*   Timeout: timeout period��unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_SwMasterTransmit(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint16_t i = 0;
    BaseType_t xReturn;
    iic->status = QL_IIC_STATUS_OK;
    
    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        return  QL_IIC_STATUS_SEM_ERROR;
    }
    
    do
    {
        Ql_IIC_SwSimulateStart();
        Ql_IIC_SwSimulateSendByte(DevAddr & QL_IIC_WRITE_FLAG);
        
        if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
        {
            iic->status = QL_IIC_STATUS_NOACK;
            iic->err_nck++;
            break;
        }
        
        if(Reg_Addr > 0)        //if Reg_Addr is 0, no need to send register address to slave
        {
            Ql_IIC_SwSimulateSendByte(Reg_Addr);

            if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
            {
                iic->status = QL_IIC_STATUS_NOACK;
                iic->err_nck++;
                break;
            }
        }
        
        for (i = 0; i < Size; i++)
        {
            Ql_IIC_SwSimulateSendByte(*(Data+i));

            if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
            {
                iic->status = QL_IIC_STATUS_NOACK;
                iic->err_nck++;
                break;
            }
        }
        break;
    }while (0);
    
    Ql_IIC_SwSimulateStop();
    if (QL_IIC_STATUS_OK == iic->status)
    {
        iic->tx_cnt += Size;
    }
    else
    {
        iic->err_er++;
    }
    xSemaphoreGive(iic->sem);
    return iic->status;
}
/*******************************************************************************
* Name: Ql_IIC_SwMasterReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode transmit data through polling.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*   Data: data buffer
*   Size: data size
*   Timeout: timeout period��unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_SwMasterReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{
    uint16_t i = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    iic->status = QL_IIC_STATUS_OK;
    
    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        iic->status = QL_IIC_STATUS_SEM_ERROR;
        return iic->status;
    }

    do
    {
        if(Reg_Addr > 0)
        {
            Ql_IIC_SwSimulateStart();
            Ql_IIC_SwSimulateSendByte(DevAddr & QL_IIC_WRITE_FLAG);
            
            if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
            {
                iic->status = QL_IIC_STATUS_NOACK;
                iic->err_nck++;
                break;
            }
            
            Ql_IIC_SwSimulateSendByte(Reg_Addr);

            if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
            {
                iic->status = QL_IIC_STATUS_NOACK;
                iic->err_nck++;
                break;
            }
        }
        
        Ql_IIC_SwSimulateStart();
        
        Ql_IIC_SwSimulateSendByte(DevAddr | QL_IIC_READ_FLAG);
        
        if(Ql_IIC_SwSimulateWaitAck() != QL_IIC_ACK)
        {
            iic->status = QL_IIC_STATUS_NOACK;
            break;
        }

        for (i = 0; i < Size; i ++)
        {
            *Data = Ql_IIC_SwSimulateReceiveByte();

            if(i < Size -1)
            {
                Ql_IIC_SwSimulateAck();
            }
            Data++;
        }
        Ql_IIC_SwSimulateNoAck();
        break;
    }while(0);
    
    Ql_IIC_SwSimulateStop();
    if (QL_IIC_STATUS_OK == iic->status)
    {
        iic->rx_cnt += Size;
    }
    else
    {
        iic->err_er++;
    }
    xSemaphoreGive(iic->sem);
    return iic->status;
}
/*******************************************************************************
* Name: Ql_IIC_HwMasterItReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode receive data through interrupt.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*   Data: data buffer
*   Size: data size
*   Timeout: timeout period unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_HwMasterItTransmit(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t start_tick = Ql_GetTick();
    BaseType_t xReturn;
    EventBits_t waitEventBits = 0;

    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        xSemaphoreGive(iic->sem);
        iic->rw_nbytes = 0;
        iic->status = QL_IIC_STATUS_SEM_ERROR;
        return iic->status;
    }
    
    iic_periph = Ql_IIC_HwGetPort(iic->mode);

    if (0 == iic_periph)
    {
        xSemaphoreGive(iic->sem);
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        return iic->status;
    }

    if(iic->tx_size < Size)
    {
        xSemaphoreGive(iic->sem);
        iic->status = QL_IIC_STATUS_MEMORY_ERROR;
        return iic->status;
    }

    iic->dev_addr = DevAddr;
    iic->reg_addr = Reg_Addr;
    iic->rw_size = Size;
    iic->rw_nbytes = Size;
    memcpy(iic->tx_buf, Data, Size);
    iic->rw = QL_IIC_WRITE;
    iic->status = QL_IIC_STATUS_OK;

    /* the master waits until the I2C bus is idle */
    if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_I2CBSY, SET, QL_I2C_DELAY))
    {
        iic->err_er++;
        xSemaphoreGive(iic->sem);
        iic->status = QL_IIC_STATUS_BUSY;
        return iic->status;
    }

    Ql_IIC_HwItEnable();
    /* the master sends a start condition to I2C bus */
    i2c_start_on_bus(iic_periph);

    waitEventBits = xEventGroupWaitBits(iic->event, VN_IIC_IT_TX_COMPLETE|VN_IIC_IT_RXTX_ERROR, pdTRUE, pdFALSE, VN_I2C_EVENT_TIMEOUT);
    
    if(0 == waitEventBits)
    {
        iic->err_er++;
        iic->status = QL_IIC_STATUS_TIMEOUT;
        QL_LOG_E("iic interrupt transmit timeout, rwbytes:%d",iic->rw_nbytes);
    } 
    else 
    {
        if(waitEventBits & VN_IIC_IT_RXTX_ERROR)
        {
            //QL_LOG_E("iic interrupt receive error");
        }

        if(waitEventBits & VN_IIC_IT_TX_COMPLETE)
        {
            iic->tx_cnt += Size;
        }
    }
    xSemaphoreGive(iic->sem);
    return iic->status;
}

/*******************************************************************************
* Name: Ql_IIC_HwMasterItReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode receive data through interrupt.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*   Data: data buffer
*   Size: data size
*   Timeout: timeout period unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_HwMasterItReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{ 
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t start_tick = Ql_GetTick();
    BaseType_t xReturn;
    EventBits_t waitEventBits = 0;
    
    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        iic->rw_nbytes = 0;
        iic->status = QL_IIC_STATUS_SEM_ERROR;
        xSemaphoreGive(iic->sem);
        return QL_IIC_STATUS_SEM_ERROR;
    }

    iic_periph = Ql_IIC_HwGetPort(iic->mode);

    if (0 == iic_periph)
    {
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        xSemaphoreGive(iic->sem);
        return QL_IIC_STATUS_MODE_ERROR;
    }

    iic->dev_addr = DevAddr;
    iic->reg_addr = Reg_Addr;
    memset(iic->rx_buf,0,iic->rx_size);
    iic->rw_size = iic->rx_size > Size ? Size : iic->rx_size;
    iic->rw_nbytes = iic->rw_size;
    iic->rx_index = 0;
    iic->rw = QL_IIC_READ;
    iic->status = QL_IIC_STATUS_OK;

    Ql_IIC_HwItEnable();

    if(2 == iic->rw_size) {
        /* send ACK for the next byte */
        i2c_ackpos_config(iic_periph, I2C_ACKPOS_NEXT);
    }
    /* the master waits until the I2C bus is idle */

    if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_I2CBSY, SET, QL_I2C_DELAY))
    {
        iic->err_er++;
        xSemaphoreGive(iic->sem);
        iic->status = QL_IIC_STATUS_BUSY;
        return iic->status;
    }
    
    /* the master sends a start condition to I2C bus */
    i2c_start_on_bus(iic_periph);

    waitEventBits = xEventGroupWaitBits(iic->event, VN_IIC_IT_RX_COMPLETE|VN_IIC_IT_RXTX_ERROR, pdTRUE, pdFALSE, VN_I2C_EVENT_TIMEOUT);
    
    if(0 == waitEventBits)
    {
        iic->err_er++;
        iic->status = QL_IIC_STATUS_TIMEOUT;
        QL_LOG_E("iic interrupt transmit timeout, rwbytes:%d",iic->rw_nbytes);
    } 
    else 
    {
        if(waitEventBits & VN_IIC_IT_RXTX_ERROR)
        {
            iic->status = QL_IIC_STATUS_ERROR;
            QL_LOG_E("iic interrupt receive error");
        }

        if(waitEventBits & VN_IIC_IT_RX_COMPLETE)
        {
            if (QL_IIC_STATUS_OK == iic->status)
            {
                memcpy(Data, iic->rx_buf, iic->rw_size);
                iic->rx_cnt += iic->rw_size;
                QL_LOG_D("iic read data: %d bytes, expect:%d bytes,nbytes:%d",iic->rx_index,Size,iic->rw_nbytes);
            }
        }
    }
    xSemaphoreGive(iic->sem);
    return iic->status;
}
/*******************************************************************************
* Name: Ql_IIC_HwMasterTransmit(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode transmit data through polling.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*   Data: data buffer
*   Size: data size
*   Timeout: timeout period unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_HwMasterTransmit(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{ 
    uint32_t i = 0;
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t start_tick = Ql_GetTick();
    BaseType_t xReturn;
    uint32_t delay = QL_I2C_DELAY;
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_OK;

    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        iic->status = QL_IIC_STATUS_SEM_ERROR;
        return QL_IIC_STATUS_SEM_ERROR;
    }

    iic_periph = Ql_IIC_HwGetPort(iic->mode);

    if (0 == iic_periph)
    {
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        xSemaphoreGive(iic->sem);
        return QL_IIC_STATUS_MODE_ERROR;
    }

    do
    {
        i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_I2CBSY, SET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_BUSY;
            break;
        }

        i2c_start_on_bus(iic_periph);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_SBSEND, RESET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_SBSEND_ERROR;
            break;
        }

        i2c_master_addressing(iic_periph, DevAddr, I2C_TRANSMITTER);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_ADDSEND, RESET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_ADDSEND_ERROR;
            break;
        }

        i2c_flag_clear(iic_periph, I2C_FLAG_ADDSEND);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_TBE, RESET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_TBE_ERROR;
            break;
        }

        /* if Reg_Addr > 0, send register address */
        if(Reg_Addr > 0)        
        {
            i2c_data_transmit(iic_periph, Reg_Addr);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_BTC, RESET, QL_I2C_DELAY))
            {
                QL_LOG_E( "i2c_data_transmit error,sent:%d,total:%d", i, Size);
                i2c_stop_on_bus(iic_periph);
                iic->status = QL_IIC_STATUS_BTC_ERROR;
                break;
            }
        }

        for(i = 0; i < Size; i++)
        {
            i2c_data_transmit(iic_periph, *(Data + i));
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_BTC, RESET, QL_I2C_DELAY))
            {
                QL_LOG_E( "i2c_data_transmit error,sent:%d,total:%d", i, Size);
                i2c_stop_on_bus(iic_periph);
                iic->status = QL_IIC_STATUS_BTC_ERROR;
                break;
            }
        }
        if(QL_IIC_STATUS_OK != iic->status)
        {
            break;
        }

        i2c_stop_on_bus(iic_periph);
        delay = QL_I2C_DELAY;
        while(I2C_CTL0(iic_periph) & I2C_CTL0_STOP)
        {
            if(0 == delay--)
            {
                iic->status = QL_IIC_STATUS_TIMEOUT;
                break;
            }
        }
    }while(0);
    
    if(QL_IIC_STATUS_OK == iic->status)
    {
        iic->tx_cnt += Size;
    }
    else
    {
        iic->err_er++;
    }
    status = iic->status;
    xSemaphoreGive(iic->sem);
    return status;
}
/*******************************************************************************
* Name: Ql_IIC_HwMasterReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
* Brief: hardware IIC master mode transmit data through polling.
* Input: 
*   DevAddr: i2c device address
*   Reg_Addr: i2c register address
*  Data: data buffer
*   Size: data size
*   Timeout: timeout period��unit:ms
* Return:
*   Ql_IIC_Status_TypeDef: QL_IIC_STATUS_OK or QL_IIC_STATUS_ERROR
*******************************************************************************/
static Ql_IIC_Status_TypeDef Ql_IIC_HwMasterReceive(uint16_t DevAddr, uint8_t Reg_Addr,uint8_t *Data, uint32_t Size, uint32_t Timeout)
{ 
    uint32_t i = 0;
    uint32_t iic_periph = 0;
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t delay = QL_I2C_DELAY;
    BaseType_t xReturn;
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_OK;

    xReturn = xSemaphoreTake(iic->sem, Timeout);
    if (xReturn == pdFAIL)
    {
        iic->status = QL_IIC_STATUS_SEM_ERROR;
        return QL_IIC_STATUS_SEM_ERROR;
    }

    iic_periph = Ql_IIC_HwGetPort(iic->mode);

    if (0 == iic_periph)
    {
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        xSemaphoreGive(iic->sem);
        return QL_IIC_STATUS_MODE_ERROR;
    }

    iic->rw_nbytes = Size;

    if(1 == Size)
    {
        i2c_ack_config(iic_periph, I2C_ACK_DISABLE);
    }
    else if( 2 == Size )
    {
        /* send a NACK for the next data byte which will be received into the shift register */
        i2c_ackpos_config(I2C0, I2C_ACKPOS_NEXT);
    }

    do
    {
        iic->status = QL_IIC_STATUS_OK;
        
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_I2CBSY, SET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_BUSY;
            QL_LOG_W( "iic ");
            break;
        }

        if(Reg_Addr > 0)
        {
            i2c_start_on_bus(iic_periph);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_SBSEND, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_SBSEND_ERROR;
                break;
            }

            i2c_master_addressing(iic_periph, DevAddr, I2C_TRANSMITTER);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_ADDSEND, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_ADDSEND_ERROR;
                break;
            }

            i2c_flag_clear(iic_periph, I2C_FLAG_ADDSEND);

            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_TBE, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_TBE_ERROR;
                break;
            }

            i2c_data_transmit(iic_periph, Reg_Addr);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_BTC, RESET, QL_I2C_DELAY))
            {
                QL_LOG_E( "i2c_data_transmit error,sent:%d,total:%d", i, Size);
                i2c_stop_on_bus(iic_periph);
                iic->status = QL_IIC_STATUS_BTC_ERROR;
                break;
            }
        }
        
        i2c_start_on_bus(iic_periph);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_SBSEND, RESET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_SBSEND_ERROR;
            break;
        }

        i2c_master_addressing(iic_periph, DevAddr, I2C_RECEIVER);
        if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_ADDSEND, RESET, QL_I2C_DELAY))
        {
            iic->status = QL_IIC_STATUS_ADDSEND_ERROR;
            break;
        }

        /* clear ADDSEND bit */
        i2c_flag_clear(iic_periph, I2C_FLAG_ADDSEND);

        if (1 == Size)
        {
            i2c_stop_on_bus(iic_periph);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_RBNE, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_RBNE_ERROR;
                break;
            }
            
            *Data = i2c_data_receive(iic_periph);
            delay = QL_I2C_DELAY;
            while(I2C_CTL0(iic_periph) & I2C_CTL0_STOP)
            {
                if(0 == delay--)
                {
                    iic->status = QL_IIC_STATUS_TIMEOUT;
                    break;
                }
            }
            i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
            break;
        } 
        
        if (2 == Size)
        {
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_BTC, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_BTC_ERROR;
                break;
            }
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_RBNE, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_RBNE_ERROR;
                break;
            }
            
            *Data = i2c_data_receive(iic_periph);
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_RBNE, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_RBNE_ERROR;
                break;
            }
            
            *(Data + 1) = i2c_data_receive(iic_periph);
            
            i2c_stop_on_bus(iic_periph);
            delay = QL_I2C_DELAY;
            while(I2C_CTL0(iic_periph) & I2C_CTL0_STOP)
            {
                if(0 == delay--)
                {
                    iic->status = QL_IIC_STATUS_TIMEOUT;
                    break;
                }
            }
            
            i2c_ackpos_config(iic_periph, I2C_ACKPOS_CURRENT);
            i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
            break;
        }
        
        for(i = 0; i < Size; i++)
        {
            if((Size - 3) == i)
            {
                if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_BTC, RESET, QL_I2C_DELAY))
                {
                    iic->status = QL_IIC_STATUS_BTC_ERROR;
                    break;
                }
                i2c_ack_config(iic_periph, I2C_ACK_DISABLE);
            }
            if(QL_IIC_STATUS_TIMEOUT == Ql_IIC_WaitForFlagUntilTimeout(iic_periph, I2C_FLAG_RBNE, RESET, QL_I2C_DELAY))
            {
                iic->status = QL_IIC_STATUS_RBNE_ERROR;
                break;
            }
            *(Data + i) = i2c_data_receive(iic_periph);
        }

        i2c_stop_on_bus(iic_periph);
        
        // if (QL_IIC_STATUS_OK != iic->status)
        // {
        //     i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
        //     break;
        // }
        delay = QL_I2C_DELAY;
        while(I2C_CTL0(iic_periph) & I2C_CTL0_STOP)
        {
            if(0 == delay--)
            {
                iic->status = QL_IIC_STATUS_TIMEOUT;
                break;
            }
        }
        i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
    }while(0);

    if (QL_IIC_STATUS_OK == iic->status)
    {
        iic->rx_cnt += Size;
    }
    else
    {
        iic->err_er++;
    }
    status = iic->status;
    xSemaphoreGive(iic->sem);
    return status;
}

/*******************************************************************************
* Hayden @ 20240912
* Name: void Ql_IIC_I2C0_EV_IRQHandler(void)
* Brief: This function handle the I2C event interrupt.
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
void Ql_IIC_I2C0_EV_IRQHandler(void)
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t  iic_periph = Ql_IIC_HwGetPort(iic->mode);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* flag to sign whether the register address has been sent or not*/
    static bool is_reg_addr_sent = false;

    if (0 == iic_periph)
    {
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        return;
    }
    
    if( iic->rw == QL_IIC_READ)
    {
        /* if register address isn't 0 and hasn't been sent yet */
        if((iic->reg_addr > 0) && (false == is_reg_addr_sent))
        {
            if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_SBSEND))
            {
                i2c_master_addressing(iic_periph, iic->dev_addr, I2C_TRANSMITTER);
            }
            else if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_ADDSEND))
            {
                i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_ADDSEND);
            }
            else if((i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_TBE)) && (!i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_AERR)))
            {
                i2c_data_transmit(iic_periph, iic->reg_addr);
                is_reg_addr_sent = true;
            }
        }

        else if((i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_TBE)) && (!i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_AERR)))
        {
            i2c_start_on_bus(iic_periph);
        }

        else if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_SBSEND))
        {
            i2c_master_addressing(iic_periph, iic->dev_addr, I2C_RECEIVER);
        }
        else if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_ADDSEND))
        {
            if((1 == iic->rw_size) || (2 == iic->rw_size)) {
                /* clear the ACKEN before the ADDSEND is cleared */
                i2c_ack_config(iic_periph, I2C_ACK_DISABLE);
            }
            /* clear the ADDSEND bit */
            i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_ADDSEND);
        }
        else if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_RBNE))
        {
            if( iic->rw_nbytes > 0){
                if(3 ==  iic->rw_nbytes) {
                    /* wait until the second last data byte is received into the shift register */
                    while(!i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_BTC));
                    /* send a NACK for the last data byte */
                    i2c_ack_config(iic_periph, I2C_ACK_DISABLE);
                }
                /* read a data byte from I2C_DATA*/
                iic->rx_buf[iic->rw_size - iic->rw_nbytes] = i2c_data_receive(iic_periph);
                iic->rw_nbytes--;
                iic->rx_index++;
                if(1 == iic->rw_nbytes)
                {
                    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_BTC);
                }
                if(0 == iic->rw_nbytes){
                    /* send a stop condition */
                    i2c_stop_on_bus(iic_periph);
                    i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
                    i2c_ackpos_config(iic_periph, I2C_ACKPOS_CURRENT);
                    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_BTC);
                    i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_RBNE);
                    /* disable the I2CX interrupt */
                    Ql_IIC_HwItDisable();
                    iic->rw = QL_IIC_STOP;
                    iic->status = QL_IIC_STATUS_OK;
                    xEventGroupSetBitsFromISR(iic->event, VN_IIC_IT_RX_COMPLETE,xHigherPriorityTaskWoken);
                    is_reg_addr_sent = false;
                }
            }
        }
        else if (SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_STPDET))
        {
            i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_STPDET);
            /* send a stop condition */
            i2c_stop_on_bus(iic_periph);
            i2c_ack_config(iic_periph, I2C_ACK_ENABLE);
            i2c_ackpos_config(iic_periph, I2C_ACKPOS_CURRENT);
            i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_BTC);
            i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_RBNE);
            /* disable the I2CX interrupt */
            Ql_IIC_HwItDisable();
            iic->rw = QL_IIC_STOP;
            iic->status = QL_IIC_STATUS_OK;
            xEventGroupSetBitsFromISR(iic->event, VN_IIC_IT_RX_COMPLETE,xHigherPriorityTaskWoken);
            is_reg_addr_sent = false;
        }
        else
        {
            iic->err_ev++;
            Ql_IIC_HwItDisable();
        }
    }
    else if (iic->rw == QL_IIC_WRITE)
    {
        if(SET == i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_SBSEND))
        {
             i2c_master_addressing(iic_periph, iic->dev_addr, I2C_TRANSMITTER);
        }
        else if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_ADDSEND)) 
        {
            /* clear the ADDSEND bit */
            i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_ADDSEND);
        } 
        else if((i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_TBE)) && (!i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_AERR)))  
        {
            if((iic->reg_addr > 0) && (false == is_reg_addr_sent))
            {
                /* send the register address that need to be written to */
                i2c_data_transmit(iic_periph, iic->reg_addr);
                is_reg_addr_sent = true;
            }
            else if ((iic->rw_nbytes > 0))
            {
                /* send a data byte */
                i2c_data_transmit(iic_periph, iic->tx_buf[iic->rw_size - iic->rw_nbytes]);
                iic->rw_nbytes--;
            }
            else
            {
                /* send a stop condition */
                i2c_stop_on_bus(iic_periph);
                /* disable the I2CX interrupt */
                Ql_IIC_HwItDisable();
                iic->rw = QL_IIC_STOP;
                iic->status = QL_IIC_STATUS_OK;
                xEventGroupSetBitsFromISR(iic->event, VN_IIC_IT_TX_COMPLETE,xHigherPriorityTaskWoken);
                /* restore the flag to default when finish operation */
                is_reg_addr_sent = false;
            }
        }
        else
        {
            iic->err_ev++;
            Ql_IIC_HwItDisable();
        }
    }
    else 
    {
        iic->err_ev++;
        Ql_IIC_HwItDisable();
    }
}
/*******************************************************************************
* Hayden @ 20240912
* Name: void Ql_IIC_I2C0_ER_IRQHandler(void)
* Brief: This function handle the I2C0 error interrupt.
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
void Ql_IIC_I2C0_ER_IRQHandler(void) //                48:I2C0 Error
{
    IIC_Ins_TypeDef* iic = &iic_ins;
    uint32_t  iic_periph = Ql_IIC_HwGetPort(iic->mode);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (0 == iic_periph)
    {
        iic->status = QL_IIC_STATUS_MODE_ERROR;
        iic->rw_nbytes = 0;
        iic->rw = QL_IIC_STOP;
        return;
    }
    
    iic->err_er++;
    iic->status = QL_IIC_STATUS_IT_ERROR;
    
    i2c_stop_on_bus(iic_periph);

    /* no acknowledge received */
    if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_AERR)){
        i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_AERR);
        iic->status = QL_IIC_STATUS_NOACK;
        iic->err_nck++;
    }

    /* over-run or under-run when SCL stretch is disabled */
    if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_OUERR)){
        i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_OUERR);
        iic->status = QL_IIC_STATUS_OU_ERROR;
    }

    /* arbitration lost */
    if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_LOSTARB)){
        i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_LOSTARB);
        iic->status = QL_IIC_STATUS_LOSTARB_ERROR;
    }

    /* bus error */
    if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_BERR)){
        i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_BERR);
    }

    /* CRC value doesn't match */
    if(i2c_interrupt_flag_get(iic_periph, I2C_INT_FLAG_PECERR)){
        i2c_interrupt_flag_clear(iic_periph, I2C_INT_FLAG_PECERR);
        iic->status = QL_IIC_STATUS_CRC_ERROR;
    }

    iic->rw_nbytes = 0;
    iic->rw = QL_IIC_STOP;
    /* disable the error interrupt */
    Ql_IIC_HwItDisable();
    xEventGroupSetBitsFromISR(iic->event, VN_IIC_IT_RXTX_ERROR,xHigherPriorityTaskWoken);
}
/*******************************************************************************
* Name:  Ql_IIC_Status_TypeDef Ql_IIC_Init(void)
* Brief: This function initialize the IIC.
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
Ql_IIC_Status_TypeDef Ql_IIC_Init(IIC_Mode_TypeDef mode, IIC_Speed_TypeDef speed,uint32_t tx_size,uint32_t rx_size)
{
    Ql_IIC_Status_TypeDef status;
    IIC_Ins_TypeDef* iic = &iic_ins;
    BaseType_t xReturn;
    
    if (mode >= IIC_MODE_INVALID || speed >= IIC_SPEED_INVALID || tx_size == 0|| rx_size == 0)
    {
        return QL_IIC_STATUS_PARAM_ERROR;
    }

    iic->mode = mode;
    iic->speed = speed;

    if(NULL == iic->sem)
    {
        iic->sem  = xSemaphoreCreateBinary();
        if (iic->sem == NULL)
        {
            return QL_IIC_STATUS_MEMORY_ERROR;
        }
    }
    else
    {
        xReturn = xSemaphoreTake(iic->sem, portMAX_DELAY);
        if (xReturn == pdFAIL)
        {
            QL_LOG_E("Error in Ql_IIC_DeInit.");
            return QL_IIC_STATUS_SEM_ERROR;
        }
    }
    xSemaphoreGive(iic->sem);
    status = Ql_IIC_LowLevelInit(iic->mode, iic->speed,tx_size,rx_size);
    iic->init_cnt++;
    return status;
}
/*******************************************************************************
* Hayden @ 20240912
* Name: void Ql_IIC_DeInit(void)
* Brief: This function de-initialize the configurations for an QL_IIC port.
* Input: 
*   none.
* Return:
*   none.
*******************************************************************************/
void Ql_IIC_DeInit(void)
{
    IIC_Ins_TypeDef* iic = &iic_ins;

    if (NULL != iic->tx_buf)
    {
        vPortFree(iic->tx_buf);
        iic->tx_buf = NULL;
    }

    if(NULL != iic->rx_buf)
    {
        vPortFree(iic->rx_buf);
        iic->rx_buf = NULL;
    }
    
    if (IIC_MODE_SW_SIMULATE == iic->mode)
    {
        Ql_IIC_SwSimulateDeInit(iic->mode);
    }
    else if(IIC_MODE_HW0_INTERRUPT == iic->mode)
    {
        if (NULL != iic->event)
        {
            vEventGroupDelete(iic->event);
            iic->event = NULL;
        }
        Ql_IIC_HwDeInit(iic->mode);
    }
    else if (IIC_MODE_HW0_POLLING == iic->mode)
    {
        Ql_IIC_HwDeInit(iic->mode);
    }
    else
    {
        QL_LOG_E("Error in IIC mode:%d",iic->mode);
    }

    Ql_IIC_InstanceReset();

}
/*******************************************************************************
* Hayden @20240912
* Name: IIC_Ins_TypeDef* Ql_IIC_GetInstance(void)
* Brief: This function get the instance of QL_IIC.
* Input: 
*   none.
* Return:
*   the pointer of the instance.
*******************************************************************************/
IIC_Ins_TypeDef* Ql_IIC_GetInstance(void)
{
    return &iic_ins;
}
/******************************************************************************
 * Name: void Ql_IIC_CheckBusSatus(void)
 * brief: check i2c bus status
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
void Ql_IIC_CheckBusSatus(void)
{
    Ql_IIC_BusReset();
}

/***************************************************************************************
* Name: Ql_IIC_Read
* Brief: This function is the master through the IIC to Read data to the slave.
*        including the Slave address, Slave register address, Read data, Data size.
* Input: Parameters
*        Sla_Addr:
*        [In] Slave adress. The address format is xxxx xxxb (b = w/r) 
*        Reg_Addr:
*        [In] Slave register address, 0 indicates that there is no need to support register operations.
*        *pData:
*        [In] Data to be read.
*        Length:
*        [In] The size of the data to be written.
* Output:
*        None.
* Return:
*        IIC_ACK, This function succeeds.
*        IIC_NACK, The IIC failed to initialize.
***************************************************************************************/
Ql_IIC_Status_TypeDef Ql_IIC_Read(uint8_t Sla_Addr, uint8_t Reg_Addr, uint8_t *pData, const uint32_t Length,uint32_t timeout)
{
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_ERROR;
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    if(NULL == pData || 0 == Length)
    {
        return QL_IIC_STATUS_PARAM_ERROR;
    }

    if(IIC_MODE_HW0_INTERRUPT == iic->mode)
    {
        status = Ql_IIC_HwMasterItReceive(Sla_Addr,Reg_Addr,pData,Length, timeout);
    }
    else if(IIC_MODE_HW0_POLLING == iic->mode)
    {
        status = Ql_IIC_HwMasterReceive(Sla_Addr,Reg_Addr,pData,Length, timeout);
    }
    else if(IIC_MODE_SW_SIMULATE == iic->mode)
    {
        status = Ql_IIC_SwMasterReceive(Sla_Addr,Reg_Addr,pData,Length, timeout);
    }
    return status;
}

/***************************************************************************************
* Harry @ 20240830
*
* Name: Ql_IIC_Write
*
* Brief: This function is the master through the IIC to write data to the slave.
*        including the Slave address, Slave register address, Write data, Data size.
*
* Input: Parameters
*        Sla_Addr:
*        [In] Slave adress.The address format is xxxx xxxb (b = w/r)
*        Reg_Addr:
*        [In] Slave register address, 0 indicates that there is no need to support register operations.
*        *pData:
*        [In] Data to be written.
*        Length:
*        [In] The size of the data to be written.
*
* Output:
*        None.
*
* Return:
*        IIC_ACK, This function succeeds.
*        IIC_NACK, The IIC failed to initialize.
***************************************************************************************/
Ql_IIC_Status_TypeDef Ql_IIC_Write(uint8_t Sla_Addr, uint8_t Reg_Addr, uint8_t *pData, const uint32_t Length,uint32_t timeout)
{
    Ql_IIC_Status_TypeDef status = QL_IIC_STATUS_ERROR;
    IIC_Ins_TypeDef* iic = &iic_ins;
    
    if(NULL == pData || 0 == Length)
    {
        return QL_IIC_STATUS_PARAM_ERROR;
    }

    if(IIC_MODE_HW0_INTERRUPT == iic->mode)
    {
        status = Ql_IIC_HwMasterItTransmit( Sla_Addr,  Reg_Addr,  pData, Length, timeout);
    }
    else if(IIC_MODE_HW0_POLLING == iic->mode)
    {
        status = Ql_IIC_HwMasterTransmit( Sla_Addr,  Reg_Addr,  pData, Length, timeout);
    }
    else if(IIC_MODE_SW_SIMULATE == iic->mode)
    {
        status = Ql_IIC_SwMasterTransmit( Sla_Addr,  Reg_Addr,  pData, Length, timeout);
    }
    
    return status;
}
