/*
 * FreeRTOS V202112.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

// Change Log
// 2024-06, Quectel GNSS: Modify and add some code for this project

/**
 * @file cellular_setup.c
 * @brief Setup cellular connectivity for board with cellular module.
 */

/* FreeRTOS include. */
#include <FreeRTOS.h>
#include "task.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Demo Specific configs. */
//#include "demo_config.h"

#define LOG_TAG "cell_setup"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

/* The config header is always included first. */
#ifndef CELLULAR_DO_NOT_USE_CUSTOM_CONFIG
    /* Include custom config file before other headers. */
    #include "cellular_config.h"
#endif
#include "cellular_config_defaults.h"
#include "cellular_types.h"
#include "cellular_api.h"
#include "cellular_comm_interface.h"

#include "sockets_wrapper.h"
#include "ql_application.h"

#ifndef CELLULAR_APN
    #error "CELLULAR_APN is not defined in cellular_config.h"
#endif

#define CELLULAR_SIM_CARD_WAIT_INTERVAL_MS       ( 500UL )
#define CELLULAR_MAX_SIM_RETRY                   ( 5U )

#define CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS    ( 1000UL )

/*-----------------------------------------------------------*/

bool IsCellSimReady = true;


/* the default Cellular comm interface in system. */
extern CellularCommInterface_t CellularCommInterface;

/*-----------------------------------------------------------*/

/* Secure socket needs application to provide the cellular handle and pdn context id. */
/* User of secure sockets cellular should provide this variable. */
CellularHandle_t CellularHandle = NULL;

/* User of secure sockets cellular should provide this variable. */
uint8_t CellularSocketPdnContextId = CELLULAR_PDN_CONTEXT_ID;

extern CellularError_t Cellular_ModuleEnableUE( CellularContext_t * pContext );
extern CellularError_t Cellular_ModuleEnableUrc( CellularContext_t * pContext );

extern CellularHandle_t CellularHandle;

Ql_CellModel_t Ql_CellModel_Table[] =
{
    {QL_EC600U, "EC600U"},
    {QL_EG25,   "EG25"},
};

uint32_t Ql_CellModelTableSize = sizeof( Ql_CellModel_Table ) / sizeof( Ql_CellModel_t );

bool Ql_SetCellModleID(void)
{
    if(0 == strlen(Ql_CellularInfoPtr->Model))
    {
        return false;
    }

    for(int i = 0;i < Ql_CellModelTableSize;i++)
    {
        if(!strncmp(Ql_CellModel_Table[i].model_str,Ql_CellularInfoPtr->Model,strlen(Ql_CellularInfoPtr->Model)))
        {
            Ql_CellularInfoPtr->ModelId = Ql_CellModel_Table[i].modelID;

            return true;
        }
    }

    QL_LOG_E("no macthed modelId for [%s]",Ql_CellularInfoPtr->Model);

    return false;
}
CellularError_t Cellular_GetSimStatus( CellularHandle_t cellularHandle)
{
    CellularError_t status = CELLULAR_SUCCESS;
    CellularSimCardStatus_t sim_status;
    uint8_t tries = 0;

    if( status == CELLULAR_SUCCESS )
    {
        /* wait until SIM is ready */
        for( tries = 0; tries < CELLULAR_MAX_SIM_RETRY; tries++ )
        {
            status = Cellular_GetSimCardStatus( cellularHandle, &sim_status );
            if((status == CELLULAR_SUCCESS)
                && ((sim_status.simCardState == CELLULAR_SIM_CARD_INSERTED)
                && (sim_status.simCardLockState == CELLULAR_SIM_CARD_READY)))
            {
                QL_LOG_I("Cellular SIM OK");
                break;
            }
            else
            {
                QL_LOG_E("Cellular SIM card state %d, Lock State %d",
                          sim_status.simCardState,sim_status.simCardLockState);
            }

            vTaskDelay(pdMS_TO_TICKS(CELLULAR_SIM_CARD_WAIT_INTERVAL_MS));
            if(CELLULAR_MAX_SIM_RETRY - 1 == tries)
            {
                status = CELLULAR_UNKNOWN;
            }
        }

    }

    return status;
}

