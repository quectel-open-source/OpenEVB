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
  Name: example_lcx9h_iic_fwupg.c
  Current support: LC29HAI, LC79H(AL)
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hongbin  Create file
*/

#include "example_def.h"

#ifdef __EXAMPLE_LCx9H_IIC_FWUPG__

#include "ql_application.h"
#include "ql_iic.h"
#include "ql_ff_user.h"

#include "ql_log_undef.h"
#define LOG_TAG "lcx9h"
#define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"
#include "ql_delay.h"

#ifndef uint32_t
#define uint32_t uint32_t
#endif


#define QL_LCX9H_FWUPG_VERSON               ("QECTEL_LCX9H_I2C_FWUPG_V1.0,"__DATE__)

#define QL_LCx29H_IIC_MODE                           (IIC_MODE_HW0_POLLING)
#define QL_LCx29H_IIC_SPEED                          (IIC_SPEED_FAST)
#define QL_LCx29H_IIC_TX_SIZE                        (2 * 1024U)
#define QL_LCx29H_IIC_RX_SIZE                        (2 * 1024U)

#define QL_LCX9H_ADDRESS                       (0x08 << 1)
#define QL_LCX9H_WRTITE_ADDRESS                (QL_LCX9H_ADDRESS & 0xFE)
#define QL_LCX9H_READ_ADDRESS                  (QL_LCX9H_ADDRESS | 0x01)

#define QL_LCX9H_SEGMENT_SIZE                  (256)

#define QL_LCX9H_FW_BLOCK_SIZE                 (0x1000)
#define QL_LCX9H_FW_FILE_PATH_MAX_LEN          (256)
#define QL_LCX9H_FW_CFG_TXT_NAME               "flash_download.cfg"

#define QL_LCX9H_I2C_DATA_LEN_REG        (0x080051AA)

#define TO_LITTLE_ENDIAN_16(number) \
    ((number >> 8) & 0x00FFU) | \
    ((number << 8) & 0xFF00U)

#define TO_LITTLE_ENDIAN_32(number) \
    ( ((number) >> 24) & 0x000000FFU ) | \
    ( ((number) >>  8) & 0x0000FF00U ) | \
    ( ((number) <<  8) & 0x00FF0000U ) | \
    ( ((number) << 24) & 0xFF000000U )

#define TO_BIG_ENDIAN_16(number) \
    ( ((number) >> 8) & 0x00FFU ) | \
    ( ((number) << 8) & 0xFF00U )

#define TO_BIG_ENDIAN_32(number) \
    ( ((number) >> 24) & 0x000000FFU ) | \
    ( ((number) >>  8) & 0x0000FF00U ) | \
    ( ((number) <<  8) & 0x00FF0000U ) | \
    ( ((number) << 24) & 0xFF000000U )


#define QL_LCx9H_IIC_DELAY                   10                            //unit ms

enum
{
    QL_LCX9H_FWUPG_MSGID_MODULE_RESP = 0x00,

    QL_LCX9H_FWUPG_MSGID_SEND_FW_ADDR = 0x01,
    QL_LCX9H_FWUPG_MSGID_SEND_FW_INFO = 0x02,
    QL_LCX9H_FWUPG_MSGID_SEND_UPGRADE = 0x03,
    QL_LCX9H_FWUPG_MSGID_SEND_FW_DATA = 0x04,
    QL_LCX9H_FWUPG_MSGID_FORMAT_FLASH = 0x21,
}Ql_LCX9H_FWUPG_Protocol_MSGID;

enum
{
    QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS = 0x00,
    QL_LCX9H_FWUPG_MODULE_STATUS_UNKNOW_ERR = 0x01,
    QL_LCX9H_FWUPG_MODULE_STATUS_CRC32_ERR = 0x02,
    QL_LCX9H_FWUPG_MODULE_STATUS_TIMEOUT_ERR = 0x03,

}Ql_LCX9H_FWUPG_Protocol_Module_Status;

#pragma pack(push, 1)  // Unalign bytes

typedef struct 
{
    uint8_t Header;
    uint8_t ClassID;
    uint8_t MsgID;
    uint16_t PayloadLength;     // big-endian format
    uint8_t* Payload;
    uint32_t CRC32;             // big-endian format
    uint8_t Tail;
}Ql_LCX9H_FWUPG_SendProtocol_Typedef;

