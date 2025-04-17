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
  Name: ql_rtc.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

//-----include header file-----
#include "ql_rtc.h"
#define LOG_TAG "rtc"
#define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

//-----macro define -----
#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE    0x32F0

//-----static global variable-----
static rtc_parameter_struct rtc_initpara;
static __IO uint32_t prescaler_a = 0, prescaler_s = 0;

//-----static function declare-----
static void Ql_RTC_PreConfig(void);

//-----function define-----

/*******************************************************************************
* Hayden @ 20240815
* Name:  static void Ql_RTC_PreConfig(void)
* Brief: rtc clock configuration
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
static void Ql_RTC_PreConfig(void)
{
    #if defined (RTC_CLOCK_SOURCE_IRC32K) 
          rcu_osci_on(RCU_IRC32K);
          rcu_osci_stab_wait(RCU_IRC32K);
          rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
  
          prescaler_s = 0x13F;
          prescaler_a = 0x63;
    #elif defined (RTC_CLOCK_SOURCE_LXTAL)
          rcu_osci_on(RCU_LXTAL);
          rcu_osci_stab_wait(RCU_LXTAL);
          rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
    
          prescaler_s = 0xFF;
          prescaler_a = 0x7F;
    #else
    #error RTC clock source should be defined.
    #endif /* RTC_CLOCK_SOURCE_IRC32K */

    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}
/*******************************************************************************
* Hayden @ 20240815
* Name:  void Ql_RTC_Init(void)
* Brief: rtc init
* Input: 
*   void
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
void Ql_RTC_Init(void)
{
    /* enable PMU clock */
    rcu_periph_clock_enable(RCU_PMU);
    /* enable the access of the RTC registers */
    pmu_backup_write_enable();
    
    Ql_RTC_PreConfig();
    
    /* check if RTC has aready been configured */
    if (BKP_VALUE != RTC_BKP0)
    {
        rtc_initpara.factor_asyn    = prescaler_a;
        rtc_initpara.factor_syn     = prescaler_s;
        rtc_initpara.year           = 0x0;
        rtc_initpara.day_of_week    = RTC_SATURDAY;
        rtc_initpara.month          = RTC_JAN;
        rtc_initpara.date           = 0x1;
        rtc_initpara.display_format = RTC_24HOUR;
        rtc_initpara.am_pm          = RTC_AM;
        rtc_initpara.hour           = 0;
        rtc_initpara.minute         = 0;
        rtc_initpara.second         = 0;
        
        /* RTC current time configuration */
        if(ERROR == rtc_init(&rtc_initpara))
        {
            QL_LOG_E("rtc time configuration failed!");
        }
        else
        {
            RTC_BKP0 = BKP_VALUE;
        }
    }
}
 /*******************************************************************************
* Hayden @ 20240815
* Name:  void Ql_RTC_Calibration(struct tm *time)
* Brief: rtc calibration
* Input: 
*   struct tm *time
* Output:
*   void
* Return:
*   Void
*******************************************************************************/
void Ql_RTC_Calibration(struct tm *time)
{
    rtc_initpara.factor_asyn = prescaler_a;
    rtc_initpara.factor_syn  = prescaler_s;
    rtc_initpara.year   = DEC2BCD(time->tm_year + 1900 - 1980);
    rtc_initpara.month  = DEC2BCD(time->tm_mon+1);
    rtc_initpara.date   = DEC2BCD(time->tm_mday);
    rtc_initpara.hour   = DEC2BCD(time->tm_hour);
    rtc_initpara.minute = DEC2BCD(time->tm_min);
    rtc_initpara.second = DEC2BCD(time->tm_sec);
    rtc_init(&rtc_initpara);
}
 /*******************************************************************************
* Hayden @ 20240815
* Name:  int32_t Ql_RTC_Get(struct tm *time)
* Brief: get rtc time
* Input: 
*   struct tm *time
* Output:
*   struct tm *time
* Return:
*   Void
*******************************************************************************/
int32_t Ql_RTC_Get(struct tm *time)
{
    rtc_parameter_struct rtc;

    if (NULL == time)
    {
        return -1;
    }

    rtc_current_time_get(&rtc);
    
    time->tm_year = BCD2DEC(rtc.year) + 1980 - 1900;
    time->tm_mon  = BCD2DEC(rtc.month) - 1;
    time->tm_mday = BCD2DEC(rtc.date);
    time->tm_hour = BCD2DEC(rtc.hour);
    time->tm_min = BCD2DEC(rtc.minute);
    time->tm_sec = BCD2DEC(rtc.second);

    return 0;
}
