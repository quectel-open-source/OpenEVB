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
  Name: ql_uart.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include <string.h>
#include <stdio.h>
#include "ql_uart.h"
#include "ql_delay.h"

#define LOG_TAG "uart"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

static usart_cfg_t Ql_Usart_Cfg[7];
static usart_manage_t *Ql_Usart_Manage[7] = { NULL };

static int32_t Ql_GetUsartID(uint32_t UsartPeriph);

#if 0
static uint32_t (*Ql_Log_Uart_RxCB)(const uint8_t *Str, uint32_t Len) = NULL;
#endif 

int32_t Ql_Log_Uart_Init(const char *Name, uint32_t Baud)
{
    const int32_t usart_id = Ql_GetUsartID(LOG_UART);
    usart_manage_t *usart = NULL;

    if (Ql_Usart_Manage[usart_id] == NULL)
    {
        usart = (usart_manage_t *)USART_MALLOC(sizeof(usart_manage_t));
        if (usart == NULL)
        {
            return -1;
        }

        memset(usart, 0, sizeof(usart_manage_t));
        usart->usart_periph  = LOG_UART;
        usart->Init          = 1;
        usart->Cfg           = &Ql_Usart_Cfg[usart_id];
        usart->rx            = NULL;
        usart->tx            = NULL;
        usart->Recv_Buf_Size = 0;
        usart->Irq_Callback  = NULL;

        Ql_Usart_Manage[usart_id] = usart;
    }
    else
    {
        if (Ql_Usart_Manage[usart_id]->Init)
        {
            return 0;
        }
    }

    usart->Name = Name;
    usart->baud = Baud;

    /* Enable UART clock */
    rcu_periph_clock_enable(LOG_RCU_PORT);
    rcu_periph_clock_enable(LOG_RCU_UART);

    gpio_af_set(LOG_PORT, LOG_PORT_AF, LOG_PIN_TX);
    gpio_af_set(LOG_PORT, LOG_PORT_AF, LOG_PIN_RX);

    /* Configure UART Rx */
    gpio_mode_set(LOG_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, LOG_PIN_RX);
    gpio_output_options_set(LOG_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LOG_PIN_RX);

    /* Configure UART Tx */
    gpio_mode_set(LOG_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, LOG_PIN_TX);
    gpio_output_options_set(LOG_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LOG_PIN_TX);

    usart_deinit(LOG_UART);
    usart_baudrate_set(LOG_UART, Baud);

    usart_interrupt_enable(LOG_UART, USART_INT_RBNE);

    usart_receive_config(LOG_UART, USART_RECEIVE_ENABLE);
    usart_transmit_config(LOG_UART, USART_TRANSMIT_ENABLE);

    nvic_irq_enable(LOG_UART_IRQ, configLIBRARY_LOWEST_INTERRUPT_PRIORITY, 0);

    usart_enable(LOG_UART);

    return 0;
}

int32_t Ql_Log_Uart_RxCB_Init(uint32_t (*Recv_CbFunc)(const uint8_t *Str, uint32_t Len))
{
    if (Recv_CbFunc == NULL)
    {
        return -1;
    }
		
#if 0
    Ql_Log_Uart_RxCB = Recv_CbFunc;
#endif
		
    return 0;
}

int32_t Ql_Log_Uart_Output(const uint8_t *Str, uint32_t Size)
{
#if 0
    for (uint32_t i = 0; i < Size; i++)
    {
        while (usart_flag_get(LOG_UART, USART_FLAG_TC) == RESET);
        usart_data_transmit(LOG_UART, (uint8_t)Str[i]);
    }
#else
    Ql_Uart_Write(USART5, Str, (uint16_t)Size, 20);
#endif
    return Size;
}

int fputc(int Ch, FILE *F)
{
    while (usart_flag_get(LOG_UART, USART_FLAG_TC) == RESET);
    usart_data_transmit(LOG_UART, (uint8_t)Ch);
    return Ch;
}

