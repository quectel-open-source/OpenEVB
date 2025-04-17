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
  Name: ql_uart.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_UART_H_
#define __QL_UART_H_

#include "gd32f4xx.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#if 0
#define LOG_RCU_UART            RCU_USART2
#define LOG_RCU_PORT            RCU_GPIOB
#define LOG_UART                USART2
#define LOG_PORT                GPIOB
#define LOG_PORT_AF             GPIO_AF_7
#define LOG_PIN_TX              GPIO_PIN_10
#define LOG_PIN_RX              GPIO_PIN_11
#define LOG_UART_IRQ            USART2_IRQn
#define LOG_UART_IRQHANDLER     USART2_IRQHandler
#else
#define LOG_RCU_UART            RCU_USART5
#define LOG_RCU_PORT            RCU_GPIOC
#define LOG_UART                USART5
#define LOG_PORT                GPIOC
#define LOG_PORT_AF             GPIO_AF_8
#define LOG_PIN_TX              GPIO_PIN_6
#define LOG_PIN_RX              GPIO_PIN_7
#define LOG_UART_IRQ            USART5_IRQn
#define LOG_UART_IRQHANDLER     USART5_IRQHandler
#endif

#define USART_MALLOC            pvPortMalloc
#define USART_FREE              vPortFree

#define USART0_IRQ_PRI          configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART0_DMA_TX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART0_DMA_RX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define USART1_IRQ_PRI          configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART1_DMA_TX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART1_DMA_RX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define USART2_IRQ_PRI          configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART2_DMA_TX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define USART2_DMA_RX_IRQ_PRI   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define UART3_IRQ_PRI           configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART3_DMA_TX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART3_DMA_RX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define UART4_IRQ_PRI           configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART4_DMA_TX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART4_DMA_RX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define UART5_IRQ_PRI           configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART5_DMA_TX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART5_DMA_RX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

#define UART6_IRQ_PRI           configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART6_DMA_TX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0
#define UART6_DMA_RX_IRQ_PRI    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0

typedef enum
{
    USART_IRQ_IDLE = 0,
    USART_IRQ_TC,
} usart_irq_e;

typedef struct
{
    uint32_t                dma_periph;
    dma_channel_enum        channelx;
    dma_subperipheral_enum  sub_periph;
} usart_hard_t;

typedef struct
{
    uint32_t    baudrate;
    uint8_t     databits     : 4;   /* 5,6,7,8 */
    uint8_t     stopbits     : 4;   /* 1,2. 1.5 Not supported */
    uint8_t     parity       : 4;   /* 0:None; 1:Odd; 2:Even; 3:Mark; 4:Space */
    uint8_t     flow_control : 4;   /* 0:None; 1:Xon/Xoff; 2:Rts/Cts; 3:Dsr/Dtr */
} __attribute__((packed)) usart_regcfg_t;

typedef struct
{
    usart_regcfg_t  Reg;
    uint8_t         Recv_Debug;
} usart_cfg_t;

typedef struct
{
    uint32_t            usart_periph;
    uint32_t            baud;
    const char         *Name;
    uint8_t             Init;
    usart_cfg_t        *Cfg;
    const usart_hard_t *tx;
    const usart_hard_t *rx;
    SemaphoreHandle_t   Mutex;
    SemaphoreHandle_t   Recv_Sem;
    SemaphoreHandle_t   Send_Sem;
    uint8_t            *Recv_Buf;
    uint32_t            Recv_Buf_Size;
    uint32_t            Remain_Byte;
    uint32_t            Receive_Idx;
    uint32_t            Read_Idx;
    uint8_t            *Send_Buf;
    uint32_t            Send_Buf_Size;
    uint32_t            Send_Len;
    void              (*Irq_Callback)(usart_irq_e Irq_Flag);
} usart_manage_t;

int32_t Ql_Log_Uart_Init(const char *Name, uint32_t Baud);
int32_t Ql_Log_Uart_RxCB_Init(uint32_t (*Recv_CbFunc)(const uint8_t *Str, uint32_t Len));
int32_t Ql_Log_Uart_Output(const uint8_t *Str, uint32_t Size);

int32_t Ql_Uart2_Init(const char *Name, uint32_t Baud);
int32_t Ql_Uart2_RxCB_Init(uint32_t (*Recv_CbFunc)(const uint8_t *Str, uint32_t Len));
int32_t Ql_Uart2_Output(const uint8_t *Str, uint32_t Size);

char   *Ql_Uart_Name_Get(uint32_t UsartPeriph);
int32_t Ql_Uart_Init(const char *Name, uint32_t UsartPeriph, uint32_t Baud, uint32_t RecvBufSize, uint32_t SendBufSize);
int32_t Ql_Uart_DeInit(uint32_t UsartPeriph);
void    Ql_Uart_Irq_Register(uint32_t UsartPeriph, void (*Irq_Callback)(usart_irq_e Irq_Flag));
int32_t Ql_Uart_Open(uint32_t UsartPeriph, uint32_t Timeout);
int32_t Ql_Uart_Release(uint32_t UsartPeriph);
int32_t Ql_Uart_Read(uint32_t UsartPeriph, void* Src, uint16_t Size, uint32_t Timeout);
int32_t Ql_Uart_Write(uint32_t UsartPeriph, const void* Src, uint16_t Len, uint32_t Timeout);
usart_cfg_t *Ql_Uart_Cfg(uint32_t UsartPeriph);

#endif
