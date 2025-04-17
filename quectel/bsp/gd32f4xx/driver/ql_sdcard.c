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
  Name: ql_sdcard.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_sdcard.h"
#include "gd32f4xx_sdio.h"
#include "gd32f4xx_dma.h"

#define LOG_TAG "sdcard"
#define LOG_LVL QL_LOG_WARN
#include "ql_log.h"

#define SDIO_STATIC_FLAGS               ((uint32_t)0x000005FF)
#define SDIO_CMD0TIMEOUT                ((uint32_t)0x00010000)

/** 
  * @brief  Mask for errors Card Status R1 (OCR Register)
  */
#define SD_OCR_ADDR_OUT_OF_RANGE        ((uint32_t)0x80000000)
#define SD_OCR_ADDR_MISALIGNED          ((uint32_t)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR            ((uint32_t)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR            ((uint32_t)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM          ((uint32_t)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION     ((uint32_t)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED       ((uint32_t)0x01000000)
#define SD_OCR_COM_CRC_FAILED           ((uint32_t)0x00800000)
#define SD_OCR_ILLEGAL_CMD              ((uint32_t)0x00400000)
#define SD_OCR_CARD_ECC_FAILED          ((uint32_t)0x00200000)
#define SD_OCR_CC_ERROR                 ((uint32_t)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR    ((uint32_t)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN     ((uint32_t)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN     ((uint32_t)0x00020000)
#define SD_OCR_CID_CSD_OVERWRIETE       ((uint32_t)0x00010000)
#define SD_OCR_WP_ERASE_SKIP            ((uint32_t)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED        ((uint32_t)0x00004000)
#define SD_OCR_ERASE_RESET              ((uint32_t)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR            ((uint32_t)0x00000008)
#define SD_OCR_ERRORBITS                ((uint32_t)0xFDFFE008)

/** 
  * @brief  Masks for R6 Response 
  */
#define SD_R6_GENERAL_UNKNOWN_ERROR     ((uint32_t)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((uint32_t)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((uint32_t)0x00008000)

#define SD_VOLTAGE_WINDOW_SD            ((uint32_t)0x80100000)
#define SD_HIGH_CAPACITY                ((uint32_t)0x40000000)
#define SD_STD_CAPACITY                 ((uint32_t)0x00000000)
#define SD_CHECK_PATTERN                ((uint32_t)0x000001AA)

#define SD_MAX_VOLT_TRIAL               ((uint32_t)0x0000FFFF)
#define SD_ALLZERO                      ((uint32_t)0x00000000)

#define SD_WIDE_BUS_SUPPORT             ((uint32_t)0x00040000)
#define SD_SINGLE_BUS_SUPPORT           ((uint32_t)0x00010000)
#define SD_CARD_LOCKED                  ((uint32_t)0x02000000)

#define SD_DATATIMEOUT                  ((uint32_t)0xFFFFFFFF)
#define SD_0TO7BITS                     ((uint32_t)0x000000FF)
#define SD_8TO15BITS                    ((uint32_t)0x0000FF00)
#define SD_16TO23BITS                   ((uint32_t)0x00FF0000)
#define SD_24TO31BITS                   ((uint32_t)0xFF000000)
#define SD_MAX_DATA_LENGTH              ((uint32_t)0x01FFFFFF)

/** 
  * @brief  Following commands are SD Card Specific commands.
  *         SDIO_APP_CMD should be sent before sending these commands. 
  */
#define SDIO_SEND_IF_COND               ((uint32_t)0x00000008)

static uint32_t CardType =  SDIO_STD_CAPACITY_SD_CARD_V1_1;
static uint32_t CSD_Tab[4], CID_Tab[4], RCA = 0;
__IO uint32_t StopCondition = 0;
__IO SD_Error TransferError = SD_OK;
__IO uint32_t TransferEnd = 0, DMAEndOfTransfer = 0;
SD_CardInfo SDCardInfo;

/** @defgroup STM324xG_EVAL_SDIO_SD_Private_Function_Prototypes
  * @{
  */
static SD_Error CmdError(void);
static SD_Error CmdResp1Error(uint8_t cmd);
static SD_Error CmdResp7Error(void);
static SD_Error CmdResp3Error(void);
static SD_Error CmdResp2Error(void);
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t *prca);
static SD_Error SDEnWideBus(ControlStatus NewState);
static SD_Error FindSCR(uint16_t rca, uint32_t *pscr);

/**
  * @}
  */ 
SD_Error SD_Detect;

/**
  * @brief  Initializes the SD Card and put it into StandBy State (Ready for data 
  *         transfer).
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_Init(void)
{
    __IO SD_Error errorstatus = SD_OK;
    
    /* SDIO Peripheral Low Level Init */
    SD_LowLevel_Init();
    
    sdio_deinit();
    
    errorstatus = SD_PowerON();
    
    if (errorstatus != SD_OK)
    {
        /*!< CMD Response TimeOut (wait for CMDSENT flag) */
        return(errorstatus);
    }
    
    errorstatus = SD_InitializeCards();
    
    if (errorstatus != SD_OK)
    {
        /*!< CMD Response TimeOut (wait for CMDSENT flag) */
        return(errorstatus);
    }
    
    /*!< Configure the SDIO peripheral */
    /*!< SDIO_CK = SDIOCLK / (SDIO_TRANSFER_CLK_DIV + 2) */
    /*!< on STM32F4xx devices, SDIOCLK is fixed to 48MHz */
    sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE, SDIO_CLOCKPWRSAVE_DISABLE, SDIO_TRANSFER_CLK_DIV);
    sdio_bus_mode_set(SDIO_BUSMODE_1BIT);
    sdio_hardware_clock_disable();
    
    /*----------------- Read CSD/CID MSD registers ------------------*/
    errorstatus = SD_GetCardInfo(&SDCardInfo);
    
    if (errorstatus == SD_OK)
    {
        /*----------------- Select Card --------------------------------*/
        errorstatus = SD_SelectDeselect((uint32_t) (SDCardInfo.RCA << 16));
    }
    
    if (errorstatus == SD_OK)
    {
        errorstatus = SD_EnableWideBusOperation(SDIO_BUSMODE_4BIT);
    }
    
    return(errorstatus);
}

/**
  * @brief  Gets the cuurent sd card data transfer status.
  * @param  None
  * @retval SDTransferState: Data Transfer state.
  *   This value can be: 
  *        - SD_TRANSFER_OK: No data transfer is acting
  *        - SD_TRANSFER_BUSY: Data transfer is acting
  */
SDTransferState SD_GetStatus(void)
{
    SDCardState cardstate =  SD_CARD_TRANSFER;

    cardstate = SD_GetState();

    if (cardstate == SD_CARD_TRANSFER)
    {
        return(SD_TRANSFER_OK);
    }
    else if(cardstate == SD_CARD_ERROR)
    {
        return (SD_TRANSFER_ERROR);
    }
    else
    {
        return(SD_TRANSFER_BUSY);
    }
}

/**
  * @brief  Returns the current card's state.
  * @param  None
  * @retval SDCardState: SD Card Error or SD Card Current State.
  */