#if 0
void LOG_UART_IRQHANDLER(void)
{
    if (usart_interrupt_flag_get(LOG_UART, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t temp;
        
        temp = USART_STAT0(LOG_UART) & 0xff;
        temp = USART_DATA(LOG_UART)  & 0xff;
        
        if (Ql_Log_Uart_RxCB != NULL)
        {
            Ql_Log_Uart_RxCB(&temp, 1);
        }
    }
}
#endif

//-------------------------------------------------------------------------------

static uint32_t (*Ql_Uart2_RxCB)(const uint8_t *Str, uint32_t Len) = NULL;

int32_t Ql_Uart2_Init(const char *Name, uint32_t Baud)
{
    const int32_t usart_id = Ql_GetUsartID(USART2);
    usart_manage_t *usart = NULL;

    if (Ql_Usart_Manage[usart_id] == NULL)
    {
        usart = (usart_manage_t *)USART_MALLOC(sizeof(usart_manage_t));
        if (usart == NULL)
        {
            return -1;
        }

        memset(usart, 0, sizeof(usart_manage_t));
        usart->usart_periph  = USART2;
        usart->Init          = 1;
        usart->Cfg           = &Ql_Usart_Cfg[usart_id];
        usart->rx            = NULL;
        usart->tx            = NULL;
        usart->Recv_Buf_Size = 0;
        usart->Irq_Callback  = NULL;

        Ql_Usart_Manage[usart_id] = usart;
    }
    else
    {
        if (Ql_Usart_Manage[usart_id]->Init)
        {
            return 0;
        }
    }

    usart->Name = Name;
    usart->baud = Baud;

    /* Enable UART clock */
    rcu_periph_clock_enable(RCU_USART2);
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_af_set(GPIOB, GPIO_AF_7, GPIO_PIN_11);
    gpio_af_set(GPIOB, GPIO_AF_7, GPIO_PIN_10);

    /* Configure UART Rx */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_11);

    /* Configure UART Tx */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    usart_deinit(USART2);
    usart_baudrate_set(USART2, Baud);

    usart_interrupt_enable(USART2, USART_INT_RBNE);

    usart_receive_config(USART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);

    nvic_irq_enable(USART2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0);

    usart_enable(USART2);

    return 0;
}

int32_t Ql_Uart2_RxCB_Init(uint32_t (*Recv_CbFunc)(const uint8_t *Str, uint32_t Len))
{
    Ql_Uart2_RxCB = Recv_CbFunc;
    return 0;
}

int32_t Ql_Uart2_Output(const uint8_t *Str, uint32_t Size)
{
    for (uint32_t i = 0; i < Size; i++)
    {
        while (usart_flag_get(USART2, USART_FLAG_TC) == RESET);
        usart_data_transmit(USART2, (uint8_t)Str[i]);
    }

    return Size;
}

void USART2_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t temp;
        
        temp = USART_STAT0(USART2) & 0xff;
        temp = USART_DATA(USART2)  & 0xff;
        
        if (Ql_Uart2_RxCB != NULL)
        {
            Ql_Uart2_RxCB(&temp, 1);
        }
    }
}

//-------------------------------------------------------------------------------

const usart_hard_t usart_tx[7] = {
    { DMA1, DMA_CH7, DMA_SUBPERI4 },  // 0
    { DMA0, DMA_CH6, DMA_SUBPERI4 },  // 1
    { DMA0, DMA_CH3, DMA_SUBPERI4 },  // 2
    { DMA0, DMA_CH4, DMA_SUBPERI4 },  // 3
    { DMA0, DMA_CH7, DMA_SUBPERI4 },  // 4
    { DMA1, DMA_CH6, DMA_SUBPERI5 },  // 5
    { DMA0, DMA_CH1, DMA_SUBPERI5 }   // 6
};

const usart_hard_t usart_rx[7] = {
    { DMA1, DMA_CH2, DMA_SUBPERI4 },  // 0
    { DMA0, DMA_CH5, DMA_SUBPERI4 },  // 1
    { DMA0, DMA_CH1, DMA_SUBPERI4 },  // 2
    { DMA0, DMA_CH2, DMA_SUBPERI4 },  // 3
    { DMA0, DMA_CH0, DMA_SUBPERI4 },  // 4
    { DMA1, DMA_CH1, DMA_SUBPERI5 },  // 5
    { DMA0, DMA_CH3, DMA_SUBPERI5 }   // 6
};

