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
  Name: example_lcx9h_iic_fwdl.c
  Current support: LC29HAI, LC29HAA, LC29HBA, LC29HCA, LC29HDA
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hongbin  Create file
*/

#include "example_def.h"

#ifdef __EXAMPLE_LCx9H_IIC_FWDL__

#include "ql_application.h"
#include "ql_iic.h"
#include "ql_ff_user.h"

#include "ql_log_undef.h"
#define LOG_TAG "lcx9h"
#define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"
#include "ql_delay.h"

#ifndef uint32_t
#define uint32_t unsigned int
#endif


#define QL_LCX9H_FWDL_VERSON               ("QECTEL_LCX9H_I2C_FWDL_V1.0,"__DATE__)


#define QL_LCX9H_ADDRESS                       (0x08 << 1)
#define QL_LCX9H_WRTITE_ADDRESS                (QL_LCX9H_ADDRESS & 0xFE)
#define QL_LCX9H_READ_ADDRESS                  (QL_LCX9H_ADDRESS | 0x01)

#define QL_LCX9H_SEGMENT_SIZE                  (256)

#define QL_LCX9H_WDT_REGISTER_ADDRESS          (0xA2080000)
#define QL_LCX9H_WDT_VALUE                     (0x0010)

#define QL_LCX9H_DA_RUN_ADDRESS                (0x04000000)
#define QL_LCX9H_DA_LENGTH                     (0X000006A44)
#define QL_LCX9H_DA_BLOCK_SIZE                 (1024)

#define QL_LCX9H_FORMAT_FLASH_ADDRESS          (0x08000000)
#define QL_LCX9H_FORMAT_FLASH_LENGTH           (0x003E0000)
#define QL_LCX9H_FORMAT_FLASH_BLOCK_SIZE       (0x00010000)

#define QL_LCX9H_FW_BLOCK_SIZE                 (0x1000)
#define QL_LCX9H_FW_FILE_PATH_MAX_LEN          (256)
#define QL_LCX9H_FW_CFG_TXT_NAME               "flash_download.cfg"

#define QL_LCX9H_BROM_ERROR                    (0x1000)
#define QL_LCX9H_HW_CORE                       (0x3335)

#define QL_LCx9H_IIC_DELAY                   10                            //unit ms

#define QL_LCx29H_IIC_MODE                           (IIC_MODE_HW0_POLLING)
#define QL_LCx29H_IIC_SPEED                          (IIC_SPEED_FAST)
#define QL_LCx29H_IIC_TX_SIZE                        (2 * 1024U)
#define QL_LCx29H_IIC_RX_SIZE                        (2 * 1024U)


#pragma pack(push, 1)  // Unalign bytes
typedef struct
{
    uint8_t Header;
    uint8_t CmdType;
    uint16_t Length;
    uint16_t CmdID;
    uint32_t FormatAddr;
    uint32_t FormatSize;
}Ql_LCX9H_RaceDaFormatSend_Typedef;

typedef struct
{
    uint8_t Header;
    uint8_t CmdType;
    uint16_t Length;
    uint16_t CmdID;
    uint8_t Status;
    uint32_t FormatAddr;
}Ql_LCX9H_RaceDaFormatResponse_Typedef;

typedef struct
{
    uint8_t Header;
    uint8_t CmdType;
    uint16_t Length;
    uint16_t CmdID;
    uint32_t StartAddr;
    uint16_t DataLength;
    uint32_t CheckSum;
    uint8_t Data[4096];
}Ql_LCX9H_RaceFwWrite_Typedef;

typedef struct
{
    uint8_t Header;
    uint8_t CmdType;
    uint16_t Length;
    uint16_t CmdID;
    uint8_t Status;
    uint32_t StartAddr;
}Ql_LCX9H_RaceFwResponse_Typedef;

#pragma pack(pop)

typedef struct
{
    uint8_t File[64];
    uint8_t FileName[32];
    uint32_t StartAddress;
}Ql_LCX9H_FileInfo_Typedef;

typedef struct
{
    int8_t (*Func)(void);
    uint8_t FuncName[32];
}Ql_LCX9H_FWDL_FuncRun_Typedef;

static int8_t Ql_LCX9H_FWDL_HandShakeModule(void);
static int8_t Ql_LCX9H_FWDL_DisableModuleWDT(void);
static int8_t Ql_LCX9H_FWDL_SendDaFile(void);
static int8_t Ql_LCX9H_FWDL_JumpToDa(void);
static int8_t Ql_LCX9H_FWDL_SyncDa(void);
static int8_t Ql_LCX9H_FWDL_FormatFlash(void);
static int8_t Ql_LCX9H_FWDL_SendFwFile(void);