typedef struct 
{
    uint8_t Header;
    uint8_t ClassID;
    uint8_t MsgID;
    uint16_t PayloadLength;
    struct {
        uint8_t ClassID;
        uint8_t MsgID;
        uint16_t Status;
    }Payload;
    uint32_t CRC32;
    uint8_t Tail;
}Ql_LCX9H_FWUPG_RespProtocol_Typedef;

typedef struct 
{
    uint8_t Header;
    uint8_t ClassID;
    uint8_t MsgID;
    uint16_t PayloadLength;
    struct {
        uint8_t ClassID;
        uint8_t MsgID;
        uint8_t Progress;
        uint16_t Status;
    }Payload;
    uint32_t CRC32;
    uint8_t Tail;
}Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef;

#pragma pack(pop)

typedef struct
{
    uint8_t File[64];
    uint8_t FileName[32];
    uint32_t StartAddress;
    uint32_t FormatSize;
}Ql_LCX9H_FileInfo_Typedef;

static int8_t QL_LCX9H_FindFwFileInfoByFileName(const Ql_LCX9H_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX9H_FileInfo_Typedef* pFoundFileInfo);
static int8_t Ql_LCX9H_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX9H_FileInfo_Typedef *pFileInfo);

static int8_t Ql_LCX9H_FWUPG_HandShakeModule(void);
static int8_t Ql_LCX9H_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX9H_FileInfo_Typedef *pFileInfo);

uint8_t LogBuffer[128 * 2];
const char FwFileDir[] = "/lcx9h_fwupg/LC79HALNR11A04SV04/";

const static char* NeedUpgradeFwFileNames[] = {"PartitionTable", "MCU_FW", "GNSS_CFG"};

static Ql_LCX9H_FileInfo_Typedef FwFileInfos[] =
{
    {"partition_table.bin", "PartitionTable", 0x08000000, QL_LCX9H_FW_BLOCK_SIZE},
    {"LC79HALNR11A04S.bin", "MCU_FW", 0x08013000, 0x083DF000 - 0x08013000},
    {"gnss_config.bin", "GNSS_CFG", 0x083DF000, QL_LCX9H_FW_BLOCK_SIZE},
};

void Ql_Example_Task(void *Param)
{
    int32_t ret_code;

    QL_LOG_I("example start!");

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
        bool is_success = false;
        bool is_upgread_file = false;

        Ql_LCX9H_FileInfo_Typedef fw_file_info;
        
        QL_LOG_I("LX29H module upgrade firmware start");
        do
        {
            is_success = false;

#if 0    // From cfg file read the file info
            QL_LOG_I("Read cfg file");
            memset(&FwFileInfos, 0, sizeof(FwFileInfos));
            ret_code = Ql_LCX9H_FindFwFileInfo(FwFileDir, &FwFileInfos, &file_info_size);
            if (ret_code != 0)
            {
                QL_LOG_E("Read cfg file error code %d", ret_code);
                break;
            }
#endif 
            QL_LOG_I("============== %s Start ============", "HandShakeModule");
            ret_code = Ql_LCX9H_FWUPG_HandShakeModule();
            if (ret_code != 0)
            {
                QL_LOG_E("%s Fail, ret_code = %d", "HandShakeModule", ret_code);
                break;
            }
            QL_LOG_I("==============%s End, %s============", "HandShakeModule", "Success");

            is_upgread_file = true;
            for (size_t i = 0; i < sizeof(NeedUpgradeFwFileNames) / sizeof(NeedUpgradeFwFileNames[0]); i++)
            {
                QL_LOG_I("============== %s Start File %s ============", "UpgradeFwFile", NeedUpgradeFwFileNames[i]);
                if (QL_LCX9H_FindFwFileInfoByFileName(FwFileInfos, NeedUpgradeFwFileNames[i], &fw_file_info) != 0)
                {
                    is_upgread_file = false;
                    QL_LOG_E("Can't find %s in cfg file", NeedUpgradeFwFileNames[i]);
                    break;
                }
                ret_code = Ql_LCX9H_FWUPG_UpgradeFwFile(FwFileDir, &fw_file_info);
                if (ret_code != 0)
                {
                    is_upgread_file = false;
                    QL_LOG_E("send %s Fail, ret_code = %d", NeedUpgradeFwFileNames[i], ret_code);
                    break;
                }
                QL_LOG_I("============== %s End File %s, %s ============", "UpgradeFwFile", NeedUpgradeFwFileNames[i], "Success");
            }

            if(is_upgread_file != true)
            {
                break;
            }

            is_success = true;
        } while (false);

        QL_LOG_I("LCX9H module upgrade firmware finish %s", is_success ? "success" : "fail");
        break;
    }