/*****************************************************************************
* @brief  id
* ex:
* @par
* None
* @retval
*****************************************************************************/
static int32_t Ql_GetUsartID(uint32_t UsartPeriph)
{
    if (UsartPeriph == USART0) return 0;
    if (UsartPeriph == USART1) return 1;
    if (UsartPeriph == USART2) return 2;
    if (UsartPeriph == UART3)  return 3;
    if (UsartPeriph == UART4)  return 4;
    if (UsartPeriph == USART5) return 5;
    if (UsartPeriph == UART6)  return 6;
    return -1;
}

int8_t *Ql_Uart_NameGet(uint32_t UsartPeriph)
{
    if (UsartPeriph == USART0) return "USART0";
    if (UsartPeriph == USART1) return "USART1";
    if (UsartPeriph == USART2) return "USART2";
    if (UsartPeriph == UART3)  return "UART3";
    if (UsartPeriph == UART4)  return "UART4";
    if (UsartPeriph == USART5) return "USART5";
    if (UsartPeriph == UART6)  return "UART6";
    return NULL;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
int32_t Ql_Uart_Init(const char *Name, uint32_t UsartPeriph, uint32_t Baud,
                            uint32_t RecvBufSize, uint32_t SendBufSize)
{
    const int32_t usart_id = Ql_GetUsartID(UsartPeriph);
    usart_manage_t *usart = NULL;

    if (usart_id == -1)
    {
        return -1;
    }

    if (Ql_Usart_Manage[usart_id] == NULL)
    {
        usart = (usart_manage_t *)USART_MALLOC(sizeof(usart_manage_t));
        if (usart == NULL)
        {
            return -1;
        }

        memset(usart, 0, sizeof(usart_manage_t));
        usart->usart_periph  = UsartPeriph;
        usart->Init          = 1;
        usart->Cfg           = &Ql_Usart_Cfg[usart_id];
        usart->rx            = &usart_rx[usart_id];
        usart->tx            = &usart_tx[usart_id];
        usart->Recv_Buf_Size = RecvBufSize;
        usart->Send_Buf_Size = SendBufSize;
        usart->Irq_Callback  = NULL;

        /************************************************************************
        * 
        ************************************************************************/
        usart->Mutex = xSemaphoreCreateMutex();
        if (usart->Mutex == NULL)
        {
            goto _fail_create_mutex;
        }
        usart->Recv_Sem = xSemaphoreCreateBinary();
        if (usart->Recv_Sem == NULL)
        {
            goto _fail_create_recv_sem;
        }
        usart->Send_Sem = xSemaphoreCreateBinary();
        if (usart->Send_Sem == NULL)
        {
            goto _fail_create_send_sem;
        }
        // xSemaphoreGive(usart->Send_Sem);
        usart->Recv_Buf = (uint8_t *)USART_MALLOC(usart->Recv_Buf_Size);
        if (usart->Recv_Buf == NULL)
        {
            goto _fail_malloc_recv_buf;
        }
        if (usart->Send_Buf_Size > 0)
        {
            usart->Send_Buf = (uint8_t *)USART_MALLOC(usart->Send_Buf_Size);
            if (usart->Send_Buf == NULL)
            {
                goto _fail_malloc_send_buf;
            }
        }

        Ql_Usart_Manage[usart_id] = usart;
    }
    else
    {
        if (Ql_Usart_Manage[usart_id]->Init)
        {
            return 0;
        }
    }

    usart->Name = Name;
    usart->baud = Baud;

    /************************************************************************
    * 
    ************************************************************************/
    if (UsartPeriph == USART0)
    {
        rcu_periph_clock_enable(RCU_GPIOA);
        rcu_periph_clock_enable(RCU_DMA1);
        rcu_periph_clock_enable(RCU_USART0);

        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);
        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

        nvic_irq_enable(USART0_IRQn,        USART0_IRQ_PRI);
        nvic_irq_enable(DMA1_Channel7_IRQn, USART0_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA1_Channel2_IRQn, USART0_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == USART1)
    {
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_USART1);

        #if 0
        rcu_periph_clock_enable(RCU_GPIOA);

        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_2);
        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_3);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_3);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
        #else
        rcu_periph_clock_enable(RCU_GPIOD);

        gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_5);
        gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_6);

        gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_5);
        gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5);

        gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6);
        gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
        #endif

        nvic_irq_enable(USART1_IRQn,        USART1_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel6_IRQn, USART1_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel5_IRQn, USART1_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == USART2)
    {
        rcu_periph_clock_enable(RCU_GPIOB);
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_USART2);

        gpio_af_set(GPIOB, GPIO_AF_7, GPIO_PIN_10);
        gpio_af_set(GPIOB, GPIO_AF_7, GPIO_PIN_11);

        gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

        gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_11);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_11);

        nvic_irq_enable(USART2_IRQn,        USART2_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel3_IRQn, USART2_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel1_IRQn, USART2_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == UART3)
    {
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_UART3);

        #if 0
        rcu_periph_clock_enable(RCU_GPIOC);

        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_10);
        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_11);

        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_11);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_11);
        #else
        rcu_periph_clock_enable(RCU_GPIOA);

        gpio_af_set(GPIOA, GPIO_AF_8, GPIO_PIN_0);
        gpio_af_set(GPIOA, GPIO_AF_8, GPIO_PIN_1);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_0);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_1);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1);
        #endif

        nvic_irq_enable(UART3_IRQn,         UART3_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel4_IRQn, UART3_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel2_IRQn, UART3_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == UART4)
    {
        rcu_periph_clock_enable(RCU_GPIOC);
        rcu_periph_clock_enable(RCU_GPIOD);
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_UART4);

        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_12);
        gpio_af_set(GPIOD, GPIO_AF_8, GPIO_PIN_2);

        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_12);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);

        gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
        gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

        nvic_irq_enable(UART4_IRQn,         UART4_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel7_IRQn, UART4_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel0_IRQn, UART4_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == USART5)
    {
        /* Enable UART clock */
        rcu_periph_clock_enable(RCU_GPIOC);
        rcu_periph_clock_enable(RCU_USART5);
        rcu_periph_clock_enable(RCU_DMA1);

        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_6);
        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_7);

        /* Configure UART Rx */
        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_7);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

        /* Configure UART Tx */
        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
        
        nvic_irq_enable(USART5_IRQn,         UART5_IRQ_PRI);
        nvic_irq_enable(DMA1_Channel6_IRQn,  UART5_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA1_Channel1_IRQn,  UART5_DMA_RX_IRQ_PRI);
    }
    else if (UsartPeriph == UART6)
    {
        rcu_periph_clock_enable(RCU_GPIOF);
        rcu_periph_clock_enable(RCU_DMA0);
        rcu_periph_clock_enable(RCU_UART6);

        gpio_af_set(GPIOF, GPIO_AF_8, GPIO_PIN_7);
        gpio_af_set(GPIOF, GPIO_AF_8, GPIO_PIN_6);

        gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_7);
        gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

        gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6);
        gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

        nvic_irq_enable(UART6_IRQn,         UART6_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel1_IRQn, UART6_DMA_TX_IRQ_PRI);
        nvic_irq_enable(DMA0_Channel3_IRQn, UART6_DMA_RX_IRQ_PRI);
    }

    dma_single_data_parameter_struct dma_init_struct;
    dma_single_data_para_struct_init(&dma_init_struct);

    // DMA TX
    dma_deinit(usart->tx->dma_periph, usart->tx->channelx);
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.periph_addr         = ((uint32_t)&USART_DATA(UsartPeriph));
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority            = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(usart->tx->dma_periph, usart->tx->channelx, &dma_init_struct);
    dma_circulation_disable(usart->tx->dma_periph, usart->tx->channelx);
    dma_channel_subperipheral_select(usart->tx->dma_periph, usart->tx->channelx, usart->tx->sub_periph);
    dma_channel_disable(usart->tx->dma_periph, usart->tx->channelx);

    // DMA RX
    dma_deinit(usart->rx->dma_periph, usart->rx->channelx);
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority            = DMA_PRIORITY_ULTRA_HIGH;
    dma_init_struct.periph_addr         = ((uint32_t)&USART_DATA(UsartPeriph));
    dma_init_struct.memory0_addr        = (uint32_t)usart->Recv_Buf;
    dma_init_struct.number              = usart->Recv_Buf_Size;
    dma_single_data_mode_init(usart->rx->dma_periph, usart->rx->channelx, &dma_init_struct);
    dma_circulation_disable(usart->rx->dma_periph, usart->rx->channelx);
    dma_channel_subperipheral_select(usart->rx->dma_periph, usart->rx->channelx, usart->rx->sub_periph);
    dma_channel_enable(usart->rx->dma_periph, usart->rx->channelx);

    usart_deinit(UsartPeriph);
    usart_baudrate_set(UsartPeriph, usart->baud);
    USART_STAT0(UsartPeriph) = 0;

    usart_interrupt_enable(UsartPeriph, USART_INT_TC);
    // usart_interrupt_disable(usart_periph, USART_INT_RBNE);
    usart_interrupt_enable(UsartPeriph, USART_INT_IDLE);
    // dma_interrupt_enable(usart->tx->dma_periph, usart->tx->channelx, DMA_CHXCTL_FTFIE);
    dma_interrupt_enable(usart->rx->dma_periph, usart->rx->channelx, DMA_CHXCTL_FTFIE);

    usart_transmit_config(UsartPeriph, USART_TRANSMIT_ENABLE);
    usart_receive_config(UsartPeriph, USART_RECEIVE_ENABLE);

    usart_dma_transmit_config(UsartPeriph, USART_DENT_ENABLE);
    usart_dma_receive_config(UsartPeriph, USART_DENR_ENABLE);
    usart_enable(UsartPeriph);

    return 0;