CellularError_t Ql_Cellular_GetMoudleInfo( CellularHandle_t cellularHandle)
{
    CellularError_t status = CELLULAR_SUCCESS;
    CellularModemInfo_t modem_info = {0};

    status = Cellular_GetModemInfo( cellularHandle, &modem_info );
    if( status == CELLULAR_SUCCESS )
    {
        memset(Ql_CellularInfoPtr->SN,0,sizeof(Ql_CellularInfoPtr->SN));
        memset(Ql_CellularInfoPtr->IMEI,0,sizeof(Ql_CellularInfoPtr->IMEI));
        memset(Ql_CellularInfoPtr->FirmwareVer,0,sizeof(Ql_CellularInfoPtr->FirmwareVer));
        memset(Ql_CellularInfoPtr->Model,0,sizeof(Ql_CellularInfoPtr->Model));

        strncpy(Ql_CellularInfoPtr->SN,modem_info.serialNumber,sizeof(Ql_CellularInfoPtr->SN));
        strncpy(Ql_CellularInfoPtr->IMEI,modem_info.imei,sizeof(Ql_CellularInfoPtr->IMEI));
        strncpy(Ql_CellularInfoPtr->FirmwareVer,modem_info.firmwareVersion,sizeof(Ql_CellularInfoPtr->FirmwareVer));
        strncpy(Ql_CellularInfoPtr->Model,modem_info.modelId,sizeof(Ql_CellularInfoPtr->Model));

        QL_LOG_I("SN[%s],IMEI[%s]",Ql_CellularInfoPtr->SN,Ql_CellularInfoPtr->IMEI);
        QL_LOG_I("modelId[%s],ver[%s]",Ql_CellularInfoPtr->Model,Ql_CellularInfoPtr->FirmwareVer);
    }

    return status;
}

CellularError_t Ql_Cellular_Init( void )
{
    CellularError_t status = CELLULAR_SUCCESS;
    CellularCommInterface_t * pCommIntf = &CellularCommInterface;

    status = Cellular_Init( &CellularHandle, pCommIntf );

    return status;
}

