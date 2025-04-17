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
  Name: ql_utils.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_utils.h"
#include "ql_application.h"
#include "ql_rtc.h"

#include "ql_log_undef.h"
#define LOG_TAG "util"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

int month_days[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define SECONS_IN_DAY     ( 86400 )
#define LEAPYEAR(year)    ((year) % 4 == 0)
#define DAYS_IN_YEAR(a)   (LEAPYEAR(a) ? 366 : 365)
#define DAYS_IN_MONTH(a)  (month_days[(a) - 1])

int64_t Ql_ConvertTimeToSeconds(const CellularTime_t *Time)
{
    int32_t mon = Time->month;
    int32_t year = Time->year;
    int32_t days = 0;
    int32_t hours = 0;
    int64_t seconds = 0;

    mon -= 2;
    if (0 >= (int)mon)
    {
        mon += 12;
        year -= 1;
    }

    days = (unsigned long)(year / 4 - year / 100 + year / 400 +
        367 * mon / 12 + Time->day) + year * 365 - 719499;
    hours = days * 24 + Time->hour;

    seconds = (hours * 60 + Time->minute) * 60 + Time->second;
    return seconds;
}

int32_t Ql_ConvertSecondsToTime(const int32_t Seconds, CellularTime_t *Time)
{
    int32_t   i;
    int64_t   hms, day;

    day = Seconds / SECONS_IN_DAY;
    hms = Seconds % SECONS_IN_DAY;

    Time->hour = hms / 3600;
    Time->minute = (hms % 3600) / 60;
    Time->second = (hms % 3600) % 60;

    /* Number of years in days */
    for (i = 1970; day >= DAYS_IN_YEAR(i); i++)
    {
        day -= DAYS_IN_YEAR(i);
    }
    Time->year = i;

    /* Number of months in days left */
    if (LEAPYEAR(Time->year))
    {
        DAYS_IN_MONTH(2) = 29;
    }

    for (i = 1; day >= DAYS_IN_MONTH(i); i++)
    {
        day -= DAYS_IN_MONTH(i);
    }

    DAYS_IN_MONTH(2) = 28;
    Time->month = i;

    /* Days are what is left over (+1) from all that. */
    Time->day = day + 1;

    return 0;
}