_fail_malloc_send_buf:
    USART_FREE(usart->Recv_Buf);
_fail_malloc_recv_buf:
    vSemaphoreDelete(usart->Send_Sem);
_fail_create_send_sem:
    vSemaphoreDelete(usart->Recv_Sem);
_fail_create_recv_sem:
    vSemaphoreDelete(usart->Mutex);
_fail_create_mutex:
    USART_FREE(usart);

    return -1;
}

int32_t Ql_Uart_DeInit(uint32_t UsartPeriph)
{
    const int32_t usart_id = Ql_GetUsartID(UsartPeriph);
    usart_manage_t *usart = NULL;

    if (usart_id == -1)
    {
        return -1;
    }
    usart = Ql_Usart_Manage[usart_id];

    if (usart == NULL)
    {
        return 0;
    }

    if (usart->Init == 0)
    {
        return 0;
    }

    usart_disable(UsartPeriph);
    dma_interrupt_disable(usart->tx->dma_periph, usart->tx->channelx, DMA_CHXCTL_FTFIE);
    dma_interrupt_disable(usart->rx->dma_periph, usart->rx->channelx, DMA_CHXCTL_FTFIE);
    // usart_interrupt_disable(usart_periph, USART_INT_TC);
    // usart_interrupt_disable(usart_periph, USART_INT_RBNE);
    usart_interrupt_disable(UsartPeriph, USART_INT_IDLE);

    dma_channel_disable(usart->tx->dma_periph, usart->tx->channelx);
    dma_channel_disable(usart->rx->dma_periph, usart->rx->channelx);

    if (UsartPeriph == USART0)
    {
        nvic_irq_disable(USART0_IRQn);
        nvic_irq_disable(DMA1_Channel7_IRQn);
        nvic_irq_disable(DMA1_Channel2_IRQn);
        rcu_periph_clock_disable(RCU_USART0);
    }
    else if (UsartPeriph == USART1)
    {
        nvic_irq_disable(USART1_IRQn);
        nvic_irq_disable(DMA0_Channel6_IRQn);
        nvic_irq_disable(DMA0_Channel5_IRQn);
        rcu_periph_clock_disable(RCU_USART1);
    }
    else if (UsartPeriph == USART2)
    {
        nvic_irq_disable(USART2_IRQn);
        nvic_irq_disable(DMA0_Channel3_IRQn);
        nvic_irq_disable(DMA0_Channel1_IRQn);
        rcu_periph_clock_disable(RCU_USART2);
    }
    else if (UsartPeriph == UART3)
    {
        nvic_irq_disable(UART3_IRQn);
        nvic_irq_disable(DMA0_Channel4_IRQn);
        nvic_irq_disable(DMA0_Channel2_IRQn);
        rcu_periph_clock_disable(RCU_UART3);
    }
    else if (UsartPeriph == UART4)
    {
        nvic_irq_disable(UART4_IRQn);
        nvic_irq_disable(DMA0_Channel7_IRQn);
        nvic_irq_disable(DMA0_Channel0_IRQn);
        rcu_periph_clock_disable(RCU_UART4);
    }
    else if (UsartPeriph == USART5)
    {

    }

    return 0;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
void Ql_Uart_Irq_Register(uint32_t UsartPeriph, void (*Irq_Callback)(usart_irq_e Irq_Flag))
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UsartPeriph)];
    usart->Irq_Callback = Irq_Callback;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