TASK_END:
    Ql_FatFs_UnMount();
    QL_LOG_E("Current task end");
    vTaskDelete(NULL);
}

/*******************************************************************************
* Name: static int8_t QL_LCX9H_FindFwFileInfoByFileName(const Ql_LCX9H_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX9H_FileInfo_Typedef* pFoundFileInfo)
* Brief: find firmware file info by file name
* Input: 
*   pFileInfos: firmware file info array
*   pFileName: file name
* Output:
*   pFoundFileInfo: found firmware file info
* Return:
*  0: found, other: not found
*******************************************************************************/
static int8_t QL_LCX9H_FindFwFileInfoByFileName(const Ql_LCX9H_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX9H_FileInfo_Typedef* pFoundFileInfo)
{
    Ql_LCX9H_FileInfo_Typedef* ptr = (Ql_LCX9H_FileInfo_Typedef*)pFileInfos;

    while((ptr != NULL) && (ptr->File[0] != 0))
    {
        if(strcmp((const char*)ptr->FileName, (const char*)pFileName) == 0)
        {
            memcpy(pFoundFileInfo, ptr, sizeof(Ql_LCX9H_FileInfo_Typedef));
            pFoundFileInfo = ptr;
            return 0;
        }
        ptr++;
    }

    return -1;
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
    vPortFree(ptr_buffer);


    return func_ret;
}

/*******************************************************************************
* Name: static uint32_t Ql_Check_CRC32(uint32_t InitVal, const unsigned char *pData, const uint32_t Length)
* Brief: lcx9h serial module iic port error handle
* Input: 
*   InitVal : initial value
*   pData : data buffer
*   Length : data length
* Output:
*   void
* Return:
*   CRC32 value
*******************************************************************************/
static uint32_t Ql_Check_CRC32(uint32_t InitVal, const unsigned char *pData, const uint32_t Length)
{
    const uint32_t Table_CRC32[256] = 
    {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
        0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
        0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
        0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
        0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
        0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
        0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
        0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
        0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
        0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
        0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
        0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
        0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
        0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
        0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
        0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
        0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
        0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
        0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
        0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
        0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
        0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
        0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
        0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
        0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
        0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
        0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
        0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
        0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
        0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
        0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
        0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
        0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
        0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
        0x2d02ef8d
    };

    uint32_t i = 0;
    uint32_t CRC_Result = InitVal ^ 0xFFFFFFFF;

    if((NULL == pData) || (Length < 1))
    {
        return (CRC_Result ^ 0xFFFFFFFF);
    }

    for (i = 0; i < Length; i++)
    {
        CRC_Result = Table_CRC32[(CRC_Result ^ *(pData + i)) & 0xFF] ^ (CRC_Result >> 8);
    }

    return (CRC_Result ^ 0xFFFFFFFF);
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
        sprintf(pOutData + strlen(pOutData), "%02X ", *(pInData + i));
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
}

