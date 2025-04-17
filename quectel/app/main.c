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
  Name: main.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_application.h"
#include "ql_clock.h"

#define LOG_TAG "main"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

void Ql_AppStart(void * pvParameters);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x0800C000);
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    Ql_AppStart(NULL);

    vTaskStartScheduler();

    while (1);
}

/*-----------------------------------------------------------*/
/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
     * state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 * application must provide an implementation of vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE == 1 )

void vPreSleepProcessing( unsigned long ulExpectedIdleTime )
{
    /* Called by the kernel before it places the MCU into a sleep mode because
    configPRE_SLEEP_PROCESSING() is #defined to vPreSleepProcessing().

    NOTE:  Additional actions can be taken here to get the power consumption
    even lower.  For example, peripherals can be turned	off here, and then back
    on again in the post sleep processing function.  For maximum power saving
    ensure all unused pins are in their lowest power state. */

    /* Avoid compiler warnings about the unused parameter. */
    ( void ) ulExpectedIdleTime;

    // gpio_bit_write(GPIOB, GPIO_PIN_9, RESET);
}

void vPostSleepProcessing( unsigned long ulExpectedIdleTime )
{
    /* Called by the kernel when the MCU exits a sleep mode because
    configPOST_SLEEP_PROCESSING is #defined to vPostSleepProcessing(). */

    /* Avoid compiler warnings about the unused parameter. */
    ( void ) ulExpectedIdleTime;

    // gpio_bit_write(GPIOB, GPIO_PIN_9, SET);
}

#endif

#if ( configUSE_IDLE_HOOK == 1 )

void vApplicationIdleHook( void )
{

}

#endif

#if ( configCHECK_FOR_STACK_OVERFLOW > 0 )

void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    uint32_t *stack = NULL;

    extern uint32_t *vTaskStackAddr(void);
    extern uint32_t vTaskStackSize(void);
    extern volatile uint32_t *vTaskStackTop(void);

    taskDISABLE_INTERRUPTS();

    Ql_Printf("\r\n**************** StackOver! ****************\r\n");
    Ql_Printf("TaskName: %s\r\n", pcTaskName);
    Ql_Printf("pxCurrentTCB->pxStack:       0x%08X\r\n", vTaskStackAddr());
    Ql_Printf("pxCurrentTCB->pxTopOfStack:  0x%08X\r\n", vTaskStackTop());
    Ql_Printf("pxCurrentTCB->uxSizeOfStack: 0x%X * 4 = 0x%X Bytes\r\n", vTaskStackSize(), vTaskStackSize() * 4);
    Ql_Printf("pxBottomOfStack:             0x%08X\r\n", vTaskStackAddr() + vTaskStackSize());

    stack = vTaskStackAddr();
    for (uint16_t i = 0; i < vTaskStackSize(); i++)
    {
        if (i % 4 == 0)
        {
            Ql_Printf("\r\n0x%08X: ", (uint32_t)vTaskStackAddr() + i);
        }
        Ql_Printf("%08X ", stack[i]);
    }

    for (;;)
    {
    }
}
#endif