int32_t Ql_Uart_Open(uint32_t UsartPeriph, uint32_t Timeout)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UsartPeriph)];

    if ((usart == NULL) || (usart->Mutex == NULL))
    {
        return -1;
    }

    if (xSemaphoreTake(usart->Mutex, Timeout) != pdPASS)
    {
        return -1;
    }

    return 0;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
int32_t Ql_Uart_Release(uint32_t UsartPeriph)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UsartPeriph)];

    if ((usart == NULL) || (usart->Mutex == NULL))
    {
        return -1;
    }

    if (xSemaphoreGive(usart->Mutex) != pdPASS)
    {
        return -1;
    }

    return 0;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
int32_t Ql_Uart_Read(uint32_t UsartPeriph, void* Src, uint16_t Size, uint32_t Timeout)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UsartPeriph)];
    uint32_t recv_count = 0;
    uint32_t read_bytes = 0;

    if (usart == NULL)
    {
        return 0;
    }

    if (Timeout > 0)
    {
        xSemaphoreTake(usart->Recv_Sem, Timeout);
    }

    usart->Remain_Byte = dma_transfer_number_get(usart->rx->dma_periph, usart->rx->channelx);
    usart->Receive_Idx = usart->Recv_Buf_Size - usart->Remain_Byte;
    if (usart->Read_Idx == usart->Receive_Idx)
    {
        return 0;
    }

    recv_count = (usart->Read_Idx < usart->Receive_Idx) ? (usart->Receive_Idx - usart->Read_Idx) : (usart->Recv_Buf_Size - usart->Read_Idx + usart->Receive_Idx);
    read_bytes = (Size > recv_count) ? recv_count : Size;
    if ((read_bytes + usart->Read_Idx) > usart->Recv_Buf_Size)
    {
        uint32_t tail_len = usart->Recv_Buf_Size - usart->Read_Idx;
        memcpy((uint8_t *)Src, usart->Recv_Buf + usart->Read_Idx, tail_len);
        memcpy((uint8_t *)Src + tail_len, usart->Recv_Buf, read_bytes - tail_len);
        usart->Read_Idx = read_bytes - tail_len;
    }
    else
    {
        memcpy(Src, usart->Recv_Buf + usart->Read_Idx, read_bytes);
        usart->Read_Idx += read_bytes;
    }

    // debug
    if (usart->Cfg->Recv_Debug == 2)
    {
        Ql_Log_Uart_Output(Src, read_bytes);
    }

    return read_bytes;
}