bool setupCellular( void )
{
    bool ret = true;
    CellularError_t status = CELLULAR_SUCCESS;
    CellularServiceStatus_t service_status;
    CellularPdnStatus_t pdn_status = {0};
    uint32_t timeout_count_limit = (CELLULAR_PDN_CONNECT_TIMEOUT / CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS) + 1U;
    uint32_t timeout_count = 0;
    uint8_t num_status = 1;
    uint32_t rssi;
    CellularPlmnInfo_t net_info = {0};
    CellularSimCardInfo_t simcard_info = {0};
    CellularPdnConfig_t pdn_config = 
    { 
        CELLULAR_PDN_CONTEXT_IPV4,
        CELLULAR_PDN_AUTH_NONE,
        CELLULAR_APN, 
        "",
        "" 
    };

    /* Initialize Cellular Comm Interface. */
//    status = Cellular_Init( &CellularHandle, pCommIntf );

    /* Setup UE, URC and query register status. */
    status = Cellular_ModuleEnableUE(CellularHandle);
    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_ModuleEnableUrc(CellularHandle);
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Ql_Cellular_GetMoudleInfo(CellularHandle);
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_GetSimStatus(CellularHandle);
        if(status == CELLULAR_SUCCESS)
        {
            IsCellSimReady = true;
        }
        else
        {
            IsCellSimReady = false;
            QL_LOG_E( "Cellular SIM failed" );
        }
    }

    /* Setup the PDN config. */
    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_SetPdnConfig(CellularHandle, CellularSocketPdnContextId, &pdn_config);
        if( status != CELLULAR_SUCCESS )
        {
            QL_LOG_E( "Set pdn failed" );
        }
    }

    /* Rescan network. */
    if(status == CELLULAR_SUCCESS)
    {
        IsCellSimReady = true;
        status = Cellular_RfOff(CellularHandle);
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_RfOn(CellularHandle);
    }

    if(status == CELLULAR_SUCCESS)
    {
        status =  Cellular_GetSimCardInfo(CellularHandle,&simcard_info);
        if(status == CELLULAR_SUCCESS)
        {
            memset(Ql_CellularInfoPtr->ICCID,0,CELLULAR_ICCID_MAXSIZE + 1);
            memset(Ql_CellularInfoPtr->IMSI,0,CELLULAR_IMEI_IMSI_MAX_SIZE + 1);

            strncpy(Ql_CellularInfoPtr->ICCID,simcard_info.iccid,CELLULAR_ICCID_MAXSIZE + 1);
            strncpy(Ql_CellularInfoPtr->IMSI,simcard_info.imsi,CELLULAR_IMEI_IMSI_MAX_SIZE + 1);

            QL_LOG_I("ICCID[%s],IMSI[%s]",Ql_CellularInfoPtr->ICCID,Ql_CellularInfoPtr->IMSI);
        }
    }

    /* Get service status. */
    if(status == CELLULAR_SUCCESS)
    {
        while( timeout_count < timeout_count_limit )
        {
            status = Cellular_GetServiceStatus(CellularHandle, &service_status);
            if((status == CELLULAR_SUCCESS)
                && ((service_status.psRegistrationStatus == REGISTRATION_STATUS_REGISTERED_HOME)
                    ||(service_status.psRegistrationStatus == REGISTRATION_STATUS_ROAMING_REGISTERED)))
            {
                Ql_SystemPtr->CellularNetReg = true;
                QL_LOG_I("Cellular module registered");

                break;
            }
            else
            {
                QL_LOG_W("GetServiceStatus failed %d, ps reg status %d",
                          status, service_status.psRegistrationStatus);
            }

            timeout_count++;

            if(timeout_count >= timeout_count_limit)
            {
                Ql_SystemPtr->CellularNetReg = false;
                status = CELLULAR_INVALID_HANDLE;
                QL_LOG_E("Cellular module can't be registered");
            }

            vTaskDelay(pdMS_TO_TICKS(CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS / 2));
        }
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_GetRegisteredNetwork(CellularHandle,&net_info);
    }

    if(status == CELLULAR_SUCCESS)
    {
        memset(Ql_CellularInfoPtr->MCC,0,sizeof(Ql_CellularInfoPtr->MCC));
        memset(Ql_CellularInfoPtr->MNC,0,sizeof(Ql_CellularInfoPtr->MNC));

        strncpy(Ql_CellularInfoPtr->MCC,net_info.mcc,sizeof(Ql_CellularInfoPtr->MCC));
        strncpy(Ql_CellularInfoPtr->MNC,net_info.mnc,sizeof(Ql_CellularInfoPtr->MNC));

        QL_LOG_I("MCC[%s],MNC[%s]",Ql_CellularInfoPtr->MCC,Ql_CellularInfoPtr->MNC);

        if(false == Ql_SetCellModleID())
        {
            status = CELLULAR_UNKNOWN;
        }
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_ActivatePdn(CellularHandle, CellularSocketPdnContextId);
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_GetRssiByCSQ(CellularHandle,&rssi);
        Ql_CellularInfoPtr->Signal_4g = rssi;
    }

    if(status == CELLULAR_SUCCESS)
    {
        status = Cellular_GetPdnStatus(CellularHandle, &pdn_status, CellularSocketPdnContextId, &num_status);
        if((status == CELLULAR_SUCCESS) && (pdn_status.state == 1))
        {
            //if connection fails,it would retry every 15s,total retry 10 times
            if(QL_EC600U == Ql_CellularInfoPtr->ModelId)
            {
                //unit:s
                status = Cellular_QICFG_SetTcpRetranscfg( CellularHandle, 5,3); //5,30 //10,15
            }
            else if(QL_EG25 == Ql_CellularInfoPtr->ModelId)
            {
                //unit: 100ms
                status = Cellular_QICFG_SetTcpRetranscfg( CellularHandle, 5,50);
            }
            else
            {
                status = CELLULAR_UNKNOWN;
            }
        }
        else
        {
            QL_LOG_W("after GetPdnStatus: status[%d],state[%d]",status,pdn_status.state);
            status = CELLULAR_UNKNOWN;
        }
    }

    if(status != CELLULAR_SUCCESS)
    {
        ret = false;
    }

    return ret;
}
