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
  Name: ql_application.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_APPLICATION_H__
#define __QL_APPLICATION_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#ifdef __EXAMPLE_GNSS__
#include "example_def.h"
#else
#include "test_def.h"
#endif
#include "ql_ctype.h"

#define EVENTBIT(bit)                       (1 << bit)

//for Ql_NtripClientLoopEvent
#define NTRIP_RTK_EVENT_CONN                EVENTBIT(0)
#define NTRIP_RTK_EVENT_RECONN              EVENTBIT(1)
#define NTRIP_RTK_EVENT_CLOSE               EVENTBIT(2)

/* WorkMode */
#define NONE_STATION_MODE                   (0U)
#define BASE_STATION_MODE                   (1U)
#define ROVER_STATION_MODE                  (2U)

#define SIZEOF_CHAR_NUL                     (1U)

#define CELLULAR_SETUP_SUCC_BIT             (0x01)
#define CELLULAR_SETUP_FAIL_BIT             (0x02)

#define CELLULAR_IMEI_IMSI_MAX_SIZE         (15U)
#define CELLULAR_MCC_MNC_MAX_SIZE           (3U)
#define CELLULAR_ICCID_MAXSIZE              (20U)
#define CELLULAR_SN_MAX_SIZE                (17U)
#define CELLULAR_FW_VER_MAX_SIZE            (40U) //same to CELLULAR_FW_VERSION_MAX_SIZE
#define CELLULAR_MODELID_MAX_SIZE           (10U) //same to CELLULAR_MODEL_ID_MAX_SIZE
typedef enum
{
    QL_EC600U = 0,   //QL_EC600U_CN,QL_EC600U_EU
    QL_EG25   = 1,   //EG25-G
} Ql_CellModelId_Typedef;

typedef struct
{
    Ql_CellModelId_Typedef modelID;
    char* model_str;
} Ql_CellModel_t;

typedef struct {
    /* General */
    uint8_t                CellSyncRTC;
    uint8_t                CellularNetReg; //for checking cellular network register status

    uint8_t                WorkMode;
    uint8_t                WorkMode_Pre;

    /* Time */
    char                   UTC_Time[BUFFSIZE32];
    /* Version */
    char                   BuildVersion[BUFFSIZE64];

    uint8_t                Debug;
} Ql_System_TypeDef;
extern Ql_System_TypeDef * const Ql_SystemPtr;

typedef struct {
    int                    Signal_4g;

    char                   ICCID[CELLULAR_ICCID_MAXSIZE + SIZEOF_CHAR_NUL];
    char                   IMSI[CELLULAR_IMEI_IMSI_MAX_SIZE + SIZEOF_CHAR_NUL];
    char                   MCC[CELLULAR_MCC_MNC_MAX_SIZE + SIZEOF_CHAR_NUL];
    char                   MNC[CELLULAR_MCC_MNC_MAX_SIZE + SIZEOF_CHAR_NUL];
    char                   SN[CELLULAR_SN_MAX_SIZE + SIZEOF_CHAR_NUL];
    char                   IMEI[CELLULAR_IMEI_IMSI_MAX_SIZE + SIZEOF_CHAR_NUL];

    char                   FirmwareVer[CELLULAR_FW_VER_MAX_SIZE + SIZEOF_CHAR_NUL];
    char                   Model[CELLULAR_MODELID_MAX_SIZE + 1];

    Ql_CellModelId_Typedef ModelId;
} Ql_CellularInfo_TypeDef;
extern Ql_CellularInfo_TypeDef * const Ql_CellularInfoPtr;

typedef struct {
    Ql_System_TypeDef              System;
    Ql_CellularInfo_TypeDef        CellularInfo;
} Ql_Monitor_TypeDef;

#endif