/*****************************************************************************
* @brief  
* ex:
* @par
* None
* @retval
*****************************************************************************/
int32_t Ql_Uart_Write(uint32_t UsartPeriph, const void* Src, uint16_t Len, uint32_t Timeout)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UsartPeriph)];

    if (usart == NULL)
    {
        return 0;
    }

    dma_channel_disable(usart->tx->dma_periph, usart->tx->channelx);
    dma_flag_clear(usart->tx->dma_periph, usart->tx->channelx, DMA_FLAG_FTF);

    if (usart->Send_Buf == NULL || usart->Send_Buf_Size == 0)
    {
        dma_memory_address_config(usart->tx->dma_periph, usart->tx->channelx, DMA_MEMORY_0, (uint32_t)Src);
        dma_transfer_number_config(usart->tx->dma_periph, usart->tx->channelx, Len);
    }
    else
    {
        if (usart->Send_Buf_Size >= Len)
        {
            memcpy(usart->Send_Buf, Src, Len);
            usart->Send_Len = Len;
        }
        else
        {
            QL_LOG_W("Send_Buf_Size(%d) < len(%d)", usart->Send_Buf_Size, Len);
            memcpy(usart->Send_Buf, Src, usart->Send_Buf_Size);
            usart->Send_Len = usart->Send_Buf_Size;
        }
        dma_memory_address_config(usart->tx->dma_periph, usart->tx->channelx, DMA_MEMORY_0, (uint32_t)usart->Send_Buf);
        dma_transfer_number_config(usart->tx->dma_periph, usart->tx->channelx, usart->Send_Len);
    }

    dma_channel_enable(usart->tx->dma_periph, usart->tx->channelx);

    if (xSemaphoreTake(usart->Send_Sem, Timeout) != pdPASS)
    {
        return 0;
    }

    return Len;
}