/*******************************************************************************
* Name: static void Ql_LCX9H_NumberToArray(uint32_t Value, uint8_t* pData)
* Brief: Converts array to integer numbers (lsb first)
* Input: 
*   pData: array to store the data
*   ByteSize: size of the array
* Output:
*   Void
* Return:
*   The integer numbers.
*******************************************************************************/
static uint32_t Ql_LCX9H_ArrayToNumber(const uint8_t* pData, uint8_t ByteSize)
{
    uint32_t value = 0;
    
    if(ByteSize > 4)
    {
        return 0;
    }

    for(int i = 0; i < ByteSize; i++)
    {
        value |= *(pData + i) << (8 * i);
    }

    return value;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
* Brief: Sends and receives data
* Input: 
*   pSendData: pointer to the data to be sent;
*   SendLength: length of the data to be sent;
*   RecvLength: length of the data to be received;
* Output:
*   pRecvData: pointer to the data received;
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
{
    Ql_IIC_Status_TypeDef iic_status;

    if((pSendData != NULL) && (SendLength != 0))
    {
        uint32_t send_length = 0;
        uint32_t had_send_length = 0;
        uint8_t* ptr_offset = (uint8_t *)pSendData;
        
        uint8_t number = 0;

        while (had_send_length < SendLength)
        {
            send_length = (SendLength - had_send_length) > QL_LCX9H_SEGMENT_SIZE ? QL_LCX9H_SEGMENT_SIZE : (SendLength - had_send_length);
            iic_status = Ql_IIC_Write(QL_LCX9H_WRTITE_ADDRESS, 0, ptr_offset, send_length, 1000);
            if (iic_status != QL_IIC_STATUS_OK)
            {
                Ql_LCx9H_IIC_ErrHandle(iic_status);
                return -1;
            }

            number++;
            //QL_LOG_I("all length 0x%04X, cur send 0x%04X, NO.%d", SendLength, send_length, number);

            had_send_length += send_length;
            ptr_offset = (uint8_t*)ptr_offset + send_length;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if((pRecvData != NULL) && (RecvLength != 0))
    {
        uint8_t send_data[4] = { 0 };
        uint8_t recv_data[4] = { 0 };
        uint32_t can_read_length = 0;

        vTaskDelay(pdMS_TO_TICKS(10));

        // read buff length
        Ql_LCX9H_NumberToArray(QL_LCX9H_I2C_DATA_LEN_REG, (uint8_t *)send_data);
        iic_status = Ql_IIC_Write(QL_LCX9H_WRTITE_ADDRESS, 0, send_data, 4, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx9H_IIC_ErrHandle(iic_status);
            return -1;
        }

        iic_status = Ql_IIC_Read(QL_LCX9H_READ_ADDRESS, 0, (uint8_t *)recv_data, 4, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx9H_IIC_ErrHandle(iic_status);
            return -2;
        }
        can_read_length = Ql_LCX9H_ArrayToNumber(recv_data, 4);
        if(can_read_length < RecvLength)
        {
            QL_LOG_E("can read length %d < RecvLength %d", can_read_length, RecvLength);
            return -3;
        }

        // read buff
        iic_status = Ql_IIC_Read(QL_LCX9H_READ_ADDRESS, 0, (uint8_t *)pRecvData, RecvLength, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx9H_IIC_ErrHandle(iic_status);
            return -2;
        }
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout)
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
static int8_t Ql_LCX9H_FWUPG_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout)
{

    if(RecvLength == 0)
    {
        return -1;
    }

    for(int i = 0; i < (Timeout / 10); i++)
    {
        if(Ql_LCX9H_FWUPG_SendAndRecvData(pSendData, SendLength, pRecvData, RecvLength) != 0)
        {
            continue;
        }

        return 0;
    }

    return -2;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_HandShakeModule(void)
* Brief: Shaking hands with module
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_HandShakeModule(void)
{
    uint16_t waiting_count =0;
    bool is_hand_shake = false;

    uint8_t send_data[8] = { 0 };
    uint8_t recv_data[8] ={ 0 };

    const uint32_t HandShakeWord1 = 0x514C1309;
    const uint32_t RespHandShakeWord1 = 0xAAFC3A4D;
    const uint32_t HandShakeWord2 = 0x1203A504;
    const uint32_t RespHandShakeWord2 = 0x55FD5BA0;

    // waiting for hand shake
    while (1)
    {
        Ql_LCX9H_NumberToArray(HandShakeWord1, send_data);
        if(Ql_LCX9H_FWUPG_SendAndRecvData(send_data, 4, recv_data, 4) == 0)
        {
            if (Ql_LCX9H_ArrayToNumber(recv_data, 4) == RespHandShakeWord1)
            {
                QL_LOG_D("get handshake word1...");
                is_hand_shake = true;
                break;
            }
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

    if(is_hand_shake != true)
    {
        return -1;
    }
    
    Ql_LCX9H_NumberToArray(HandShakeWord2, send_data);
    if(Ql_LCX9H_FWUPG_SendAndWaitingResponse(send_data, 4, recv_data, 4, 2000) == 0)
    {
        if (Ql_LCX9H_ArrayToNumber(recv_data, 4) != RespHandShakeWord2)
        {
            QL_LOG_E("get handshake word2 error, 0x%04X != 0x%04X", Ql_LCX9H_ArrayToNumber(recv_data, 4), RespHandShakeWord2);
            return -2;
        }
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(Ql_LCX9H_FWUPG_SendProtocol_Typedef* pProtocol)
* Brief: Encode the protocol and send it to the module
* Input: 
*   pProtocol: protocol data buffer
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(Ql_LCX9H_FWUPG_SendProtocol_Typedef* pProtocol)
{
    static uint8_t send_data[4096 + 14] = { 0 };
    uint32_t need_send_length = 0;
    uint32_t temp_number;
    uint32_t check_data_size = 0;

    send_data[need_send_length] = pProtocol->Header;
    need_send_length += sizeof(pProtocol->Header);

    send_data[need_send_length] = pProtocol->ClassID;
    need_send_length += sizeof(pProtocol->ClassID);

    send_data[need_send_length] = pProtocol->MsgID;
    need_send_length += sizeof(pProtocol->MsgID);

    temp_number = TO_BIG_ENDIAN_16(pProtocol->PayloadLength);
    memcpy(send_data + need_send_length, &temp_number, sizeof(pProtocol->PayloadLength));
    need_send_length += sizeof(pProtocol->PayloadLength);

    memcpy(send_data + need_send_length, pProtocol->Payload, pProtocol->PayloadLength);
    need_send_length += pProtocol->PayloadLength;

    check_data_size = sizeof(pProtocol->ClassID) + sizeof(pProtocol->MsgID) + sizeof(pProtocol->PayloadLength) + pProtocol->PayloadLength;
    pProtocol->CRC32 = Ql_Check_CRC32(0, send_data + sizeof(pProtocol->Header), check_data_size);

    temp_number = TO_BIG_ENDIAN_32(pProtocol->CRC32);
    memcpy(send_data + need_send_length, &temp_number, sizeof(pProtocol->CRC32));
    need_send_length += sizeof(pProtocol->CRC32);

    memcpy(send_data + need_send_length, &pProtocol->Tail, sizeof(pProtocol->Tail));
    need_send_length += sizeof(pProtocol->Tail);

    if(need_send_length < 64)
    {
        Ql_LCX9H_DataToHexString(send_data, need_send_length, LogBuffer);
        QL_LOG_D("-- send data: %s", LogBuffer);
    }

    if(Ql_LCX9H_FWUPG_SendAndRecvData(send_data, need_send_length, NULL, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_RecvAndDecodeRespFormatProtocol(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef *pProtocol)
* Brief: Receive and decode the format protocol data
* Input: 
*   pProtocol: protocol data buffer
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_RecvAndDecodeRespFormatProtocol(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef *pProtocol)
{
    uint8_t recv_data[sizeof(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef)] = {0};
    uint32_t crc32 = 0;
    uint32_t check_data_size = 0;

    if (Ql_LCX9H_FWUPG_SendAndWaitingResponse(NULL, 0, recv_data, sizeof(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef), 2000) != 0)
    {
        return -1;
    }
    memcpy(pProtocol, recv_data, sizeof(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef));

    if (pProtocol->Header != 0xAA || pProtocol->Tail != 0x55)
    {
        Ql_LCX9H_DataToHexString(recv_data, sizeof(Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef), LogBuffer);
        QL_LOG_E("-- get response: %s", LogBuffer);
        QL_LOG_E("pProtocol Header or Tail get response error");
        return -2;
    }

    pProtocol->Payload.Status = TO_LITTLE_ENDIAN_16(pProtocol->Payload.Status);
    pProtocol->PayloadLength = TO_LITTLE_ENDIAN_16(pProtocol->PayloadLength);
    pProtocol->CRC32 = TO_LITTLE_ENDIAN_32(pProtocol->CRC32);

    check_data_size = sizeof(pProtocol->ClassID) + sizeof(pProtocol->MsgID) + sizeof(pProtocol->PayloadLength) + pProtocol->PayloadLength;
    crc32 = Ql_Check_CRC32(0, recv_data + sizeof(pProtocol->Header), check_data_size);

    if (crc32 != pProtocol->CRC32)
    {
        QL_LOG_E("pProtocol CRC32 response error calc 0x%08X != recv 0x%08X", crc32, pProtocol->CRC32);

        return -3;
    }


    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(Ql_LCX9H_FWUPG_RespProtocol_Typedef *pProtocol)
* Brief: Receive and decode the protocol data
* Input: 
*   pProtocol: protocol data buffer
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(Ql_LCX9H_FWUPG_RespProtocol_Typedef *pProtocol)
{
    uint8_t recv_data[sizeof(Ql_LCX9H_FWUPG_RespProtocol_Typedef)] = { 0 };
    uint32_t crc32 = 0;
    uint32_t check_data_size = 0;

    if(Ql_LCX9H_FWUPG_SendAndWaitingResponse(NULL, 0, recv_data, sizeof(Ql_LCX9H_FWUPG_RespProtocol_Typedef), 2000) != 0)
    {
        return -1;
    }

    memcpy(pProtocol, recv_data, sizeof(Ql_LCX9H_FWUPG_RespProtocol_Typedef));

    if(pProtocol->Header != 0xAA || pProtocol->Tail != 0x55)
    {
        Ql_LCX9H_DataToHexString(recv_data, sizeof(Ql_LCX9H_FWUPG_RespProtocol_Typedef), LogBuffer);
        QL_LOG_E("-- get response: %s", LogBuffer);
        QL_LOG_E("pProtocol Header or Tail get response error");
        return -2;
    }

    pProtocol->Payload.Status = TO_LITTLE_ENDIAN_16(pProtocol->Payload.Status);
    pProtocol->PayloadLength = TO_LITTLE_ENDIAN_16(pProtocol->PayloadLength);
    pProtocol->CRC32 = TO_LITTLE_ENDIAN_32(pProtocol->CRC32);

    check_data_size = sizeof(pProtocol->ClassID) + sizeof(pProtocol->MsgID) + sizeof(pProtocol->PayloadLength) + pProtocol->PayloadLength;
    crc32 = Ql_Check_CRC32(0, recv_data + sizeof(pProtocol->Header), check_data_size);

    if(crc32 != pProtocol->CRC32)
    {
        QL_LOG_E("pProtocol CRC32 response error calc 0x%08X != recv 0x%08X", crc32, pProtocol->CRC32);
        
        return -3;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendFwAddr(uint32_t FwAddr)
* Brief: send the fw address
* Input: 
*   FwAddr: fw address
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_SendFwAddr(uint32_t FwAddr)
{
    Ql_LCX9H_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespProtocol_Typedef resp_protocol;
    uint8_t payload[4] = { 0 };

    QL_LOG_I("send fw addr: 0x%08X", FwAddr);

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX9H_FWUPG_MSGID_SEND_FW_ADDR;
    send_protocol.PayloadLength = 0x0004;

    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(FwAddr), payload);
    send_protocol.Payload = payload;

    if(Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    if(Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendFwInfo(uint32_t FwSize, uint32_t FwCRC32, uint32_t DestAddr, uint32_t EraseFlag)
* Brief: Send the fw address
* Input: 
*   FwSize: fw size
*   FwCRC32: fw crc32
*   DestAddr: dest address
*   EraseFlag: erase flag
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_SendFwInfo(uint32_t FwSize, uint32_t FwCRC32, uint32_t DestAddr, uint32_t EraseFlag)
{
    Ql_LCX9H_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespProtocol_Typedef resp_protocol;
    uint8_t payload[0x0010] = { 0 };

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX9H_FWUPG_MSGID_SEND_FW_INFO;
    send_protocol.PayloadLength = 0x0010;

    QL_LOG_I("send fw info: size 0x%08X", FwSize);
    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(FwSize), payload);
    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(FwCRC32), payload + 4);
    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(DestAddr), payload + 8);

    memcpy(payload + 12, &EraseFlag, 1);

    send_protocol.Payload = payload;

    if(Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if(Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendFwUpgradeCommand(void)
* Brief: Send the fw upgrade command
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_SendFwUpgradeCommand(void)
{
    Ql_LCX9H_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespProtocol_Typedef resp_protocol;

     send_protocol.Header = 0xAA;
   send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX9H_FWUPG_MSGID_SEND_UPGRADE;
    send_protocol.PayloadLength = 0x0000;

    send_protocol.Payload = NULL;


    if(Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if(Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_FormatFlash(uint32_t StartAddr, uint32_t FormatLength)
* Brief: Format the flash
* Input: 
*   StartAddr: start address
*   FormatLength: format length
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_FormatFlash(uint32_t StartAddr, uint32_t FormatLength)
{
    Ql_LCX9H_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespFormatProtocol_Typedef resp_format_protocol;
    Ql_LCX9H_FWUPG_RespProtocol_Typedef resp_protocol;

    uint8_t payload[8] = { 0 };

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX9H_FWUPG_MSGID_FORMAT_FLASH;
    send_protocol.PayloadLength = 0x0008;

    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(StartAddr), payload);
    Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(FormatLength), payload + 4);

    send_protocol.Payload = payload;

    if (Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    do
    {
        if(Ql_LCX9H_FWUPG_RecvAndDecodeRespFormatProtocol(&resp_format_protocol) != 0)
        {
            return -2;
        }

        if (resp_format_protocol.Payload.MsgID != QL_LCX9H_FWUPG_MSGID_FORMAT_FLASH)
        {
            QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_format_protocol.Payload.MsgID, send_protocol.MsgID);
            return -4;
        }

        if (resp_format_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
        {
            QL_LOG_E("get protocol status error, 0x%04X", resp_format_protocol.Payload.Status);
            return -5;
        }

        QL_LOG_I("format flash progress %d%%", resp_format_protocol.Payload.Progress);

        send_protocol.PayloadLength = 0x0005;
        payload[0] = 0;
        payload[1] = 0;
        payload[2] = resp_format_protocol.Payload.Progress;
        memcpy(payload + 3, &resp_format_protocol.Payload.Status, 2);

        if (Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
        {
            QL_LOG_E("send protocol error");
            return -3;
        }
    } while(resp_format_protocol.Payload.Progress < 100);

    if(Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -4;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -5;
    }

    if(resp_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -6;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_SendFwFile(const uint8_t* pFwFilePath)
* Brief: Send the fw file
* Input: 
*   pFwFilePath: fw file path
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_SendFwFile(const uint8_t* pFwFilePath)
{
    Ql_LCX9H_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespProtocol_Typedef resp_protocol;
    static uint8_t payload[QL_LCX9H_FW_BLOCK_SIZE + 4] = { 0 };
    uint32_t packet_number = 0;

    uint8_t* file_data_buffer_ptr = NULL;
    bool is_send_success = true;

    FIL fp;
    uint32_t file_size = 0;
    uint32_t read_length = 0;
    uint32_t had_read_length = 0;
    uint8_t progress = 0;
    uint8_t temp_progress;
    uint8_t progress_step = 10;

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX9H_FWUPG_MSGID_SEND_FW_DATA;
    send_protocol.PayloadLength = QL_LCX9H_FW_BLOCK_SIZE + 4;

    file_data_buffer_ptr = (uint8_t*)(payload + 4);
    send_protocol.Payload = payload;


    QL_LOG_I("open file path: %s", pFwFilePath);
    if(Ql_FatFs_OpenFile(pFwFilePath, &fp, QL_FILE_READ) != 0)
    {
        QL_LOG_E("open file error, path: %s", pFwFilePath);
        return -1;
    }
    //read the file
    file_size = Ql_FatFs_ReadFileSize(&fp);

    progress = 0;
    had_read_length = 0;

    packet_number = 0;
    do
    {
        Ql_LCX9H_NumberToArray(TO_BIG_ENDIAN_32(packet_number), payload);
        read_length = QL_LCX9H_FW_BLOCK_SIZE;
        if (Ql_FatFs_ReadFile(&fp, file_data_buffer_ptr, QL_LCX9H_FW_BLOCK_SIZE, &read_length) != 0)
        {
            break;
        }

        if (read_length < QL_LCX9H_FW_BLOCK_SIZE)
        {
            memset((file_data_buffer_ptr + read_length), 0xFF, QL_LCX9H_FW_BLOCK_SIZE - read_length);
        }
        packet_number += 1;
        had_read_length += read_length;

        if(Ql_LCX9H_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
        {
            is_send_success = false;
            QL_LOG_E("send protocol error");
            break;
        }

        if(Ql_LCX9H_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
        {
            is_send_success = false;
            QL_LOG_E("get protocol error");
            break;
        }

        if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
        {
            is_send_success = false;
            QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
            break;
        }

        if(resp_protocol.Payload.Status != QL_LCX9H_FWUPG_MODULE_STATUS_SUCCESS)
        {
            is_send_success = false;
            QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
            break;
        }

        temp_progress = (had_read_length * 100) / file_size;
        if ((temp_progress > progress) && (progress < 100))
        {
            progress += ((temp_progress - progress) / progress_step + 1) * (progress_step);
            if (progress >= 100)
            {
                progress = 100;
            }
            QL_LOG_I("Send FW progress: %02d%%", progress);
        }
    } while (read_length >= QL_LCX9H_FW_BLOCK_SIZE);

    Ql_FatFs_CloseFile(&fp);

    if(is_send_success != true)
    {
        return -2;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX9H_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX9H_FileInfo_Typedef *pFileInfo)
* Brief: Upgrade the fw file
* Input: 
*   pFwFileDir: fw file dir
*   pFileInfo: file info
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX9H_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX9H_FileInfo_Typedef *pFileInfo)
{
    uint8_t file_path[QL_LCX9H_FW_FILE_PATH_MAX_LEN] ={ 0 };

    int32_t file_size;

    uint32_t send_fw_size = 0;
    uint32_t need_add_size = 0;
    uint32_t fw_crc32 = 0;
    uint8_t fw_buffer[256] = {0};
    uint32_t read_length = 0;
    uint32_t format_size = 0;
    FIL fp;


    sprintf(file_path, "%s%s", pFwFileDir, pFileInfo->File);

    if(Ql_FatFs_OpenFile(file_path, &fp, QL_FILE_READ) != 0)
    {
        QL_LOG_E("open file error, path: %s", file_path);
        return -1;
    }

    file_size = Ql_FatFs_ReadFileSize(&fp);

    if(file_size < 0)
    {
        QL_LOG_E("file size error, resp: %s", file_size);
    }

    send_fw_size = (uint32_t)(file_size / QL_LCX9H_FW_BLOCK_SIZE) * QL_LCX9H_FW_BLOCK_SIZE;
    if(file_size % QL_LCX9H_FW_BLOCK_SIZE > 0)
    {
        send_fw_size += QL_LCX9H_FW_BLOCK_SIZE;
    }

    fw_crc32 = Ql_Check_CRC32(0, &send_fw_size, 4);

    need_add_size = send_fw_size - file_size;
    // get crc32
    while ((Ql_FatFs_ReadFile(&fp, fw_buffer, 256, &read_length) == 0) && (read_length > 0))
    {
        fw_crc32 = Ql_Check_CRC32(fw_crc32, fw_buffer, read_length);
    }

    Ql_FatFs_CloseFile(&fp);

    if(need_add_size > 0)
    {
        memset(fw_buffer, 0xFF, 256);
        
        while(need_add_size > 0)
        {
            uint32_t calc_length = need_add_size > 256 ? 256 : need_add_size;

            fw_crc32 = Ql_Check_CRC32(fw_crc32, fw_buffer, calc_length);

            need_add_size = need_add_size - calc_length;
        }
    
    }

    QL_LOG_I("-->send fw addr");
    if(Ql_LCX9H_FWUPG_SendFwAddr(pFileInfo->StartAddress) != 0)
    {
        QL_LOG_E("send fw addr error");
        return -1;
    }

    QL_LOG_I("-->send fw info");
    if(Ql_LCX9H_FWUPG_SendFwInfo(send_fw_size, fw_crc32, pFileInfo->StartAddress, 0x01) != 0)
    {
        QL_LOG_E("send fw info error");
        return -2;
    }
    
    QL_LOG_I("-->send fw upgrade command");
    if(Ql_LCX9H_FWUPG_SendFwUpgradeCommand() != 0)
    {
        QL_LOG_E("send fw upgrade command error");
        return -3;
    }

    format_size = pFileInfo->FormatSize < QL_LCX9H_FW_BLOCK_SIZE ? send_fw_size : pFileInfo->FormatSize;
    
    QL_LOG_I("-->format flash");
    if(Ql_LCX9H_FWUPG_FormatFlash(pFileInfo->StartAddress, format_size) != 0)
    {
        QL_LOG_E("format flash error");
        return -4;
    }

    QL_LOG_I("-->send fw file");
    if(Ql_LCX9H_FWUPG_SendFwFile(file_path) != 0)
    {
        QL_LOG_E("send fw file error");
        return -5;
    }

    return 0;
}
#endif // __EXAMPLE_LCx9H_IIC_FWUPG__