SDCardState SD_GetState(void)
{
    uint32_t resp1 = 0;
  
    // if(SD_Detect()== SD_PRESENT)
    if(SD_Detect == SD_OK)
    {
        if (SD_SendStatus(&resp1) != SD_OK)
        {
            return SD_CARD_ERROR;
        }
        else
        {
            return (SDCardState)((resp1 >> 9) & 0x0F);
        }
    }
    else
    {
        return SD_CARD_ERROR;
    }
}

/**
  * @brief  Enquires cards about their operating voltage and configures 
  *   clock controls.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_PowerON(void)
{
    __IO SD_Error errorstatus = SD_OK;
    uint32_t response = 0, count = 0, validvoltage = 0;
    uint32_t SDType = SD_STD_CAPACITY;
    
    /*!< Power ON Sequence -----------------------------------------------------*/
    /*!< Configure the SDIO peripheral */
    /*!< SDIO_CK = SDIOCLK / (SDIO_INIT_CLK_DIV + 2) */
    /*!< on STM32F4xx devices, SDIOCLK is fixed to 48MHz */
    /*!< SDIO_CK for initialization should not exceed 400 KHz */
    sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE, SDIO_CLOCKPWRSAVE_DISABLE, SDIO_INIT_CLK_DIV);
    sdio_bus_mode_set(SDIO_BUSMODE_1BIT);
    sdio_hardware_clock_disable();
    
    /*!< Set Power State to ON */
    sdio_power_state_set(SDIO_POWER_ON);
    
    /*!< Enable SDIO Clock */
    sdio_clock_enable();
    
    /*!< CMD0: GO_IDLE_STATE ---------------------------------------------------*/
    /*!< No CMD response required */
    sdio_command_response_config(SD_CMD_GO_IDLE_STATE, (uint32_t)0x0, SDIO_RESPONSETYPE_NO);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    
    errorstatus = CmdError();
    
    if (errorstatus != SD_OK)
    {
        /*!< CMD Response TimeOut (wait for CMDSENT flag) */
        return(errorstatus);
    }
    
    /*!< CMD8: SEND_IF_COND ----------------------------------------------------*/
    /*!< Send CMD8 to verify SD card interface operating condition */
    /*!< Argument: - [31:12]: Reserved (shall be set to '0')
                   - [11:8]: Supply Voltage (VHS) 0x1 (Range: 2.7-3.6 V)
                   - [7:0]: Check Pattern (recommended 0xAA) */
    /*!< CMD Response: R7 */
    sdio_command_response_config(SDIO_SEND_IF_COND, SD_CHECK_PATTERN, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    
    errorstatus = CmdResp7Error();
    
    if (errorstatus == SD_OK)
    {
        CardType = SDIO_STD_CAPACITY_SD_CARD_V2_0; /*!< SD Card 2.0 */
        SDType = SD_HIGH_CAPACITY;
    }
    else
    {
        /*!< CMD55 */
        sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
    }
    /*!< CMD55 */
    sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
    
    /*!< If errorstatus is Command TimeOut, it is a MMC card */
    /*!< If errorstatus is SD_OK it is a SD card: SD card 2.0 (voltage range mismatch)
         or SD card 1.x */
    if (errorstatus == SD_OK)
    {
        /*!< SD CARD */
        /*!< Send ACMD41 SD_APP_OP_COND with Argument 0x80100000 */
        while ((!validvoltage) && (count < SD_MAX_VOLT_TRIAL))
        {
            
            /*!< SEND CMD55 APP_CMD with RCA as 0 */
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            
            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }
            sdio_command_response_config(SD_CMD_SD_APP_OP_COND, (SD_VOLTAGE_WINDOW_SD | SDType), SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            
            errorstatus = CmdResp3Error();
            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }
            
            response = sdio_response_get(SDIO_RESPONSE0);
            validvoltage = (((response >> 31) == 1) ? 1 : 0);
            count++;
        }
        if (count >= SD_MAX_VOLT_TRIAL)
        {
            errorstatus = SD_INVALID_VOLTRANGE;
            return(errorstatus);
        }
        
        if (response &= SD_HIGH_CAPACITY)
        {
            CardType = SDIO_HIGH_CAPACITY_SD_CARD;
        }
        
    }/*!< else MMC Card */
    
    return(errorstatus);
}

/**
  * @brief  Turns the SDIO output signals off.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_PowerOFF(void)
{
    SD_Error errorstatus = SD_OK;

    /*!< Set Power State to OFF */
    sdio_power_state_set(SDIO_POWER_OFF);

    return(errorstatus);
}