/*****************************************************************************
* @brief  UART CFG
* ex:
* @par
* None
* @retval
*****************************************************************************/
usart_cfg_t *Ql_Uart_Cfg(uint32_t UsartPeriph)
{
    int32_t usart_id = Ql_GetUsartID(UsartPeriph);

    if (usart_id < 0)
    {
        return NULL;
    }

    return &Ql_Usart_Cfg[usart_id];
}

/*****************************************************************************
* @brief  UART IRQ
* ex:
* @par
* None
* @retval
*****************************************************************************/
static inline void Ql_Uart_IrqHandler(usart_manage_t *Usart)
{
    static portBASE_TYPE xHigherPriorityTaskWoken;
    uint16_t temp;

    if (usart_interrupt_flag_get(Usart->usart_periph, USART_INT_FLAG_IDLE) != RESET)
    {
        temp = USART_STAT0(Usart->usart_periph);
        temp = USART_DATA(Usart->usart_periph);
        (void)temp;

        //--------------------------------------------------------------
        // debug
        if (Usart->Cfg->Recv_Debug == 1)
        {
            uint32_t read_bytes = 0;

            Usart->Remain_Byte = dma_transfer_number_get(Usart->rx->dma_periph, Usart->rx->channelx);
            Usart->Receive_Idx = Usart->Recv_Buf_Size - Usart->Remain_Byte;
            if (Usart->Read_Idx != Usart->Receive_Idx)
            {
                read_bytes = (Usart->Read_Idx < Usart->Receive_Idx) ?
                             (Usart->Receive_Idx - Usart->Read_Idx) :
                             (Usart->Recv_Buf_Size - Usart->Read_Idx + Usart->Receive_Idx);

                if ((read_bytes + Usart->Read_Idx) > Usart->Recv_Buf_Size)
                {
                    uint32_t tail_len = Usart->Recv_Buf_Size - Usart->Read_Idx;
                    Ql_Log_Uart_Output(Usart->Recv_Buf + Usart->Read_Idx, tail_len);
                    Ql_Log_Uart_Output(Usart->Recv_Buf, read_bytes - tail_len);
                }
                else
                {
                    Ql_Log_Uart_Output(Usart->Recv_Buf + Usart->Read_Idx, read_bytes);
                }
            }
        }
        //--------------------------------------------------------------

        if (Usart->Irq_Callback != NULL)
        {
            Usart->Irq_Callback(USART_IRQ_IDLE);
        }

        xSemaphoreGiveFromISR(Usart->Recv_Sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }

    if (usart_interrupt_flag_get(Usart->usart_periph, USART_INT_FLAG_TC) != RESET)
    {
        usart_interrupt_flag_clear(Usart->usart_periph, USART_INT_FLAG_TC);

        if (Usart->Irq_Callback != NULL)
        {
            Usart->Irq_Callback(USART_IRQ_TC);
        }

        xSemaphoreGiveFromISR(Usart->Send_Sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

/*****************************************************************************
* @brief  DMA Rx IRQ
* ex:
* @par
* None
* @retval
*****************************************************************************/
static inline void Ql_Uart_Dma_Recv_IrqHandler(usart_manage_t *Usart)
{
    if (dma_interrupt_flag_get(Usart->rx->dma_periph, Usart->rx->channelx, DMA_INT_FLAG_FTF) != RESET)
    {
        dma_interrupt_flag_clear(Usart->rx->dma_periph, Usart->rx->channelx, DMA_INT_FLAG_FTF);

        dma_channel_disable(Usart->rx->dma_periph, Usart->rx->channelx);
        dma_transfer_number_config(Usart->rx->dma_periph, Usart->rx->channelx, Usart->Recv_Buf_Size);
        dma_channel_enable(Usart->rx->dma_periph, Usart->rx->channelx);
    }
}

/*****************************************************************************
* @brief  DMA Tx IRQ
* ex:
* @par
* None
* @retval
*****************************************************************************/
static inline void Ql_Uart_Dma_Send_IrqHandler(usart_manage_t *Usart)
{
//    static portBASE_TYPE xHigherPriorityTaskWoken;
//    
//    if (dma_interrupt_flag_get(usart->tx->dma_periph, usart->tx->channelx, DMA_INT_FLAG_FTF) != RESET)
//    {
//        dma_interrupt_flag_clear(usart->tx->dma_periph, usart->tx->channelx, DMA_INT_FLAG_FTF);
//        
//        xSemaphoreGiveFromISR(usart->Send_Sem, &xHigherPriorityTaskWoken);
//        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
//    }
}

/*****************************************************************************
* @brief  0
* ex:
* @par
* None
* @retval
*****************************************************************************/
void USART0_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART0)];
    Ql_Uart_IrqHandler(usart);
}

void DMA1_Channel2_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART0)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA1_Channel7_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART0)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}

