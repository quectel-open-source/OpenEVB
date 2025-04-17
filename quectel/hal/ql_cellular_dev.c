#include "ql_cellular_dev.h"
#include "ql_uart.h"
#include <string.h>

#define QL_EC600U_UART  USART1

uint8_t Ql_EC600U_GnssComSelect = 0;

static int32_t isInit = -1;

static void Ql_EC600U_IRQ_CallBack(usart_irq_e Flag);

int Ql_EC600U_Init( void )
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    /* 4G UART Connection - PA6 */
    gpio_mode_set(EC600U_UART_LINK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, EC600U_UART_LINK_PIN);
    gpio_output_options_set(EC600U_UART_LINK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, EC600U_UART_LINK_PIN);

    /* 4G Power - PA7 */
    gpio_mode_set(EC600U_PWRKEY_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, EC600U_PWRKEY_PIN);
    gpio_output_options_set(EC600U_PWRKEY_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, EC600U_PWRKEY_PIN);

    /* 4G Reset - PB1 */
    gpio_mode_set(EC600U_RESET_N_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, EC600U_RESET_N_PIN);
    gpio_output_options_set(EC600U_RESET_N_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, EC600U_RESET_N_PIN);

    /* 4G Boot - PB0 */
    gpio_mode_set(EC600U_USB_BOOT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, EC600U_USB_BOOT_PIN);
    gpio_output_options_set(EC600U_USB_BOOT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, EC600U_USB_BOOT_PIN);

    gpio_bit_write(EC600U_UART_LINK_PORT, EC600U_UART_LINK_PIN, Ql_EC600U_GnssComSelect ? SET : RESET);
    gpio_bit_write(EC600U_PWRKEY_PORT,    EC600U_PWRKEY_PIN,    SET);
    gpio_bit_write(EC600U_RESET_N_PORT,   EC600U_RESET_N_PIN,   SET);
    gpio_bit_write(EC600U_USB_BOOT_PORT,  EC600U_USB_BOOT_PIN,  RESET);

    if (isInit == 0)
    {
        return 0;
    }

    Ql_Uart_Init("Cellular", QL_EC600U_UART, Ql_Uart_Cfg(QL_EC600U_UART)->Reg.baudrate, 1024 * 4, 0);
    Ql_Uart_Irq_Register(QL_EC600U_UART, Ql_EC600U_IRQ_CallBack);

    isInit = 0;

    return 0;
}

int Ql_EC600U_DeInit(void)
{
    return 0;
}

int Ql_EC600U_Open(void)
{
    return 0;
}

int Ql_EC600U_Release(void)
{
    return 0;
}

void Ql_EC600U_PowerOn(void)
{
//    gpio_bit_write(EC600U_PWRKEY_PORT, EC600U_PWRKEY_PIN, RESET);
//    gpio_bit_write(EC600U_RESET_N_PORT, EC600U_RESET_N_PIN, RESET);
//    vTaskDelay(100 / portTICK_RATE_MS);
//    gpio_bit_write(EC600U_USB_BOOT_PORT, EC600U_USB_BOOT_PIN, SET);
//    
//    vTaskDelay(500 / portTICK_RATE_MS);
//    
//    gpio_bit_write(EC600U_PWRKEY_PORT, EC600U_PWRKEY_PIN, SET);
//    vTaskDelay(2100 / portTICK_RATE_MS);
//    gpio_bit_write(EC600U_PWRKEY_PORT, EC600U_PWRKEY_PIN, RESET);
}

void Ql_EC600U_PowerOff(void)
{
//    gpio_bit_write(EC600U_PWRKEY_PORT, EC600U_PWRKEY_PIN, SET);
//    vTaskDelay(3100 / portTICK_RATE_MS);
//    gpio_bit_write(EC600U_PWRKEY_PORT, EC600U_PWRKEY_PIN, RESET);
}

void Ql_EC600U_Reset(void)
{
//    Ql_Pin_SetLevel(CELL_POWER_PIN, SET);
//    Ql_Pin_SetLevel(CELL_RESET_N_PIN, RESET);
//    vTaskDelay(500 / portTICK_RATE_MS);
//    
//    Ql_Pin_SetLevel(CELL_RESET_N_PIN, SET);
//    vTaskDelay(200 / portTICK_RATE_MS);
//    Ql_Pin_SetLevel(CELL_RESET_N_PIN, RESET);
//    vTaskDelay(3100 / portTICK_RATE_MS);
}

uint32_t Ql_EC600U_Read(uint8_t *Buf, uint32_t Size, uint32_t Timeout)
{
    if ((Buf == NULL) || (Size == 0))
    {
        return 0;
    }

    Size = Ql_Uart_Read(QL_EC600U_UART, Buf, Size, Timeout);
    return Size;
}

uint32_t Ql_EC600U_Write(const uint8_t *Buf, uint32_t Len, uint32_t Timeout)
{
    if ((Buf == NULL) || (Len == 0))
    {
        return 0;
    }

    Len = Ql_Uart_Write(QL_EC600U_UART, Buf, Len, Timeout);
    return Len;
}

static void Ql_EC600U_IRQ_CallBack(usart_irq_e Flag)
{
    if (Flag == USART_IRQ_IDLE)
    {
        extern uint32_t prvProcessUartInt( void );
        prvProcessUartInt();
    }
}