// Fill in the functions according to the running order!!
Ql_LCX9H_FWDL_FuncRun_Typedef Ql_LCX9H_FWDL_FuncRunList[] =
{
    {Ql_LCX9H_FWDL_HandShakeModule, "HandShakeModule"},
    {Ql_LCX9H_FWDL_DisableModuleWDT, "DisableModuleWDT"},
    {Ql_LCX9H_FWDL_SendDaFile, "SendDaFile"},
    {Ql_LCX9H_FWDL_JumpToDa, "JumpToDa"},
    {Ql_LCX9H_FWDL_SyncDa, "SyncDa"},
    {Ql_LCX9H_FWDL_FormatFlash, "FormatFlash"},
    {Ql_LCX9H_FWDL_SendFwFile, "SendFwFile"},
};

const char DaFilePath[] = "/lcx9h_fwdl/LC29HBANR11A04SV01_CSA2/da_i2c.bin";
const char FwFileDir[] = "/lcx9h_fwdl/LC29HBANR11A04SV01_CSA2/";

static Ql_LCX9H_FileInfo_Typedef FwFileInfos[8];  // 

void Ql_Example_Task(void *Param)
{
    int32_t ret_code;

    QL_LOG_I("Version: %s, example start!", QL_LCX9H_FWDL_VERSON);

    // init i2c
    ret_code = Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);

    if(ret_code != QL_IIC_STATUS_OK)
    {
        QL_LOG_E("IIC init failed!, ret_code = %d", ret_code);
        goto TASK_END;
    }

    ret_code = Ql_FatFs_Mount();
    
    if(ret_code != 0)
    {
        QL_LOG_E("FatFs mount failed!");
    }

    while (1)
    {
        bool is_success = true;

        QL_LOG_I("LX29H module download firmware start");
 
        for (int i = 0; i < sizeof(Ql_LCX9H_FWDL_FuncRunList) / sizeof(Ql_LCX9H_FWDL_FuncRunList[0]); i++)
        {
            QL_LOG_I("==============( %d ) %s Start ============", i, Ql_LCX9H_FWDL_FuncRunList[i].FuncName);

            ret_code = Ql_LCX9H_FWDL_FuncRunList[i].Func();
            if (ret_code != 0)
            {
                is_success = false;
                QL_LOG_E("%s Fail, ret_code = %d", Ql_LCX9H_FWDL_FuncRunList[i].FuncName, ret_code);
                break;
            }

            QL_LOG_I("==============%s End, %s============", Ql_LCX9H_FWDL_FuncRunList[i].FuncName, "Success");
        }

        QL_LOG_I("LX29H module download firmware finish %s", is_success ? "success" : "fail");
        break;
    }

TASK_END:
    Ql_FatFs_UnMount();
    QL_LOG_E("Current task end");
    vTaskDelete(NULL);
}