/*****************************************************************************
* @brief  1
* ex:
* @par
* None
* @retval
*****************************************************************************/
void USART1_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART1)];
    Ql_Uart_IrqHandler(usart);
}

void DMA0_Channel5_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART1)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA0_Channel6_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART1)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}

#if 0
/*****************************************************************************
* @brief  2
* ex:
* @par
* None
* @retval
*****************************************************************************/
void USART2_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART2)];
    Ql_Uart_IrqHandler(usart);
}

void DMA0_Channel1_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART2)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA0_Channel3_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART2)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}
#endif

/*****************************************************************************
* @brief  3
* ex:
* @par
* None
* @retval
*****************************************************************************/
void UART3_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART3)];
    Ql_Uart_IrqHandler(usart);
}

void DMA0_Channel2_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART3)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA0_Channel4_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART3)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}

/*****************************************************************************
* @brief  4
* ex:
* @par
* None
* @retval
*****************************************************************************/
void UART4_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART4)];
    Ql_Uart_IrqHandler(usart);
}

void DMA0_Channel0_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART4)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA0_Channel7_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART4)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}

#if 1
/*****************************************************************************
* @brief  5
* ex:
* @par
* None
* @retval
*****************************************************************************/
void USART5_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART5)];
    Ql_Uart_IrqHandler(usart);
}

void DMA1_Channel1_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART5)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA1_Channel6_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(USART5)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}
#endif

/*****************************************************************************
* @brief  6
* ex:
* @par
* None
* @retval
*****************************************************************************/
void UART6_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART6)];
    Ql_Uart_IrqHandler(usart);
}

void DMA0_Channel3_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART6)];
    Ql_Uart_Dma_Recv_IrqHandler(usart);
}

void DMA0_Channel1_IRQHandler(void)
{
    usart_manage_t *usart = Ql_Usart_Manage[Ql_GetUsartID(UART6)];
    Ql_Uart_Dma_Send_IrqHandler(usart);
}

//---------------------------------------------------------------------

