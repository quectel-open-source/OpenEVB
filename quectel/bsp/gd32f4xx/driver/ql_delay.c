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
  Name: ql_delay.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_delay.h"

__IO uint32_t uwTick = 0;

void Ql_IncTick(void)
{
    uwTick++;
}

uint32_t Ql_GetTick(void)
{
    return uwTick;
}

void Ql_DelayUS(uint32_t Millisecond)
{
    uint32_t ticks;
    uint32_t start_time, stop_time, cycle_count = 0;
    
    ticks = Millisecond * (SystemCoreClock / 1000000);
    start_time = SysTick->VAL;
    while (1)
    {
        stop_time = SysTick->VAL;
        if (stop_time != start_time)
        {
            if (stop_time < start_time)
            {
                cycle_count += start_time - stop_time;
            }
            else
            {
                cycle_count += SysTick->LOAD - stop_time + start_time;
            }

            start_time = stop_time;
            if (cycle_count >= ticks)
            {
                break;
            }
        }
    };
}
/*-----------------------------------------------------------*/

#define SYSTICK_1MS_TICKS       (SystemCoreClock / 1000)
#define SYSTICK_uS_PER_TICK     (1000L / SYSTICK_1MS_TICKS)
#define CALIBRATION_TICKS       (500000UL)

uint32_t _ticks_per_us = 8;

uint64_t getus(void)
{
    return ((uint64_t)uwTick * 1000 + ((SYSTICK_1MS_TICKS - 1 - SysTick->VAL) * SYSTICK_uS_PER_TICK));
}

static void _delay_loop(volatile uint32_t count)
{
    while(count--);
}

void softdelay_calibrate(void)
{
    uint64_t startUs = getus();
    _delay_loop(CALIBRATION_TICKS);
    uint64_t usedTime = getus() - startUs;

    if(!usedTime) usedTime = 1;

    _ticks_per_us = CALIBRATION_TICKS / usedTime;

    if(!_ticks_per_us) _ticks_per_us = 1;
}

void delay_us(uint32_t us)
{
    _delay_loop(us * _ticks_per_us);
}