/*******************************************************************************
* Name: static void Ql_LCx9H_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
* Brief: lcx9h serial module iic port error handle
* Input: 
*   Param: iic status
* Output:
*   void
* Return:
*   void
*******************************************************************************/
static void Ql_LCx9H_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
{
    switch(iic_state)
    {
        case QL_IIC_STATUS_OK:
            //QL_LOG_I("lcx9h module iic port r/w ok.");
            break;
        case QL_IIC_STATUS_BUSY:
            Ql_IIC_CheckBusSatus();
            Ql_IIC_DeInit();
            QL_LOG_E("lcx9h module iic port busy and re-init.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
             Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);
            break;
        case QL_IIC_STATUS_TIMEOUT:
            QL_LOG_E("lcx9h module iic port r/w timeout.");
            break;
        case QL_IIC_STATUS_NOACK:
            // QL_LOG_E("lcx9h module iic port r/w no ack.");
            // Ql_IIC_CheckBusSatus();
            // Ql_IIC_DeInit();
            // vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
            // Ql_IIC_Init(QL_LCx29H_IIC_MODE, QL_LCx29H_IIC_SPEED, QL_LCx29H_IIC_TX_SIZE, QL_LCx29H_IIC_RX_SIZE);
            break;
        case QL_IIC_STATUS_SEM_ERROR:
            QL_LOG_E("lcx9h module iic port semaphore error.");
            break;
        case QL_IIC_STATUS_MODE_ERROR:
            QL_LOG_E("lcx9h module iic port mode error.");
            break;
        case QL_IIC_STATUS_MEMORY_ERROR:
            QL_LOG_E("lcx9h module iic port memory error.");
            break;
        case QL_IIC_STATUS_ADDSEND_ERROR:
            QL_LOG_E("lcx9h module iic port address send error.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
            break;
        default:
            QL_LOG_W("lcx9h module iic port unknown state:%d",iic_state);
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(QL_LCx9H_IIC_DELAY));
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FindFwFileInfo(const uint8_t* pFwFileDir, Ql_LCX9H_FileInfo_Typedef* pFileInfo, uint8_t* pCount)
* Brief: Find the firmware file information from the cfg file
* Input: 
*   Param: pFwFileDir: cfg file path
* Output:
*   Param: pFileInfo: firmware file information
*   Param: pCount: firmware file count
* Return:
*   0: success; other: failure
*******************************************************************************/
static int8_t Ql_LCX9H_FindFwFileInfo(const uint8_t* pFwFileDir, Ql_LCX9H_FileInfo_Typedef* pFileInfo, uint8_t* pCount)
{ 
    int8_t func_ret = 0;
    uint8_t count = 0;
    uint8_t* ptr_buffer = NULL;
    uint8_t* ptr_start = NULL;
    uint8_t* ptr_offset = NULL;

    char* p_file_start = NULL;
    char* p_name_start = NULL;
    char* p_address_start = NULL;

    FIL fp;
    int file_ret_code;
    char cfg_file_path[QL_LCX9H_FW_FILE_PATH_MAX_LEN];
    uint32_t file_length = 4096;
    uint32_t read_length = 0;
    Ql_LCX9H_FileInfo_Typedef* p_file_info = pFileInfo;

    sprintf(cfg_file_path, "%s%s", pFwFileDir, QL_LCX9H_FW_CFG_TXT_NAME);
    
    // Open CFG File
    file_ret_code = Ql_FatFs_OpenFile(cfg_file_path, &fp, QL_FILE_READ);
    if (file_ret_code != 0)
    {
        func_ret = -1;
        QL_LOG_E("(%d) Ql_FatFs_OpenFile, ret_code = %d", __LINE__, file_ret_code);
        goto FUNC_END;
    }
    QL_LOG_D("(%d) Ql_FatFs_OpenFile, %s", __LINE__, cfg_file_path);

    file_length = Ql_FatFs_ReadFileSize(&fp);

    ptr_buffer = pvPortMalloc(file_length);
    if (ptr_buffer == NULL)
    {
        func_ret = -2;
        QL_LOG_E("(%d) pvPortMalloc Fail", __LINE__);
        goto FUNC_END;
    }
    file_ret_code = Ql_FatFs_ReadFile(&fp, ptr_buffer, file_length, &read_length);
    if (file_ret_code != 0)
    {
        func_ret = -3;
        QL_LOG_E("(%d) Ql_FatFs_ReadFile, ret_code = %d, %s", __LINE__, file_ret_code);
        goto FUNC_END;
    }
    QL_LOG_D("(%d) Ql_FatFs_ReadFile, file_length %d, read_length %d", __LINE__, file_length, read_length);

    ptr_start = ptr_buffer;
    while ((ptr_offset = strstr(ptr_start, "rom:")) != NULL)
    {
        ptr_offset += strlen("rom:");

        p_file_start = strstr(ptr_offset, "file: ");
        p_name_start = strstr(ptr_offset, "name: ");
        p_address_start = strstr(ptr_offset, "begin_address: ");
        
        if (!p_file_start || !p_name_start || !p_address_start)
        {
            func_ret = -4;
            goto FUNC_END;
        }

        sscanf(p_file_start, "file: %s", p_file_info->File);
        sscanf(p_name_start, "name: %s", p_file_info->FileName);
        sscanf(p_address_start, "begin_address: 0x%x", &p_file_info->StartAddress);

        QL_LOG_I("parsed file: %s; name: %s; begin_address: 0x%x", p_file_info->File, p_file_info->FileName, p_file_info->StartAddress);
        
        count++;
        ptr_start = ptr_offset;
        p_file_info++;
    }

    *pCount = count;
    func_ret = 0;

FUNC_END: 
    file_ret_code = Ql_FatFs_CloseFile(&fp);
    if (file_ret_code != 0)
    {
        QL_LOG_E("(%d) Ql_FatFs_CloseFile, ret_code = %d", __LINE__, file_ret_code);
    }

    if(ptr_buffer != NULL)
    {
        vPortFree(ptr_buffer);
        ptr_buffer = NULL;
    }

    return func_ret;
}

/*******************************************************************************
* Name: static uint16_t Ql_LCX9H_DaChecksum(const uint8_t *pBuf, uint32_t Length)
* Brief: Calculate the checksum of the DA file
* Input: 
*   Param: pBuf: buffer
*   Param: Length: length of the buffer
* Output:
*   void
* Return:
*   checksum
*******************************************************************************/
static uint16_t Ql_LCX9H_DaChecksum(const uint8_t *pBuf, uint32_t Length)
{
    uint16_t checksum = 0;
    if (pBuf == NULL || Length == 0) {

        return 0;
    }

    int i = 0;
    for (i = 0; i < Length / 2; i++) {
        checksum ^= *(uint16_t *)(pBuf + i * 2);
    }

    if ((Length % 2) == 1) {
        checksum ^= pBuf[i * 2];
    }

    return checksum;
}

/*******************************************************************************
* Name: static uint32_t Ql_LCX9H_FwChecksum(const uint8_t *pBuf, uint32_t Length)
* Brief: Calculate the checksum of the FW file
* Input: 
*   Param: pBuf: buffer 
*   Length: Length: buffer length
* Output:
*   void
* Return:
*   checksum
*******************************************************************************/
static uint32_t Ql_LCX9H_FwChecksum(const uint8_t *pBuf, uint32_t Length)
{
    uint32_t checksum = 0;

    if (pBuf == NULL || Length == 0)
    {
        return 0;
    }
    for (int i = 0; i < Length; i++)
    {
        checksum += *(pBuf + i);
    }

    return checksum;
}

/*******************************************************************************
* Name: static void Ql_LCX9H_DataToHexString(const uint8_t* pInData, int Length, uint8_t* pOutData)
* Brief: Converts data to a hexadecimal string
* Input: 
*   Param: pInData: input data
*   Param: Length: data length
* Output:
*   Param: pOutData: output data
* Return:
*   None.
*******************************************************************************/
static void Ql_LCX9H_DataToHexString(const uint8_t* pInData, int Length, uint8_t* pOutData)
{
    
    memset(pOutData, 0, Length);

    for(int i = 0; i < Length; i++)
    {
        sprintf(pOutData + strlen(pOutData), " %02X", *(pInData + i));
    }
}

/*******************************************************************************
* Name: static void Ql_LCX9H_NumberToArray(uint32_t Value, uint8_t* pData)
* Brief: Converts integer numbers to array
* Input: 
*   Param: Value: integer number
* Output:
*   Param: pData: array to store the converted data
* Return:
*   None.
*******************************************************************************/
static void Ql_LCX9H_NumberToArray(uint32_t Value, uint8_t* pData)
{
    pData[0] = (Value) & 0xFF;
    pData[1] = (Value >> (8 * 1)) & 0xFF;
    pData[2] = (Value >> (8 * 2)) & 0xFF;
    pData[3] = (Value >> (8 * 3)) & 0xFF;
    // pData[4] = (Value >> (8 * 4)) & 0xFF;
    // pData[5] = (Value >> (8 * 5)) & 0xFF;
    // pData[6] = (Value >> (8 * 6)) & 0xFF;
    // pData[7] = (Value >> (8 * 7)) & 0xFF;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
* Brief: Sends and receives data and no memory needs to be allocated to the data mark, and the function automatically checks the data mark 0x66 
* Input: 
*   Param: pSendData: pointer to the data to be sent;
*   Param: SendLength: length of the data to be sent;
*   Param: RecvLength: length of the data to be received;
* Output:
*   Param: pRecvData: pointer to the data received;
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
{
    Ql_IIC_Status_TypeDef iic_status;

    if((pSendData != NULL) && (SendLength != 0))
    {
        uint32_t send_length = 0;
        uint32_t had_send_length = 0;
        uint8_t* ptr_offset = (uint8_t *)pSendData;


        while (had_send_length < SendLength)
        {
            send_length = (SendLength - had_send_length) > QL_LCX9H_SEGMENT_SIZE ? QL_LCX9H_SEGMENT_SIZE : (SendLength - had_send_length);
            iic_status = Ql_IIC_Write(QL_LCX9H_WRTITE_ADDRESS, 0, ptr_offset, send_length, 1000);
            if (iic_status != QL_IIC_STATUS_OK)
            {
                Ql_LCx9H_IIC_ErrHandle(iic_status);
                return -1;
            }

            had_send_length += send_length;
            ptr_offset = (uint8_t*)ptr_offset + send_length;
            vTaskDelay(1);  //pdMS_TO_TICKS(1)
        }
    }

    if((pRecvData != NULL) && (RecvLength != 0))
    {
        uint8_t recv_data[128] ={ 0 };

        if (RecvLength >= 128 - 1)
        {
            return -4;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        // read data and data mask (0x66)
        iic_status = Ql_IIC_Read(QL_LCX9H_READ_ADDRESS, 0, (uint8_t *)recv_data, RecvLength + 1, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx9H_IIC_ErrHandle(iic_status);
            return -2;
        }

        if (recv_data[RecvLength] != 0x66)
        {
            return -3;
        }

        memcpy(pRecvData, recv_data, RecvLength);
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout)
* Brief: Send and wait to receive data
* Input: 
*   Param: pSendData: send data buffer
*   Param: SendLength: send data length
*   Param: RecvLength: receive data length
*   Param: Timeout: waiting timeout
* Output:
*   Param: pRecvData: receive data buffer
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout)
{

    if(RecvLength == 0)
    {
        return -1;
    }

    for(int i = 0; i < (Timeout / 10); i++)
    {
        if(Ql_LCX9H_FWDL_SendAndRecvData(pSendData, SendLength, pRecvData, RecvLength) != 0)
        {
            continue;
        }

        return 0;
    }

    return -2;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SendAndCheckNumber(uint32_t SendData, uint8_t SendLength, uint32_t RecvData, uint16_t RecvLength)
* Brief: Sends and receives data and determines whether the received data is correct
* Input: 
*   Param: SendData: data to be sent
*   Param: SendLength: length of data to be sent
*   Param: RecvData: data to be received
*   Param: RecvLength: length of data to be received
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SendAndCheckNumber(uint32_t SendData, uint8_t SendLength, uint32_t RecvData, uint16_t RecvLength)
{   
    int8_t ret_code = 0;
    uint8_t send_data[8] = { 0 };
    uint8_t recv_data[8] ={ 0 };

    Ql_LCX9H_NumberToArray(SendData, send_data);

    QL_LOG_I("(%d) send data: 0x%X (%d byte), waiting recv data: 0x%X (%d byte)", __LINE__, (uint32_t)SendData, SendLength, (uint32_t)RecvData, RecvLength);

    ret_code = Ql_LCX9H_FWDL_SendAndRecvData(send_data, SendLength, recv_data, RecvLength);
    if(ret_code != 0)
    {
        return -2;
    }

    if(memcmp(recv_data, &RecvData, RecvLength) != 0)
    {
        uint8_t buffer[64] ={0};
        Ql_LCX9H_DataToHexString(recv_data, RecvLength, buffer);
        QL_LOG_E("(:%d) had recv length=%d, data: %s", __LINE__, RecvLength, buffer);
        return -3;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_HandShakeModule(void)
* Brief: Shaking hands with module
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/

static int8_t Ql_LCX9H_FWDL_HandShakeModule(void)
{
    int8_t func_ret = -1;
    uint16_t waiting_count =0;
    bool is_hand_shake = false;

    uint8_t send_data[12] = { 0 };
    uint8_t recv_data[24] ={ 0 };

    // waiting for hand shake
    while (1)
    {
        Ql_LCX9H_NumberToArray(0xA0, send_data);
        if((Ql_LCX9H_FWDL_SendAndRecvData(send_data, 1, recv_data, 1) == 0) && (recv_data[0] == 0x5F))
        {
            is_hand_shake = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        if(waiting_count % 100 == 0)
        {
            QL_LOG_I("waiting for handshake...");
        }

        waiting_count++;
        if(waiting_count > 100 * 50)
        {
            is_hand_shake = false;
            break;
        }
    }

    do
    {
        func_ret = -1;
        if(is_hand_shake != true)
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x0A, 1, 0xF5, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x50, 1, 0xAF, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x05, 1, 0xFA, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xD0, 1, 0xD0, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x80000008, 4, 0x80000008, 4) != 0)
        {
            break;
        }

        /* Get Hardware Information */
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x00000001, 4, 0x00000001, 4) != 0)
        {
            break;
        }
        // Brom Status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(:%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);

        // HW_CODE
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0))
        {
            break;
        }
        QL_LOG_I("(%d) recv HW_CODE: 0X%04X", __LINE__, *(uint16_t*)recv_data);
        
        // Brom Status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);


        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xD0, 1, 0xD0, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x8000000C, 4, 0x8000000C, 4) != 0)
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x00000001, 4, 0x00000001, 4) != 0)
        {
            break;
        }
        // Brom Status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(:%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);

        // HW_SUBCODE
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0))
        {
            break;
        }
        QL_LOG_I("(%d) recv HW_SUBCODE: 0X%04X", __LINE__, *(uint16_t*)recv_data);
        
        // Brom Status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);

        func_ret = 0;

    } while (false);

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_DisableModuleWDT(void)
* Brief: Disable module WDT
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_DisableModuleWDT(void)
{
    int8_t func_ret = -1;

    uint8_t recv_data[24] ={ 0 };

    
    do
    {
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xD2, 1, 0xD2, 1) != 0)
        {
            break;
        }

        if((Ql_LCX9H_FWDL_SendAndCheckNumber(QL_LCX9H_WDT_REGISTER_ADDRESS, 4, QL_LCX9H_WDT_REGISTER_ADDRESS, 4) != 0))
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x01, 4, 0x01, 4) != 0)
        {
            break;
        }

        // Brom status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        // WDT
        if((Ql_LCX9H_FWDL_SendAndCheckNumber(QL_LCX9H_WDT_VALUE, 2, QL_LCX9H_WDT_VALUE, 2) != 0))
        {
            break;
        }

        // Brom status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        func_ret = 0;

    } while (false);

    

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SendDaFile(void)
* Brief: Send data file to LCX9H
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SendDaFile(void)
{
    int8_t func_ret = -1;

    uint8_t recv_data[24] ={ 0 };

    FIL fp;
    uint16_t readLength = 0;
    uint32_t had_read_da_length = 0;
    int8_t file_ret_code = 0;
    uint8_t* p_da_file_block = NULL;
    uint16_t block_count = 0;
    uint16_t remaining_length = 0;
    uint16_t da_checksum = 0;

    uint8_t progress = 0;
    uint8_t temp_progress = 0;
    uint8_t progress_step = 5;

    
    do
    {
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xD7, 1, 0xD7, 1) != 0)
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(QL_LCX9H_DA_RUN_ADDRESS, 4, QL_LCX9H_DA_RUN_ADDRESS, 4) != 0)
        {
            break;
        }

        if((Ql_LCX9H_FWDL_SendAndCheckNumber(QL_LCX9H_DA_LENGTH, 4, QL_LCX9H_DA_LENGTH, 4) != 0))
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x00, 4, 0x00, 4) != 0)
        {
            break;
        }

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom Status: 0X%04X", __LINE__, *(uint16_t*)recv_data);

        had_read_da_length = 0;
        block_count = QL_LCX9H_DA_LENGTH / QL_LCX9H_DA_BLOCK_SIZE;
        remaining_length = QL_LCX9H_DA_LENGTH % QL_LCX9H_DA_BLOCK_SIZE;

        if(remaining_length > 0)
        {
            block_count += 1;
        }

        p_da_file_block = pvPortMalloc(QL_LCX9H_DA_BLOCK_SIZE);
        if (p_da_file_block == NULL)
        {
            QL_LOG_E("(%d) p_da_file_block pvPortMalloc Fail", __LINE__);
            break;
        }

        file_ret_code = Ql_FatFs_OpenFile(DaFilePath, &fp, QL_FILE_READ);
        if (file_ret_code != 0)
        {
            QL_LOG_E("(%d) Ql_FatFs_OpenFile, ret_code = %d", __LINE__, file_ret_code);
            break;
        }

        for (int i = 0; i < block_count; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            file_ret_code = Ql_FatFs_ReadFile(&fp, p_da_file_block, QL_LCX9H_DA_BLOCK_SIZE, &readLength);
            if ((file_ret_code != 0) || (readLength == 0))
            {
                QL_LOG_E("(%d) Ql_FatFs_ReadFile, ret_code = %d, readLength=%d", __LINE__, file_ret_code, readLength);
                break;
            }

            da_checksum ^= Ql_LCX9H_DaChecksum(p_da_file_block, readLength);
            had_read_da_length += readLength;
            
            if (Ql_LCX9H_FWDL_SendAndRecvData(p_da_file_block, readLength, NULL, 0) != 0)
            {
                QL_LOG_E("(%d) send da file block error", __LINE__);
                break;
            }

            temp_progress = (i * 100) / block_count;
            if ((temp_progress > progress) && (progress < 100))
            {
                progress += ((temp_progress - progress) / progress_step + 1) * (progress_step);
                if (progress >= 100)
                {
                    progress = 100;
                }
                QL_LOG_I("(%d) send da file block (%02d:%02d), %02d%%", __LINE__, i + 1, block_count, progress);
            }
        }

        file_ret_code = Ql_FatFs_CloseFile(&fp);
        if (file_ret_code != 0)
        {
            QL_LOG_E("(%d) Ql_FatFs_CloseFile, ret_code = %d", __LINE__, file_ret_code);
        }

        if (had_read_da_length != QL_LCX9H_DA_LENGTH)
        {
            QL_LOG_E("(%d) read form da file length = %d unequally defined = %d", __LINE__, had_read_da_length, QL_LCX9H_DA_LENGTH);
        }

        if((Ql_LCX9H_FWDL_SendAndWaitingResponse(NULL, 0, recv_data, 2, 500) != 0) || (*(uint16_t*)recv_data != da_checksum))
        {
            QL_LOG_I("(%d) recv checksum of DA: 0x%04X, calc=0x%04x", __LINE__, *(uint16_t*)recv_data, da_checksum);
            break;
        }
        QL_LOG_I("(%d) recv checksum of DA: 0x%04X, calc=0x%04x", __LINE__, *(uint16_t*)recv_data, da_checksum);

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        func_ret = 0;

    } while (false);

    if(p_da_file_block != NULL)
    {
        vPortFree(p_da_file_block);
        p_da_file_block = NULL;
    }

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_JumpToDa(void)
* Brief: Jump to DA
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_JumpToDa(void)
{
    int8_t func_ret = -1;

    uint8_t recv_data[24] ={ 0 };

    
    do
    {
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xD5, 1, 0xD5, 1) != 0)
        {
            break;
        }

        if((Ql_LCX9H_FWDL_SendAndCheckNumber(QL_LCX9H_DA_RUN_ADDRESS, 4, QL_LCX9H_DA_RUN_ADDRESS, 4) != 0))
        {
            break;
        }

        // Brom status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_I("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Brom status: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        func_ret = 0;
    } while (false);

    

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SyncDa(void)
* Brief: Sync with DA
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SyncDa(void)
{
    int8_t func_ret = -1;
    uint8_t recv_data[24] ={ 0 };

    do
    {
        if((Ql_LCX9H_FWDL_SendAndWaitingResponse(NULL, 0, recv_data, 1, 500) != 0) && (recv_data[0] != 0xC0))
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x3F, 1, 0x0C, 1) != 0)
        {
            break;
        }

        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xF3, 1, 0x3F, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0xC0, 1, 0xF3, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x0C, 1, 0x5A, 1) != 0)
        {
            break;
        }
        if(Ql_LCX9H_FWDL_SendAndCheckNumber(0x01, 1, 0x69, 1) != 0)
        {
            break;
        }

        // Brom status
        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0) || (*(uint16_t*)recv_data >= QL_LCX9H_BROM_ERROR))
        {
            QL_LOG_E("(%d) recv Flash Manufacturer ID: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Flash Manufacturer ID: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0))
        {
            QL_LOG_E("(%d) recv Flash Device ID 1: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Flash Device ID 1: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 2) != 0))
        {
            QL_LOG_E("(%d) recv Flash Device ID 2: 0x%04X", __LINE__, *(uint16_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Flash Device ID 2: 0x%04X", __LINE__, *(uint16_t*)recv_data);

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 4) != 0))
        {
            QL_LOG_E("(%d) recv Flash Start Addr: 0x%08X", __LINE__, *(uint32_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Flash Start Addr: 0x%08X", __LINE__, *(uint32_t*)recv_data);

        if((Ql_LCX9H_FWDL_SendAndRecvData(NULL, 0, recv_data, 4) != 0))
        {
            QL_LOG_E("(%d) recv Flash Size: 0x%08X", __LINE__, *(uint32_t*)recv_data);
            break;
        }
        QL_LOG_I("(%d) recv Flash Size: 0x%08X", __LINE__, *(uint32_t*)recv_data);

        func_ret = 0;
    } while (false);

    

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_FormatFlash(void)
* Brief: Format Flash
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_FormatFlash(void)
{
    int8_t func_ret = -1;
    uint8_t* ptr_cmd = NULL;

    uint8_t recv_data[64] ={ 0 };
    uint8_t buffer[128] = { 0 };

    uint32_t flash_format_count = 0;
    uint8_t progress = 0;
    uint8_t temp_progress = 0;
    uint8_t progress_step = 5;

    
    do
    {
        func_ret = 0;
        Ql_LCX9H_RaceDaFormatResponse_Typedef response;
        Ql_LCX9H_RaceDaFormatSend_Typedef send_cmd = {
            .Header = 0x05,
            .CmdType = 0x5A,
            .Length = 0x000A,
            .CmdID = 0x2104,
            .FormatAddr = QL_LCX9H_FORMAT_FLASH_ADDRESS,
            .FormatSize = QL_LCX9H_FORMAT_FLASH_BLOCK_SIZE
        };
        ptr_cmd = &send_cmd;

        flash_format_count = QL_LCX9H_FORMAT_FLASH_LENGTH / QL_LCX9H_FORMAT_FLASH_BLOCK_SIZE;

        QL_LOG_I("Format Flash address: 0x%08X, length: 0x%08X", QL_LCX9H_FORMAT_FLASH_ADDRESS, QL_LCX9H_FORMAT_FLASH_LENGTH);

        for(int i = 0; i <= flash_format_count; i++)
        {
            send_cmd.FormatAddr = QL_LCX9H_FORMAT_FLASH_ADDRESS + i * QL_LCX9H_FORMAT_FLASH_BLOCK_SIZE;
            for (int j = 0; j < sizeof(send_cmd); j++)
            {
                vTaskDelay(1);
                if (Ql_LCX9H_FWDL_SendAndRecvData(ptr_cmd + j, 1, NULL, 0) != 0)
                {
                    QL_LOG_E("Format Flash address send no response");
                    func_ret = -1;
                    break;
                }
            }

            if(Ql_LCX9H_FWDL_SendAndWaitingResponse(NULL, 0, &recv_data, sizeof(response), 500) != 0)
            {
                QL_LOG_E("Format Flash address recv no response");
                func_ret = -1;
                break;
            }

            memcpy(&response, recv_data, sizeof(response));
            if((response.Status != 0) || (response.FormatAddr != send_cmd.FormatAddr))
            {
                func_ret = -1;
                Ql_LCX9H_DataToHexString(&send_cmd, sizeof(send_cmd), buffer);
                Ql_LCX9H_DataToHexString(&response, sizeof(response), buffer + sizeof(send_cmd) + 1);
                QL_LOG_E("Format Flash address send:%s, recv:%s", buffer, buffer + sizeof(send_cmd) + 1);
                
                break;
            }

            temp_progress = (i * 100) / flash_format_count;
            if ((temp_progress > progress) && (progress < 100))
            {
                progress += ((temp_progress - progress) / progress_step + 1) * (progress_step);
                if (progress >= 100)
                {
                    progress = 100;
                }
                QL_LOG_I("Format Flash progress: %d%%", progress);
            }

        }

    } while (false);

    return func_ret;

}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWDL_SendFwFile(void)
* Brief: Send firmware file to LCX9H
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWDL_SendFwFile(void)
{
    int8_t func_ret = -1;
    uint8_t buffer[64] = { 0 };
    static uint8_t fw_file_path[QL_LCX9H_FW_FILE_PATH_MAX_LEN] = { 0 };
    FIL fp;
    uint32_t file_count = 0;
    uint32_t data_start_addrress = 0;
    uint32_t file_size = 0;
    uint32_t read_length = 0;
    uint32_t had_read_length = 0;

    uint8_t progress = 0;
    uint8_t temp_progress;
    uint8_t progress_step = 2;
    uint8_t file_info_size = 0;
    int8_t ret;
    Ql_LCX9H_RaceFwResponse_Typedef response;
    Ql_LCX9H_RaceFwWrite_Typedef *send_cmd = NULL;

    do
    {
        func_ret = 0;

        send_cmd = pvPortMalloc(sizeof(Ql_LCX9H_RaceFwWrite_Typedef));

        send_cmd->Header = 0x05;
        send_cmd->CmdType = 0x5A;
        send_cmd->Length = 0x100C;
        send_cmd->CmdID = 0x2100;
        send_cmd->DataLength = QL_LCX9H_FW_BLOCK_SIZE;

        memset(&FwFileInfos, 0, sizeof(FwFileInfos));

        ret = Ql_LCX9H_FindFwFileInfo(FwFileDir, &FwFileInfos, &file_info_size);

        if(ret != 0)
        {
            QL_LOG_E("Read cfg file error code %d", ret);
            func_ret = -1;
            break;
        }

        while(file_count < file_info_size)
        {
            memset(fw_file_path, 0, sizeof(fw_file_path));
            sprintf(fw_file_path, "%s%s", FwFileDir, FwFileInfos[file_count].File);
    
            data_start_addrress = FwFileInfos[file_count].StartAddress;
            QL_LOG_I("(%d : %d), file: %s, file name: %s, Send FW address: 0x%08X", (file_count + 1), file_info_size, FwFileInfos[file_count].File, FwFileInfos[file_count].FileName, data_start_addrress);

            if(Ql_FatFs_OpenFile(fw_file_path, &fp, QL_FILE_READ) != 0)
            {
                break;
            }
            
            file_size = Ql_FatFs_ReadFileSize(&fp);

            progress = 0;
            had_read_length = 0;

            do
            {
                read_length = QL_LCX9H_FW_BLOCK_SIZE;
                send_cmd->StartAddr = data_start_addrress + had_read_length;

                if (Ql_FatFs_ReadFile(&fp, send_cmd->Data, send_cmd->DataLength, &read_length) != 0)
                {
                    break;
                }
                if (read_length < QL_LCX9H_FW_BLOCK_SIZE)
                {
                    memset((send_cmd->Data + read_length), 0xff, QL_LCX9H_FW_BLOCK_SIZE - read_length);
                }

                send_cmd->CheckSum = Ql_LCX9H_FwChecksum(send_cmd->Data, send_cmd->DataLength);
                had_read_length += read_length;
                
                Ql_LCX9H_DataToHexString(send_cmd, 16, buffer);
                QL_LOG_D("Send FW send: %s, payload null", buffer);

                for (int j = 0; j < sizeof(Ql_LCX9H_RaceFwWrite_Typedef) - send_cmd->DataLength; j++)
                {
                    if (Ql_LCX9H_FWDL_SendAndRecvData((uint8_t*)send_cmd + j, 1, NULL, 0) != 0)
                    {
                        QL_LOG_E("%d, Send FW send no response, %d, %X", __LINE__, j, (uint8_t*)send_cmd + j);
                        func_ret = -1;
                        break;
                    }
                }

                if (Ql_LCX9H_FWDL_SendAndRecvData(&send_cmd->Data, send_cmd->DataLength, NULL, 0) != 0)
                {
                    QL_LOG_E("%d, Send FW send no response", __LINE__);
                    func_ret = -1;
                    break;
                }

                if (Ql_LCX9H_FWDL_SendAndWaitingResponse(NULL, 0, &response, sizeof(response), 500) != 0)
                {
                    QL_LOG_E("%d, Send FW recv no response", __LINE__);
                    func_ret = -1;
                    break;
                }

                Ql_LCX9H_DataToHexString(&response, sizeof(response), buffer);
                QL_LOG_D("Send FW recv:%s", buffer);

                if ((response.Status != 0) || (response.StartAddr != send_cmd->StartAddr))  // 
                {
                    func_ret = -1;
                    QL_LOG_E("Send FW recv:%s", buffer);

                    break;
                }
            
                temp_progress = (had_read_length * 100) / file_size;
                if((temp_progress > progress) && (progress < 100))
                {
                    progress += ((temp_progress - progress) / progress_step + 1) * (progress_step);
                    if(progress >= 100)
                    {
                        progress = 100;
                    }
                    QL_LOG_I("Send FW progress: %02d%%", progress);
                }
            } while (read_length >= QL_LCX9H_FW_BLOCK_SIZE);

            Ql_FatFs_CloseFile(&fp);
            file_count++;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

    } while (false);

    if(send_cmd != NULL)
    {
        vPortFree(send_cmd);
        send_cmd = NULL;
    }

    return func_ret;

}

#endif // __EXAMPLE_LCx9H_IIC_FWDL__