/**
  * @brief  Intialises all cards or single card as the case may be Card(s) come 
  *         into standby state.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_InitializeCards(void)
{
    SD_Error errorstatus = SD_OK;
    uint16_t rca = 0x01;
    
    if (sdio_power_state_get() == SDIO_POWER_OFF)
    {
        errorstatus = SD_REQUEST_NOT_APPLICABLE;
        return(errorstatus);
    }
    
    if (SDIO_SECURE_DIGITAL_IO_CARD != CardType)
    {
        /*!< Send CMD2 ALL_SEND_CID */
        sdio_command_response_config(SD_CMD_ALL_SEND_CID, (uint32_t)0x0, SDIO_RESPONSETYPE_LONG);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        
        errorstatus = CmdResp2Error();

        if (SD_OK != errorstatus)
        {
            return(errorstatus);
        }
        
        CID_Tab[0] = sdio_response_get(SDIO_RESPONSE0);
        CID_Tab[1] = sdio_response_get(SDIO_RESPONSE1);
        CID_Tab[2] = sdio_response_get(SDIO_RESPONSE2);
        CID_Tab[3] = sdio_response_get(SDIO_RESPONSE3);
    }
    if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) ||  (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) ||  (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == CardType)
      ||  (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
    {
        /*!< Send CMD3 SET_REL_ADDR with argument 0 */
        /*!< SD Card publishes its RCA. */
        sdio_command_response_config(SD_CMD_SET_REL_ADDR, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        errorstatus = CmdResp6Error(SD_CMD_SET_REL_ADDR, &rca);

        if (SD_OK != errorstatus)
        {
            return(errorstatus);
        }
    }
    
    if (SDIO_SECURE_DIGITAL_IO_CARD != CardType)
    {
        RCA = rca;

        /*!< Send CMD9 SEND_CSD with argument as card's RCA */
        sdio_command_response_config(SD_CMD_SEND_CSD, (uint32_t)(rca << 16), SDIO_RESPONSETYPE_LONG);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        errorstatus = CmdResp2Error();

        if (SD_OK != errorstatus)
        {
            return(errorstatus);
        }

        CSD_Tab[0] = sdio_response_get(SDIO_RESPONSE0);
        CSD_Tab[1] = sdio_response_get(SDIO_RESPONSE1);
        CSD_Tab[2] = sdio_response_get(SDIO_RESPONSE2);
        CSD_Tab[3] = sdio_response_get(SDIO_RESPONSE3);
    }
    
    errorstatus = SD_OK; /*!< All cards get intialized */

    return(errorstatus);
}

/**
  * @brief  Returns information about specific card.
  * @param  cardinfo: pointer to a SD_CardInfo structure that contains all SD card 
  *         information.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
    SD_Error errorstatus = SD_OK;
    uint8_t tmp = 0;

    cardinfo->CardType = (uint8_t)CardType;
    cardinfo->RCA = (uint16_t)RCA;

    /*!< Byte 0 */
    tmp = (uint8_t)((CSD_Tab[0] & 0xFF000000) >> 24);
    cardinfo->SD_csd.CSDStruct = (tmp & 0xC0) >> 6;
    cardinfo->SD_csd.SysSpecVersion = (tmp & 0x3C) >> 2;
    cardinfo->SD_csd.Reserved1 = tmp & 0x03;

    /*!< Byte 1 */
    tmp = (uint8_t)((CSD_Tab[0] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.TAAC = tmp;

    /*!< Byte 2 */
    tmp = (uint8_t)((CSD_Tab[0] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.NSAC = tmp;

    /*!< Byte 3 */
    tmp = (uint8_t)(CSD_Tab[0] & 0x000000FF);
    cardinfo->SD_csd.MaxBusClkFrec = tmp;

    /*!< Byte 4 */
    tmp = (uint8_t)((CSD_Tab[1] & 0xFF000000) >> 24);
    cardinfo->SD_csd.CardComdClasses = tmp << 4;

    /*!< Byte 5 */
    tmp = (uint8_t)((CSD_Tab[1] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.CardComdClasses |= (tmp & 0xF0) >> 4;
    cardinfo->SD_csd.RdBlockLen = tmp & 0x0F;

    /*!< Byte 6 */
    tmp = (uint8_t)((CSD_Tab[1] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.PartBlockRead = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.WrBlockMisalign = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.RdBlockMisalign = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.DSRImpl = (tmp & 0x10) >> 4;
    cardinfo->SD_csd.Reserved2 = 0; /*!< Reserved */

    if ((CardType == SDIO_STD_CAPACITY_SD_CARD_V1_1) || (CardType == SDIO_STD_CAPACITY_SD_CARD_V2_0))
    {
        cardinfo->SD_csd.DeviceSize = (tmp & 0x03) << 10;

        /*!< Byte 7 */
        tmp = (uint8_t)(CSD_Tab[1] & 0x000000FF);
        cardinfo->SD_csd.DeviceSize |= (tmp) << 2;

        /*!< Byte 8 */
        tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);
        cardinfo->SD_csd.DeviceSize |= (tmp & 0xC0) >> 6;

        cardinfo->SD_csd.MaxRdCurrentVDDMin = (tmp & 0x38) >> 3;
        cardinfo->SD_csd.MaxRdCurrentVDDMax = (tmp & 0x07);

        /*!< Byte 9 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);
        cardinfo->SD_csd.MaxWrCurrentVDDMin = (tmp & 0xE0) >> 5;
        cardinfo->SD_csd.MaxWrCurrentVDDMax = (tmp & 0x1C) >> 2;
        cardinfo->SD_csd.DeviceSizeMul = (tmp & 0x03) << 1;
        /*!< Byte 10 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);
        cardinfo->SD_csd.DeviceSizeMul |= (tmp & 0x80) >> 7;

        cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) ;
        cardinfo->CardCapacity *= (1 << (cardinfo->SD_csd.DeviceSizeMul + 2));
        cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
        cardinfo->CardCapacity *= cardinfo->CardBlockSize;
    }
    else if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        /*!< Byte 7 */
        tmp = (uint8_t)(CSD_Tab[1] & 0x000000FF);
        cardinfo->SD_csd.DeviceSize = (tmp & 0x3F) << 16;

        /*!< Byte 8 */
        tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);

        cardinfo->SD_csd.DeviceSize |= (tmp << 8);

        /*!< Byte 9 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);

        cardinfo->SD_csd.DeviceSize |= (tmp);

        /*!< Byte 10 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);

        cardinfo->CardCapacity = ((uint64_t)cardinfo->SD_csd.DeviceSize + 1) * 512 * 1024;
        cardinfo->CardBlockSize = 512;
    }


    cardinfo->SD_csd.EraseGrSize = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.EraseGrMul = (tmp & 0x3F) << 1;

    /*!< Byte 11 */
    tmp = (uint8_t)(CSD_Tab[2] & 0x000000FF);
    cardinfo->SD_csd.EraseGrMul |= (tmp & 0x80) >> 7;
    cardinfo->SD_csd.WrProtectGrSize = (tmp & 0x7F);

    /*!< Byte 12 */
    tmp = (uint8_t)((CSD_Tab[3] & 0xFF000000) >> 24);
    cardinfo->SD_csd.WrProtectGrEnable = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.ManDeflECC = (tmp & 0x60) >> 5;
    cardinfo->SD_csd.WrSpeedFact = (tmp & 0x1C) >> 2;
    cardinfo->SD_csd.MaxWrBlockLen = (tmp & 0x03) << 2;

    /*!< Byte 13 */
    tmp = (uint8_t)((CSD_Tab[3] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.MaxWrBlockLen |= (tmp & 0xC0) >> 6;
    cardinfo->SD_csd.WriteBlockPaPartial = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.Reserved3 = 0;
    cardinfo->SD_csd.ContentProtectAppli = (tmp & 0x01);

    /*!< Byte 14 */
    tmp = (uint8_t)((CSD_Tab[3] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.FileFormatGrouop = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.CopyFlag = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.PermWrProtect = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.TempWrProtect = (tmp & 0x10) >> 4;
    cardinfo->SD_csd.FileFormat = (tmp & 0x0C) >> 2;
    cardinfo->SD_csd.ECC = (tmp & 0x03);

    /*!< Byte 15 */
    tmp = (uint8_t)(CSD_Tab[3] & 0x000000FF);
    cardinfo->SD_csd.CSD_CRC = (tmp & 0xFE) >> 1;
    cardinfo->SD_csd.Reserved4 = 1;


    /*!< Byte 0 */
    tmp = (uint8_t)((CID_Tab[0] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ManufacturerID = tmp;

    /*!< Byte 1 */
    tmp = (uint8_t)((CID_Tab[0] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.OEM_AppliID = tmp << 8;

    /*!< Byte 2 */
    tmp = (uint8_t)((CID_Tab[0] & 0x000000FF00) >> 8);
    cardinfo->SD_cid.OEM_AppliID |= tmp;

    /*!< Byte 3 */
    tmp = (uint8_t)(CID_Tab[0] & 0x000000FF);
    cardinfo->SD_cid.ProdName1 = tmp << 24;

    /*!< Byte 4 */
    tmp = (uint8_t)((CID_Tab[1] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdName1 |= tmp << 16;

    /*!< Byte 5 */
    tmp = (uint8_t)((CID_Tab[1] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.ProdName1 |= tmp << 8;

    /*!< Byte 6 */
    tmp = (uint8_t)((CID_Tab[1] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ProdName1 |= tmp;

    /*!< Byte 7 */
    tmp = (uint8_t)(CID_Tab[1] & 0x000000FF);
    cardinfo->SD_cid.ProdName2 = tmp;

    /*!< Byte 8 */
    tmp = (uint8_t)((CID_Tab[2] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdRev = tmp;

    /*!< Byte 9 */
    tmp = (uint8_t)((CID_Tab[2] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.ProdSN = tmp << 24;

    /*!< Byte 10 */
    tmp = (uint8_t)((CID_Tab[2] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ProdSN |= tmp << 16;

    /*!< Byte 11 */
    tmp = (uint8_t)(CID_Tab[2] & 0x000000FF);
    cardinfo->SD_cid.ProdSN |= tmp << 8;

    /*!< Byte 12 */
    tmp = (uint8_t)((CID_Tab[3] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdSN |= tmp;

    /*!< Byte 13 */
    tmp = (uint8_t)((CID_Tab[3] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.Reserved1 |= (tmp & 0xF0) >> 4;
    cardinfo->SD_cid.ManufactDate = (tmp & 0x0F) << 8;

    /*!< Byte 14 */
    tmp = (uint8_t)((CID_Tab[3] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ManufactDate |= tmp;

    /*!< Byte 15 */
    tmp = (uint8_t)(CID_Tab[3] & 0x000000FF);
    cardinfo->SD_cid.CID_CRC = (tmp & 0xFE) >> 1;
    cardinfo->SD_cid.Reserved2 = 1;

    return(errorstatus);
}

/**
  * @brief  Enables wide bus opeartion for the requeseted card if supported by 
  *         card.
  * @param  WideMode: Specifies the SD card wide bus mode. 
  *   This parameter can be one of the following values:
  *     @arg SDIO_BusWide_8b: 8-bit data transfer (Only for MMC)
  *     @arg SDIO_BusWide_4b: 4-bit data transfer
  *     @arg SDIO_BusWide_1b: 1-bit data transfer
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_EnableWideBusOperation(uint32_t WideMode)
{
    SD_Error errorstatus = SD_OK;

    /*!< MMC Card doesn't support this feature */
    if (SDIO_MULTIMEDIA_CARD == CardType)
    {
        errorstatus = SD_UNSUPPORTED_FEATURE;
        return(errorstatus);
    }
    else if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
    {
        if (SDIO_BUSMODE_8BIT == WideMode)
        {
            errorstatus = SD_UNSUPPORTED_FEATURE;
            return(errorstatus);
        }
        else if (SDIO_BUSMODE_4BIT == WideMode)
        {
            errorstatus = SDEnWideBus(ENABLE);

            if (SD_OK == errorstatus)
            {
                /*!< Configure the SDIO peripheral */
                sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE,
                                  SDIO_CLOCKPWRSAVE_DISABLE, SDIO_TRANSFER_CLK_DIV);
                sdio_bus_mode_set(WideMode);
                sdio_hardware_clock_disable();
            }
        }
        else
        {
            errorstatus = SDEnWideBus(DISABLE);

            if (SD_OK == errorstatus)
            {
                /*!< Configure the SDIO peripheral */
                sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE,
                                  SDIO_CLOCKPWRSAVE_DISABLE, SDIO_TRANSFER_CLK_DIV);
                sdio_bus_mode_set(WideMode);
                sdio_hardware_clock_disable();
            }
        }
    }

    return(errorstatus);
}

/**
  * @brief  Selects od Deselects the corresponding card.
  * @param  addr: Address of the Card to be selected.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_SelectDeselect(uint64_t addr)
{
    SD_Error errorstatus = SD_OK;

    /*!< Send CMD7 SDIO_SEL_DESEL_CARD */
    sdio_command_response_config(SD_CMD_SEL_DESEL_CARD, (uint32_t)addr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SEL_DESEL_CARD);

    return(errorstatus);
}

/**
  * @brief  Allows to read one block from a specified address in a card. The Data
  *         transfer can be managed by DMA mode or Polling mode. 
  * @note   This operation should be followed by two functions to check if the 
  *         DMA Controller and SD Card status.
  *          - SD_ReadWaitOperation(): this function insure that the DMA
  *            controller has finished all data transfer.
  *          - SD_GetStatus(): to check that the SD Card has finished the 
  *            data transfer and it is ready for data.            
  * @param  readbuff: pointer to the buffer that will contain the received data
  * @param  ReadAddr: Address from where data are to be read.  
  * @param  BlockSize: the SD card Data block size. The Block size should be 512.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_ReadBlock(uint8_t *readbuff, uint64_t ReadAddr, uint16_t BlockSize)
{
    SD_Error errorstatus = SD_OK;
#if defined (SD_POLLING_MODE) 
  uint32_t count = 0, *tempbuff = (uint32_t *)readbuff;
#endif
    
    TransferError = SD_OK;
    TransferEnd = 0;
    StopCondition = 0;
    
    SDIO_DATACTL = 0x0;
#if defined (SD_DMA_MODE)
    sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_RXORE | SDIO_INT_STBITE);
    sdio_dma_enable();
    SD_LowLevel_DMA_RxConfig((uint32_t *)readbuff, BlockSize);
#endif
    
    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        BlockSize = 512;
        ReadAddr /= 512;
    }
    
    /* Set Block Size for Card */
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)BlockSize, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    
    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);
    
    if (SD_OK != errorstatus)
    {
        return(errorstatus);
    }
    
    sdio_data_config(SD_DATATIMEOUT, BlockSize, (uint32_t) 9 << 4);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();
    
    /*!< Send CMD17 READ_SINGLE_BLOCK */
    sdio_command_response_config(SD_CMD_READ_SINGLE_BLOCK, (uint32_t)ReadAddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    
    errorstatus = CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);
    
    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

#if defined (SD_POLLING_MODE)
    
#endif

    return(errorstatus);
}

/**
  * @brief  Allows to read blocks from a specified address  in a card.  The Data
  *         transfer can be managed by DMA mode or Polling mode. 
  * @note   This operation should be followed by two functions to check if the 
  *         DMA Controller and SD Card status.
  *          - SD_ReadWaitOperation(): this function insure that the DMA
  *            controller has finished all data transfer.
  *          - SD_GetStatus(): to check that the SD Card has finished the 
  *            data transfer and it is ready for data.   
  * @param  readbuff: pointer to the buffer that will contain the received data.
  * @param  ReadAddr: Address from where data are to be read.
  * @param  BlockSize: the SD card Data block size. The Block size should be 512.
  * @param  NumberOfBlocks: number of blocks to be read.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_ReadMultiBlocks(uint8_t *readbuff, uint64_t ReadAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
    SD_Error errorstatus = SD_OK;
    TransferError = SD_OK;
    TransferEnd = 0;
    StopCondition = 1;
    
    SDIO_DATACTL = 0x0;
    sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_RXORE | SDIO_INT_STBITE);
    SD_LowLevel_DMA_RxConfig((uint32_t *)readbuff, (NumberOfBlocks * BlockSize));
    sdio_dma_enable();
    
    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        BlockSize = 512;
        ReadAddr /= 512;
    }
    
    /*!< Set Block Size for Card */
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)BlockSize, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (SD_OK != errorstatus)
    {
        return(errorstatus);
    }

    sdio_data_config(SD_DATATIMEOUT, NumberOfBlocks * BlockSize, (uint32_t) 9 << 4);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();

    /*!< Send CMD18 READ_MULT_BLOCK with argument data address */
    sdio_command_response_config(SD_CMD_READ_MULT_BLOCK, (uint32_t)ReadAddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_READ_MULT_BLOCK);
    
    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    return(errorstatus);
}

/**
  * @brief  This function waits until the SDIO DMA data transfer is finished. 
  *         This function should be called after SDIO_ReadMultiBlocks() function
  *         to insure that all data sent by the card are already transferred by 
  *         the DMA controller.
  * @param  None.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WaitReadOperation(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t timeout;

    timeout = SD_DATATIMEOUT;
    
    while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0))
    {
        timeout--;
    }
    
    DMAEndOfTransfer = 0x00;
    
    timeout = SD_DATATIMEOUT;
    
    while(((SDIO_STAT & SDIO_FLAG_RXRUN)) && (timeout > 0))
    {
        timeout--;
    }
    
    if (StopCondition == 1)
    {
        errorstatus = SD_StopTransfer();
        StopCondition = 0;
    }
    
    if ((timeout == 0) && (errorstatus == SD_OK))
    {
        errorstatus = SD_DATA_TIMEOUT;
    }

    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);

    if (TransferError != SD_OK)
    {
        return(TransferError);
    }
    else
    {
        return(errorstatus);
    }
}

/**
  * @brief  Allows to write one block starting from a specified address in a card.
  *         The Data transfer can be managed by DMA mode or Polling mode.
  * @note   This operation should be followed by two functions to check if the 
  *         DMA Controller and SD Card status.
  *          - SD_ReadWaitOperation(): this function insure that the DMA
  *            controller has finished all data transfer.
  *          - SD_GetStatus(): to check that the SD Card has finished the 
  *            data transfer and it is ready for data.      
  * @param  writebuff: pointer to the buffer that contain the data to be transferred.
  * @param  WriteAddr: Address from where data are to be read.   
  * @param  BlockSize: the SD card Data block size. The Block size should be 512.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WriteBlock(uint8_t *writebuff, uint64_t WriteAddr, uint16_t BlockSize)
{
    SD_Error errorstatus = SD_OK;
    
#if defined (SD_POLLING_MODE)
  uint32_t bytestransferred = 0, count = 0, restwords = 0;
  uint32_t *tempbuff = (uint32_t *)writebuff;
#endif
    
    TransferError = SD_OK;
    TransferEnd = 0;
    StopCondition = 0;
    
    SDIO_DATACTL = 0x0;
    
#if defined (SD_DMA_MODE)
    sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_RXORE | SDIO_INT_STBITE);
    SD_LowLevel_DMA_TxConfig((uint32_t *)writebuff, BlockSize);
    sdio_dma_enable();
#endif
    
    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        BlockSize = 512;
        WriteAddr /= 512;
    }
    
    /* Set Block Size for Card */ 
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)BlockSize, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (SD_OK != errorstatus)
    {
        return(errorstatus);
    }

    /*!< Send CMD24 WRITE_SINGLE_BLOCK */
    sdio_command_response_config(SD_CMD_WRITE_SINGLE_BLOCK, (uint32_t)WriteAddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    sdio_data_config(SD_DATATIMEOUT, BlockSize, (uint32_t) 9 << 4);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_enable();
    
    /*!< In case of single data block transfer no need of stop command at all */
#if defined (SD_POLLING_MODE)

#endif

    return(errorstatus);
}

/**
  * @brief  Allows to write blocks starting from a specified address in a card.
  *         The Data transfer can be managed by DMA mode only. 
  * @note   This operation should be followed by two functions to check if the 
  *         DMA Controller and SD Card status.
  *          - SD_ReadWaitOperation(): this function insure that the DMA
  *            controller has finished all data transfer.
  *          - SD_GetStatus(): to check that the SD Card has finished the 
  *            data transfer and it is ready for data.     
  * @param  WriteAddr: Address from where data are to be read.
  * @param  writebuff: pointer to the buffer that contain the data to be transferred.
  * @param  BlockSize: the SD card Data block size. The Block size should be 512.
  * @param  NumberOfBlocks: number of blocks to be written.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WriteMultiBlocks(uint8_t *writebuff, uint64_t WriteAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
    SD_Error errorstatus = SD_OK;

    TransferError = SD_OK;
    TransferEnd = 0;
    StopCondition = 1;
    SDIO_DATACTL = 0x0;
    
    sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_RXORE | SDIO_INT_STBITE);
    SD_LowLevel_DMA_TxConfig((uint32_t *)writebuff, (NumberOfBlocks * BlockSize));
    sdio_dma_enable();
    
    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        BlockSize = 512;
        WriteAddr /= 512;
    }
    
    /* Set Block Size for Card */ 
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)BlockSize, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (SD_OK != errorstatus)
    {
        return(errorstatus);
    }

    /*!< To improve performance */
    sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t) (RCA << 16), SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }
    /*!< To improve performance */
    sdio_command_response_config(SD_CMD_SET_BLOCK_COUNT, (uint32_t)NumberOfBlocks, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }


    /*!< Send CMD25 WRITE_MULT_BLOCK with argument data address */
    sdio_command_response_config(SD_CMD_WRITE_MULT_BLOCK, (uint32_t)WriteAddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);

    if (SD_OK != errorstatus)
    {
        return(errorstatus);
    }

    sdio_data_config(SD_DATATIMEOUT, NumberOfBlocks * BlockSize, (uint32_t) 9 << 4);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_enable();

    return(errorstatus);
}

/**
  * @brief  This function waits until the SDIO DMA data transfer is finished. 
  *         This function should be called after SDIO_WriteBlock() and
  *         SDIO_WriteMultiBlocks() function to insure that all data sent by the 
  *         card are already transferred by the DMA controller.        
  * @param  None.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WaitWriteOperation(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t timeout;

    timeout = SD_DATATIMEOUT;

    while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0))
    {
        timeout--;
    }

    DMAEndOfTransfer = 0x00;

    timeout = SD_DATATIMEOUT;

    while(((SDIO_STAT & SDIO_FLAG_TXRUN)) && (timeout > 0))
    {
        timeout--;
    }

    if (StopCondition == 1)
    {
    errorstatus = SD_StopTransfer();
    StopCondition = 0;
    }

    if ((timeout == 0) && (errorstatus == SD_OK))
    {
        errorstatus = SD_DATA_TIMEOUT;
    }

    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);

    if (TransferError != SD_OK)
    {
        return(TransferError);
    }
    else
    {
        return(errorstatus);
    }
}

/**
  * @brief  Aborts an ongoing data transfer.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_StopTransfer(void)
{
    SD_Error errorstatus = SD_OK;

    /*!< Send CMD12 STOP_TRANSMISSION  */
    sdio_command_response_config(SD_CMD_STOP_TRANSMISSION, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

    return(errorstatus);
}

/**
  * @brief  Returns the current card's status.
  * @param  pcardstatus: pointer to the buffer that will contain the SD card 
  *         status (Card Status register).
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_SendStatus(uint32_t *pcardstatus)
{
    SD_Error errorstatus = SD_OK;

    if (pcardstatus == NULL)
    {
        errorstatus = SD_INVALID_PARAMETER;
        return(errorstatus);
    }
    
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t) RCA << 16, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SEND_STATUS);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    *pcardstatus = sdio_response_get(SDIO_RESPONSE0);

    return(errorstatus);
}

/**
  * @brief  Allows to process all the interrupts that are high.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_ProcessIRQSrc(void)
{
    if (sdio_interrupt_flag_get(SDIO_INT_FLAG_DTEND) != RESET)
    {
        TransferError = SD_OK;
        sdio_interrupt_flag_clear(SDIO_INT_DTEND);
        TransferEnd = 1;
    }  
    else if (sdio_interrupt_flag_get(SDIO_INT_FLAG_DTCRCERR) != RESET)
    {
        sdio_interrupt_flag_clear(SDIO_INT_DTCRCERR);
        TransferError = SD_DATA_CRC_FAIL;
    }
    else if (sdio_interrupt_flag_get(SDIO_INT_FLAG_DTTMOUT) != RESET)
    {
        sdio_interrupt_flag_clear(SDIO_INT_DTTMOUT);
        TransferError = SD_DATA_TIMEOUT;
    }
    else if (sdio_interrupt_flag_get(SDIO_INT_FLAG_RXORE) != RESET)
    {
        sdio_interrupt_flag_clear(SDIO_INT_RXORE);
        TransferError = SD_RX_OVERRUN;
    }
    else if (sdio_interrupt_flag_get(SDIO_INT_FLAG_TXURE) != RESET)
    {
        sdio_interrupt_flag_clear(SDIO_INT_TXURE);
        TransferError = SD_TX_UNDERRUN;
    }
    else if (sdio_interrupt_flag_get(SDIO_INT_FLAG_STBITE) != RESET)
    {
        sdio_interrupt_flag_clear(SDIO_INT_STBITE);
        TransferError = SD_START_BIT_ERR;
    }

    sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND |
                           SDIO_INT_TFH      | SDIO_INT_RFH     | SDIO_INT_TXURE |
                           SDIO_INT_RXORE    | SDIO_INT_STBITE);
    
    return(TransferError);
}

/**
  * @brief  This function waits until the SDIO DMA data transfer is finished. 
  * @param  None.
  * @retval None.
  */
void SD_ProcessDMAIRQ(void)
{
    if(DMA_INTF0(DMA1) & DMA_FLAG_ADD(DMA_FLAG_FTF, DMA_CH3))
    {
        DMAEndOfTransfer = 0x01;
        dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FEE);
        dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    }
}

/**
  * @brief  Checks for error conditions for CMD0.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdError(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t timeout;
    
    timeout = SDIO_CMD0TIMEOUT; /*!< 10000 */
    
    while ((timeout > 0) && (sdio_flag_get(SDIO_FLAG_CMDSEND) == RESET))
    {
        timeout--;
    }
    
    if (timeout == 0)
    {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        return(errorstatus);
    }
    
    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);
    
    return(errorstatus);
}

/**
  * @brief  Checks for error conditions for R7 response.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdResp7Error(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t timeout = SDIO_CMD0TIMEOUT;
    
    status = SDIO_STAT;
    
    while (!(status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDRECV | SDIO_FLAG_CMDTMOUT)) && (timeout > 0))
    {
        timeout--;
        status = SDIO_STAT;
    }
    
    if ((timeout == 0) || (status & SDIO_FLAG_CMDTMOUT))
    {
        /*!< Card is not V2.0 complient or card does not support the set voltage range */
        errorstatus = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return(errorstatus);
    }
    
    if (status & SDIO_FLAG_CMDRECV)
    {
        /*!< Card is SD V2.0 compliant */
        errorstatus = SD_OK;
        sdio_flag_clear(SDIO_FLAG_CMDRECV);
        return(errorstatus);
    }
    return(errorstatus);
}

/**
  * @brief  Checks for error conditions for R1 response.
  * @param  cmd: The sent command index.
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdResp1Error(uint8_t cmd)
{
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t response_r1;
    
    status = SDIO_STAT;
    
    while (!(status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDRECV | SDIO_FLAG_CMDTMOUT)))
    {
        status = SDIO_STAT;
    }
    
    if (status & SDIO_FLAG_CMDTMOUT)
    {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return(errorstatus);
    }
    else if (status & SDIO_FLAG_CCRCERR)
    {
        errorstatus = SD_CMD_CRC_FAIL;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return(errorstatus);
    }
    
    /*!< Check response received is of desired command */
    if (sdio_command_index_get() != cmd)
    {
        errorstatus = SD_ILLEGAL_CMD;
        return(errorstatus);
    }
    
    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);
    
    /*!< We have received response, retrieve it for analysis  */
    response_r1 = sdio_response_get(SDIO_RESPONSE0);
    
    if ((response_r1 & SD_OCR_ERRORBITS) == SD_ALLZERO)
    {
        return(errorstatus);
    }
    
    if (response_r1 & SD_OCR_ADDR_OUT_OF_RANGE)
    {
        return(SD_ADDR_OUT_OF_RANGE);
    }

    if (response_r1 & SD_OCR_ADDR_MISALIGNED)
    {
        return(SD_ADDR_MISALIGNED);
    }

    if (response_r1 & SD_OCR_BLOCK_LEN_ERR)
    {
        return(SD_BLOCK_LEN_ERR);
    }

    if (response_r1 & SD_OCR_ERASE_SEQ_ERR)
    {
        return(SD_ERASE_SEQ_ERR);
    }

    if (response_r1 & SD_OCR_BAD_ERASE_PARAM)
    {
        return(SD_BAD_ERASE_PARAM);
    }

    if (response_r1 & SD_OCR_WRITE_PROT_VIOLATION)
    {
        return(SD_WRITE_PROT_VIOLATION);
    }

    if (response_r1 & SD_OCR_LOCK_UNLOCK_FAILED)
    {
        return(SD_LOCK_UNLOCK_FAILED);
    }

    if (response_r1 & SD_OCR_COM_CRC_FAILED)
    {
        return(SD_COM_CRC_FAILED);
    }

    if (response_r1 & SD_OCR_ILLEGAL_CMD)
    {
        return(SD_ILLEGAL_CMD);
    }

    if (response_r1 & SD_OCR_CARD_ECC_FAILED)
    {
        return(SD_CARD_ECC_FAILED);
    }

    if (response_r1 & SD_OCR_CC_ERROR)
    {
        return(SD_CC_ERROR);
    }

    if (response_r1 & SD_OCR_GENERAL_UNKNOWN_ERROR)
    {
        return(SD_GENERAL_UNKNOWN_ERROR);
    }

    if (response_r1 & SD_OCR_STREAM_READ_UNDERRUN)
    {
        return(SD_STREAM_READ_UNDERRUN);
    }

    if (response_r1 & SD_OCR_STREAM_WRITE_OVERRUN)
    {
        return(SD_STREAM_WRITE_OVERRUN);
    }

    if (response_r1 & SD_OCR_CID_CSD_OVERWRIETE)
    {
        return(SD_CID_CSD_OVERWRITE);
    }

    if (response_r1 & SD_OCR_WP_ERASE_SKIP)
    {
        return(SD_WP_ERASE_SKIP);
    }

    if (response_r1 & SD_OCR_CARD_ECC_DISABLED)
    {
        return(SD_CARD_ECC_DISABLED);
    }

    if (response_r1 & SD_OCR_ERASE_RESET)
    {
        return(SD_ERASE_RESET);
    }

    if (response_r1 & SD_OCR_AKE_SEQ_ERROR)
    {
        return(SD_AKE_SEQ_ERROR);
    }
    return(errorstatus);
}

/**
  * @brief  Checks for error conditions for R3 (OCR) response.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdResp3Error(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    
    status = SDIO_STAT;
    
    while (!(status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDRECV | SDIO_FLAG_CMDTMOUT)))
    {
        status = SDIO_STAT;
    }
    
    if (status & SDIO_FLAG_CMDTMOUT)
    {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return(errorstatus);
    }
    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);
    return(errorstatus);
}

/**
  * @brief  Checks for error conditions for R2 (CID or CSD) response.
  * @param  None
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdResp2Error(void)
{
    SD_Error errorstatus = SD_OK;
    uint32_t status;

    status = SDIO_STAT;

    while (!(status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)))
    {
        status = SDIO_STAT;
    }

    if (status & SDIO_FLAG_CMDTMOUT)
    {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return(errorstatus);
    }
    else if (status & SDIO_FLAG_CCRCERR)
    {
        errorstatus = SD_CMD_CRC_FAIL;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return(errorstatus);
    }

    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);

    return(errorstatus);
}

/**
  * @brief  Checks for error conditions for R6 (RCA) response.
  * @param  cmd: The sent command index.
  * @param  prca: pointer to the variable that will contain the SD card relative 
  *         address RCA. 
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t *prca)
{
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t response_r1;

    status = SDIO_STAT;

    while (!(status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)))
    {
        status = SDIO_STAT;
    }

    if (status & SDIO_FLAG_CMDTMOUT)
    {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return(errorstatus);
    }
    else if (status & SDIO_FLAG_CCRCERR)
    {
        errorstatus = SD_CMD_CRC_FAIL;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return(errorstatus);
    }

    /*!< Check response received is of desired command */
    if (sdio_command_index_get() != cmd)
    {
        errorstatus = SD_ILLEGAL_CMD;
        return(errorstatus);
    }

    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);

    /*!< We have received response, retrieve it.  */
    response_r1 = sdio_response_get(SDIO_RESPONSE0);

    if (SD_ALLZERO == (response_r1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)))
    {
        *prca = (uint16_t) (response_r1 >> 16);
        return(errorstatus);
    }

    if (response_r1 & SD_R6_GENERAL_UNKNOWN_ERROR)
    {
        return(SD_GENERAL_UNKNOWN_ERROR);
    }

    if (response_r1 & SD_R6_ILLEGAL_CMD)
    {
        return(SD_ILLEGAL_CMD);
    }

    if (response_r1 & SD_R6_COM_CRC_FAILED)
    {
        return(SD_COM_CRC_FAILED);
    }

    return(errorstatus);
}

/**
  * @brief  Enables or disables the SDIO wide bus mode.
  * @param  NewState: new state of the SDIO wide bus mode.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error SDEnWideBus(ControlStatus NewState)
{
    SD_Error errorstatus = SD_OK;

    uint32_t scr[2] = {0, 0};

    if (sdio_response_get(SDIO_RESPONSE0) & SD_CARD_LOCKED)
    {
        errorstatus = SD_LOCK_UNLOCK_FAILED;
        return(errorstatus);
    }

    /*!< Get SCR Register */
    errorstatus = FindSCR(RCA, scr);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    /*!< If wide bus operation to be enabled */
    if (NewState == ENABLE)
    {
        /*!< If requested card supports wide bus operation */
        if ((scr[1] & SD_WIDE_BUS_SUPPORT) != SD_ALLZERO)
        {
            /*!< Send CMD55 APP_CMD with argument as card's RCA.*/
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)RCA << 16, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();

            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }

            /*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
            sdio_command_response_config(SD_CMD_APP_SD_SET_BUSWIDTH, (uint32_t)0x2, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();

            errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }
            return(errorstatus);
        }
        else
        {
            errorstatus = SD_REQUEST_NOT_APPLICABLE;
            return(errorstatus);
        }
    }   /*!< If wide bus operation to be disabled */
    else
    {
        /*!< If requested card supports 1 bit mode operation */
        if ((scr[1] & SD_SINGLE_BUS_SUPPORT) != SD_ALLZERO)
        {
            /*!< Send CMD55 APP_CMD with argument as card's RCA.*/
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)RCA << 16, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();

            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }

            /*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
            sdio_command_response_config(SD_CMD_APP_SD_SET_BUSWIDTH, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();

            errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

            if (errorstatus != SD_OK)
            {
                return(errorstatus);
            }

            return(errorstatus);
        }
        else
        {
            errorstatus = SD_REQUEST_NOT_APPLICABLE;
            return(errorstatus);
        }
    }
}

/**
  * @brief  Find the SD card SCR register value.
  * @param  rca: selected card address.
  * @param  pscr: pointer to the buffer that will contain the SCR value.
  * @retval SD_Error: SD Card Error code.
  */
static SD_Error FindSCR(uint16_t rca, uint32_t *pscr)
{
    uint32_t index = 0;
    SD_Error errorstatus = SD_OK;
    uint32_t tempscr[2] = {0, 0};

    /*!< Set Block Size To 8 Bytes */
    /*!< Send CMD55 APP_CMD with argument as card's RCA */
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)8, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    /*!< Send CMD55 APP_CMD with argument as card's RCA */
    sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)RCA << 16, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }
    sdio_data_config(SD_DATATIMEOUT, (uint32_t)8, SDIO_DATABLOCKSIZE_8BYTES);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();

    /*!< Send ACMD51 SD_APP_SEND_SCR with argument as 0 */
    sdio_command_response_config(SD_CMD_SD_APP_SEND_SCR, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    errorstatus = CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);

    if (errorstatus != SD_OK)
    {
        return(errorstatus);
    }

    while (!(SDIO_STAT & (SDIO_FLAG_RXORE | SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_DTBLKEND | SDIO_FLAG_STBITE)))
    {
        if (sdio_flag_get(SDIO_FLAG_RXDTVAL) != RESET)
        {
            *(tempscr + index) = sdio_data_read();
            index++;
        }
    }

    if (sdio_flag_get(SDIO_FLAG_DTTMOUT) != RESET)
    {
        sdio_flag_clear(SDIO_FLAG_DTTMOUT);
        errorstatus = SD_DATA_TIMEOUT;
        return(errorstatus);
    }
    else if (sdio_flag_get(SDIO_FLAG_DTCRCERR) != RESET)
    {
        sdio_flag_clear(SDIO_FLAG_DTCRCERR);
        errorstatus = SD_DATA_CRC_FAIL;
        return(errorstatus);
    }
    else if (sdio_flag_get(SDIO_FLAG_RXORE) != RESET)
    {
        sdio_flag_clear(SDIO_FLAG_RXORE);
        errorstatus = SD_RX_OVERRUN;
        return(errorstatus);
    }
    else if (sdio_flag_get(SDIO_FLAG_STBITE) != RESET)
    {
        sdio_flag_clear(SDIO_FLAG_STBITE);
        errorstatus = SD_START_BIT_ERR;
        return(errorstatus);
    }

    /*!< Clear all the static flags */
    sdio_flag_clear(SDIO_STATIC_FLAGS);

    *(pscr + 1) = ((tempscr[0] & SD_0TO7BITS) << 24) | ((tempscr[0] & SD_8TO15BITS) << 8) | ((tempscr[0] & SD_16TO23BITS) >> 8) | ((tempscr[0] & SD_24TO31BITS) >> 24);

    *(pscr) = ((tempscr[1] & SD_0TO7BITS) << 24) | ((tempscr[1] & SD_8TO15BITS) << 8) | ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);

    return(errorstatus);
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */  

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

/**
  * @brief  Initializes the SD Card and put it into StandBy State (Ready for 
  *         data transfer).
  * @param  None
  * @retval None
  */
void SD_LowLevel_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    
    /* SDIO_DAT0(PC8)
     * SDIO_DAT1(PC9)
     * SDIO_DAT2(PC10)
     * SDIO_DAT3(PC11)
     * SDIO_CLK (PC12)
     * SDIO_CMD (PD2) */
    gpio_af_set(GPIOC, GPIO_AF_12, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    gpio_af_set(GPIOD, GPIO_AF_12, GPIO_PIN_2);
    
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
    
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
    
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    
    rcu_periph_clock_enable(RCU_SDIO);
    rcu_periph_clock_enable(RCU_DMA1);
}

/**
  * @brief  Configures the DMA2 Channel4 for SDIO Tx request.
  * @param  BufferSRC: pointer to the source buffer
  * @param  BufferSize: buffer size
  * @retval None
  */
void SD_LowLevel_DMA_TxConfig(uint32_t *BufferSRC, uint32_t BufferSize)
{
    dma_multi_data_parameter_struct dma_struct;
    
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FEE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_SDE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_TAE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_HTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    
    dma_channel_disable(DMA1, DMA_CH3);
    
    dma_deinit(DMA1, DMA_CH3);
    
    dma_struct.periph_addr = (uint32_t)SDIO_FIFO_ADDRESS;
    dma_struct.memory0_addr = (uint32_t)BufferSRC;
    dma_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_struct.number = BufferSize;
    dma_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_width = DMA_PERIPH_WIDTH_32BIT;
    dma_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_struct.periph_burst_width = DMA_PERIPH_BURST_4_BEAT;
    dma_struct.memory_burst_width = DMA_MEMORY_BURST_4_BEAT;
    dma_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_struct.critical_value = DMA_FIFO_4_WORD;
    dma_multi_data_mode_init(DMA1, DMA_CH3, &dma_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI4);
    dma_flow_controller_config(DMA1, DMA_CH3, DMA_FLOW_CONTROLLER_PERI);
    
    dma_channel_enable(DMA1, DMA_CH3);
}

/**
  * @brief  Configures the DMA2 Channel4 for SDIO Rx request.
  * @param  BufferDST: pointer to the destination buffer
  * @param  BufferSize: buffer size
  * @retval None
  */
void SD_LowLevel_DMA_RxConfig(uint32_t *BufferDST, uint32_t BufferSize)
{
    dma_multi_data_parameter_struct dma_struct;
    
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FEE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_SDE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_TAE);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_HTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    
    dma_channel_disable(DMA1, DMA_CH3);
    
    dma_deinit(DMA1, DMA_CH3);
    
    dma_struct.periph_addr = (uint32_t)SDIO_FIFO_ADDRESS;
    dma_struct.memory0_addr = (uint32_t)BufferDST;
    dma_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_struct.number = BufferSize;
    dma_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_width = DMA_PERIPH_WIDTH_32BIT;
    dma_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_struct.periph_burst_width = DMA_PERIPH_BURST_4_BEAT;
    dma_struct.memory_burst_width = DMA_MEMORY_BURST_4_BEAT;
    dma_struct.critical_value = DMA_FIFO_4_WORD;
    dma_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_multi_data_mode_init(DMA1, DMA_CH3, &dma_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI4);
    dma_interrupt_enable(DMA1, DMA_CH3, DMA_CHXCTL_FTFIE);
    dma_flow_controller_config(DMA1, DMA_CH3, DMA_FLOW_CONTROLLER_PERI);
    
    dma_channel_enable(DMA1, DMA_CH3);
}

/*!
    \brief      this function handles SDIO interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SDIO_IRQHandler(void)
{
    SD_ProcessIRQSrc();
}

void DMA1_Channel3_IRQHandler(void)
{
    SD_ProcessDMAIRQ();
}

//-------------------------------------------------------------------

int MMC_disk_status(void)
{
    return 0;
}

int MMC_disk_initialize(void)
{
    int result = 0;
    SD_Error Status = SD_OK;
    
    #if 1
    /*!< Disable SDIO Clock */
    sdio_clock_disable();
    
    /*!< Set Power State to OFF */
    sdio_power_state_set(SDIO_POWER_OFF);
    
    rcu_periph_reset_enable(RCU_SDIORST);
    rcu_periph_reset_disable(RCU_SDIORST);
    rcu_periph_clock_disable(RCU_SDIO);
    #endif
    
    /*!< Disable SDIO Clock */
    sdio_clock_disable();
    
    /*!< Set Power State to OFF */
    sdio_power_state_set(SDIO_POWER_OFF);
    
    rcu_periph_reset_enable(RCU_SDIORST);
    rcu_periph_reset_disable(RCU_SDIORST);
    rcu_periph_clock_disable(RCU_SDIO);
    
    nvic_irq_enable(SDIO_IRQn, 3, 0);
    nvic_irq_enable(DMA1_Channel3_IRQn, 3, 0);
    
    Status = SD_Init();
    
    if (Status != SD_OK)
    {
        result = 1;
    }
    
    return result;
}

int MMC_disk_read(uint8_t *buff, uint32_t sector, uint32_t count)
{
    int result = 0;
    SD_Error Status = SD_OK;
    
    if(count == 1)
    {
        Status = SD_ReadBlock(buff, (uint64_t)sector << 9 , MMC_sector_size());
    }
    else
    {
        Status = SD_ReadMultiBlocks(buff, (uint64_t)sector << 9 , MMC_sector_size(), count);
    }
    
    if (Status != SD_OK)
    {
        QL_LOG_E("SD Read Fail %d", Status);
        result = 1;
    }
    
#ifdef SD_DMA_MODE
    Status = SD_WaitReadOperation();
    if (Status != SD_OK)
    {
        QL_LOG_E("SD Read Fail %d", Status);
        result = 1;
    }
    while (SD_GetStatus() != SD_TRANSFER_OK);
#endif
    
    return result;
}

int MMC_disk_write(const uint8_t *buff, uint32_t sector, uint32_t count)
{
    int result = 0;
    SD_Error Status = SD_OK;
    
    if (count == 1)
    {
        Status = SD_WriteBlock((uint8_t *)buff, (uint64_t)sector << 9 ,MMC_sector_size());
    }
    else
    {
        Status = SD_WriteMultiBlocks((uint8_t *)buff, (uint64_t)sector << 9 ,MMC_sector_size(), count);
    }
    
    if (Status != SD_OK)
    {
        QL_LOG_E("SD Write Fail %d", Status);
        result = 1;
    }
    
#ifdef SD_DMA_MODE
    Status = SD_WaitWriteOperation();
    if (Status != SD_OK)
    {
        QL_LOG_E("SD Write Fail %d", Status);
        result = 1;
    }
    while(SD_GetStatus() != SD_TRANSFER_OK);
#endif
    
    return result;
}

uint32_t MMC_sector_count(void)
{
    return (SDCardInfo.CardCapacity / 512);
}

uint32_t MMC_sector_size(void)
{
    return 512;
}

uint32_t MMC_block_size(void)
{
    return 16;
}
