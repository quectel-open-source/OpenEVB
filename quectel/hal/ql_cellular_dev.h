#ifndef __QL_EC600U_H_
#define __QL_EC600U_H_

#include "gd32f4xx.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define EC600U_UART_LINK_PORT       GPIOA
#define EC600U_UART_LINK_PIN        GPIO_PIN_6

#define EC600U_PWRKEY_PORT          GPIOA
#define EC600U_PWRKEY_PIN           GPIO_PIN_7
#define EC600U_RESET_N_PORT         GPIOB
#define EC600U_RESET_N_PIN          GPIO_PIN_1
#define EC600U_USB_BOOT_PORT        GPIOB
#define EC600U_USB_BOOT_PIN         GPIO_PIN_0

extern uint8_t Ql_EC600U_GnssComSelect;

int      Ql_EC600U_Init(void);
int      Ql_EC600U_DeInit(void);
int      Ql_EC600U_Open(void);
int      Ql_EC600U_Release(void);
void     Ql_EC600U_PowerOn(void);
void     Ql_EC600U_PowerOff(void);
void     Ql_EC600U_Reset(void);
uint32_t Ql_EC600U_Read(uint8_t *Buf, uint32_t Size, uint32_t Timeout);
uint32_t Ql_EC600U_Write(const uint8_t *Buf, uint32_t Len, uint32_t Timeout);

#endif
