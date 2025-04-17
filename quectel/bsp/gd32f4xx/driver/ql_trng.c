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
  Name: ql_trng.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include <stdio.h>
#include "ql_trng.h"

#define LOG_TAG "trng"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

static ErrStatus trng_configuration(void);
static ErrStatus trng_ready_check(void);

void Ql_TRNG_Init(void)
{
    uint8_t retry = 0;

    #if 0 /* RCU_IRC48M will cause an error on the SD card */
    /* enable IRC48M clock */
    rcu_osci_on(RCU_IRC48M);
    /* wait until IRC48M is ready */
    while(ERROR == rcu_osci_stab_wait(RCU_IRC48M)) {
    }
    rcu_ck48m_clock_config(RCU_CK48MSRC_IRC48M);
    #endif

    /* configure TRNG module */
    while ((ERROR == trng_configuration()) && retry < 3)
    {
        QL_LOG_E("TRNG init fail \r\n");
        QL_LOG_E("TRNG init retry \r\n");
        retry++;
    }

    // QL_LOG_I("TRNG init ok \r\n");
    /* get the first random data */
    trng_get_true_random_data();
}
/*-----------------------------------------------------------*/

uint32_t uxRand(void)
{
    uint32_t random_data = 0;
    uint32_t random_lastdata = 0;

    /* check wherther the random data is valid and get it */
    if (SUCCESS == trng_ready_check())
    {
        random_data = trng_get_true_random_data();
        if (random_data != random_lastdata)
        {
            random_lastdata = random_data;
            QL_LOG_D("Get random data: 0x%08x", random_data);
        }
        else
        {
            /* the random data is invalid */
            QL_LOG_E("Error: Get the random data is same");
        }
    }

    return random_data;
}
/*-----------------------------------------------------------*/

/*!
    \brief      configure TRNG module
    \param[in]  none
    \param[out] none
    \retval     ErrStatus: SUCCESS or ERROR
*/
static ErrStatus trng_configuration(void)
{
    ErrStatus reval = SUCCESS;

    /* TRNG module clock enable */
    rcu_periph_clock_enable(RCU_TRNG);

    /* TRNG registers reset */
    trng_deinit();
    trng_enable();

    /* check TRNG work status */
    reval = trng_ready_check();

    return reval;
}

/*!
    \brief      check whether the TRNG module is ready
    \param[in]  none
    \param[out] none
    \retval     ErrStatus: SUCCESS or ERROR
*/
static ErrStatus trng_ready_check(void)
{
    uint32_t timeout = 0;
    FlagStatus trng_flag = RESET;
    ErrStatus reval = SUCCESS;

    /* check wherther the random data is valid */
    do
    {
        timeout++;
        trng_flag = trng_flag_get(TRNG_FLAG_DRDY);
    } while ((RESET == trng_flag) && (0xFFFF > timeout));

    if (RESET == trng_flag)
    {
        /* ready check timeout */
        QL_LOG_E("Error: TRNG can't ready \r\n");
        trng_flag = trng_flag_get(TRNG_FLAG_CECS);
        QL_LOG_E("Clock error current status: %d \r\n", trng_flag);
        trng_flag = trng_flag_get(TRNG_FLAG_SECS);
        QL_LOG_E("Seed error current status: %d \r\n", trng_flag);
        reval = ERROR;
    }

    /* return check status */
    return reval;
}
