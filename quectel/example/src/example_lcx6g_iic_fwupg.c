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
  Name: example_lcx6g_iic_fwupg.c
  Current support: LC76GPB
  History:
    Version  Date         Author   Description
    v1.0     2025-0103    Turner   Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_LCx6G_IIC_FWUPG__
#include "ql_application.h"
#include "ql_iic.h"
#include "ql_ff_user.h"

#include "ql_log_undef.h"
#define LOG_TAG "lcx6G"
#define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"
#include "ql_delay.h"
#ifndef uint32_t
#define uint32_t uint32_t
#endif

#define QL_LCX6H_IIC_MODE                           (IIC_MODE_SW_SIMULATE)
#define QL_LCX6H_IIC_SPEED                          (IIC_SPEED_STANDARD)
#define QL_LCX6H_IIC_TX_SIZE                        (2 * 1024U)
#define QL_LCX6H_IIC_RX_SIZE                        (2 * 1024U)

#define QL_LCX6G_FWUPG_VERSON               ("QECTEL_LCX6G_I2C_FWDL_V1.0,"__DATE__)

#define QL_LCX6G_ADDRESS                       (0x08 << 1)
#define QL_LCX6G_WRITE_ADDRESS                 (QL_LCX6G_ADDRESS & 0xFE)
#define QL_LCX6G_READ_ADDRESS                  (QL_LCX6G_ADDRESS | 0x01)

#define QL_LCX6G_SEGMENT_SIZE                  (256)

#define QL_LCX6G_FW_BLOCK_SIZE                 (0x1000)
#define QL_LCX6G_FW_FILE_PATH_MAX_LEN          (256)
#define QL_LCX6G_FW_CFG_TXT_NAME               "flash_download.cfg"

#define QL_LCX6G_I2C_READ_DATA_LEN_REG        (0x040051AA) //slave read address
#define QL_LCX6G_I2C_WRITE_DATA_LEN_REG       (0x080051AA) //slave send address

#define QL_LCx6G_IIC_DELAY                   10       
#define QL_ERASE_TIMEOUT                     (60*1000)

#define QL_FW_UPG_ADDRESS   _T("/lcx6G_fwupg/LC76GABNR12A02S_BETA0419/")
#define QL_FW_UPG_FILE_NAME _T("LC76GABNR12A02S_BETA0419.bin")

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

enum
{
    QL_LCX6G_FWUPG_MSGID_MODULE_RESP = 0x00,
    QL_LCX6G_FWUPG_MSGID_FW_INFO = 0x02,
    QL_LCX6G_FWUPG_MSGID_ERASE   = 0x03,
    QL_LCX6G_FWUPG_MSGID_FW_DATA = 0x04,
    QL_LCX6G_FWUPG_MSGID_READ_DATA = 0x11,
    QL_LCX6G_FWUPG_MSGID_FORMAT_FLASH = 0x21,
    QL_LCX6G_FWUPG_MSGID_REBOOT = 0x31,
    QL_LCX6G_FWUPG_MSGID_BL_VER = 0x71,
}Ql_LCX6G_FWUPG_Protocol_MSGID;

enum
{
    QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS = 0x00,
    QL_LCX6G_FWUPG_MODULE_STATUS_UNKNOW_ERR = 0x01,
    QL_LCX6G_FWUPG_MODULE_STATUS_CRC32_ERR = 0x02,
    QL_LCX6G_FWUPG_MODULE_STATUS_TIMEOUT_ERR = 0x03,

}Ql_LCX6G_FWUPG_Protocol_Module_Status;

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
}Ql_LCX6G_FWUPG_SendProtocol_Typedef;

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
}Ql_LCX6G_FWUPG_RespProtocol_Typedef;

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
        uint8_t BootVer[3];
    }Payload;
    uint32_t CRC32;
    uint8_t Tail;
}Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef;
#pragma pack(pop)

typedef struct
{
    uint8_t File[64];
    uint8_t FileName[32];
    uint32_t StartAddress;
    uint32_t FormatSize;
}Ql_LCX6G_FileInfo_Typedef;


static int8_t Ql_LCX6G_FWUPG_HandShakeModule(void);
static int8_t QL_LCX6G_FindFwFileInfoByFileName(const Ql_LCX6G_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX6G_FileInfo_Typedef* pFoundFileInfo);
static int8_t Ql_LCX6G_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX6G_FileInfo_Typedef *pFileInfo);

uint8_t LogBuffer[128 * 2];



const char FwFileDir[] = QL_FW_UPG_ADDRESS;

const static char* NeedUpgradeFwFileNames[] = {"MCU_FW"};
static Ql_LCX6G_FileInfo_Typedef FwFileInfos[] =
{
    {QL_FW_UPG_FILE_NAME, "MCU_FW", 0x08009000, 0x081F7000 - 0x08009000},
};

void Ql_Example_Task(void *Param)
{
    int32_t ret_code;

    QL_LOG_I("example start!");

    // init i2c
    ret_code = Ql_IIC_Init(QL_LCX6H_IIC_MODE,QL_LCX6H_IIC_SPEED,QL_LCX6H_IIC_TX_SIZE,QL_LCX6H_IIC_RX_SIZE);

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

        Ql_LCX6G_FileInfo_Typedef fw_file_info;
        
        QL_LOG_I("LCx6G module upgrade firmware start");
        do
        {
            is_success = false;
            

            QL_LOG_I("============== %s Start ============", "HandShakeModule");
            ret_code = Ql_LCX6G_FWUPG_HandShakeModule();
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
                if (QL_LCX6G_FindFwFileInfoByFileName(FwFileInfos, NeedUpgradeFwFileNames[i], &fw_file_info) != 0)
                {
                    is_upgread_file = false;
                    QL_LOG_E("Can't find %s in cfg file", NeedUpgradeFwFileNames[i]);
                    break;
                }
                ret_code = Ql_LCX6G_FWUPG_UpgradeFwFile(FwFileDir, &fw_file_info);
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

        QL_LOG_I("LCX6G module upgrade firmware finish %s", is_success ? "success" : "fail");
        break;
    }

TASK_END:
    Ql_FatFs_UnMount();
    QL_LOG_E("Current task end");
    vTaskDelete(NULL);
}

/*******************************************************************************
* Name: static int8_t QL_LCX6G_FindFwFileInfoByFileName(const Ql_LCX6G_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX6G_FileInfo_Typedef* pFoundFileInfo)
* Brief: find firmware file info by file name
* Input: 
*   pFileInfos: firmware file info array
*   pFileName: file name
* Output:
*   pFoundFileInfo: found firmware file info
* Return:
*  0: found, other: not found
*******************************************************************************/
static int8_t QL_LCX6G_FindFwFileInfoByFileName(const Ql_LCX6G_FileInfo_Typedef* pFileInfos, const uint8_t* pFileName, Ql_LCX6G_FileInfo_Typedef* pFoundFileInfo)
{
    Ql_LCX6G_FileInfo_Typedef* ptr = (Ql_LCX6G_FileInfo_Typedef*)pFileInfos;

    while((ptr != NULL) && (ptr->File[0] != 0))
    {
        if(strcmp((const char*)ptr->FileName, (const char*)pFileName) == 0)
        {
            memcpy(pFoundFileInfo, ptr, sizeof(Ql_LCX6G_FileInfo_Typedef));
            pFoundFileInfo = ptr;
            return 0;
        }
        ptr++;
    }

    return -1;
}

/*******************************************************************************
* Name: static void Ql_LCx6G_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
* Brief: lcx6G serial module iic port error handle
* Input: 
*   Param: iic status
* Output:
*   void
* Return:
*   void
*******************************************************************************/
static void Ql_LCx6G_IIC_ErrHandle(Ql_IIC_Status_TypeDef iic_state)
{
    switch(iic_state)
    {
        case QL_IIC_STATUS_OK:
            //QL_LOG_I("lcx6G module iic port r/w ok.");
            break;
        case QL_IIC_STATUS_BUSY:
            Ql_IIC_CheckBusSatus();
            Ql_IIC_DeInit();
            QL_LOG_E("lcx6G module iic port busy and re-init.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx6G_IIC_DELAY));
            Ql_IIC_Init(QL_LCX6H_IIC_MODE,QL_LCX6H_IIC_SPEED,QL_LCX6H_IIC_TX_SIZE,QL_LCX6H_IIC_RX_SIZE);
            break;
        case QL_IIC_STATUS_TIMEOUT:
            QL_LOG_E("lcx6G module iic port r/w timeout.");
            break;
        case QL_IIC_STATUS_NOACK:
            // QL_LOG_E("lcx6G module iic port r/w no ack.");
            // Ql_IIC_CheckBusSatus();
            // Ql_IIC_DeInit();
            // vTaskDelay(pdMS_TO_TICKS(QL_LCx6G_IIC_DELAY));
            // Ql_IIC_Init();
            break;
        case QL_IIC_STATUS_SEM_ERROR:
            QL_LOG_E("lcx6G module iic port semaphore error.");
            break;
        case QL_IIC_STATUS_MODE_ERROR:
            QL_LOG_E("lcx6G module iic port mode error.");
            break;
        case QL_IIC_STATUS_MEMORY_ERROR:
            QL_LOG_E("lcx6G module iic port memory error.");
            break;
        case QL_IIC_STATUS_ADDSEND_ERROR:
            QL_LOG_E("lcx6G module iic port address send error.");
            vTaskDelay(pdMS_TO_TICKS(QL_LCx6G_IIC_DELAY));
            break;
        default:
            QL_LOG_W("lcx6G module iic port unknown state:%d",iic_state);
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(QL_LCx6G_IIC_DELAY));
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FindFwFileInfo(const uint8_t* pFwFileDir, Ql_LCX6G_FileInfo_Typedef* pFileInfo, uint8_t* pCount)
* Brief: Find the firmware file information from the cfg file
* Input: 
*   Param: pFwFileDir: cfg file path
* Output:
*   Param: pFileInfo: firmware file information
*   Param: pCount: firmware file count
* Return:
*   0: success; other: failure
*******************************************************************************/
static int8_t Ql_LCX6G_FindFwFileInfo(const uint8_t* pFwFileDir, Ql_LCX6G_FileInfo_Typedef* pFileInfo, uint8_t* pCount)
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
    char cfg_file_path[QL_LCX6G_FW_FILE_PATH_MAX_LEN];
    uint32_t file_length = 4096;
    uint32_t read_length = 0;
    Ql_LCX6G_FileInfo_Typedef* p_file_info = pFileInfo;

    sprintf(cfg_file_path, "%s%s", pFwFileDir, QL_LCX6G_FW_CFG_TXT_NAME);
    
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
* Brief: lcx6G serial module iic port error handle
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
* Name: static void Ql_LCX6G_DataToHexString(const uint8_t* pInData, int Length, uint8_t* pOutData)
* Brief: Converts data to a hexadecimal string
* Input: 
*   Param: pInData: input data
*   Param: Length: data length
* Output:
*   Param: pOutData: output data
* Return:
*   None.
*******************************************************************************/
static void Ql_LCX6G_DataToHexString(const uint8_t* pInData, int Length, uint8_t* pOutData)
{
    
    memset(pOutData, 0, Length);

    for(int i = 0; i < Length; i++)
    {
        sprintf(pOutData + strlen(pOutData), "%02X ", *(pInData + i));
    }
}

/*******************************************************************************
* Name: static void Ql_LCX6G_NumberToArray(uint32_t Value, uint8_t* pData)
* Brief: Converts integer numbers to array
* Input: 
*   Param: Value: integer number
* Output:
*   Param: pData: array to store the converted data
* Return:
*   None.
*******************************************************************************/
static void Ql_LCX6G_NumberToArray(uint32_t Value, uint8_t* pData)
{
    pData[0] = (Value) & 0xFF;
    pData[1] = (Value >> (8 * 1)) & 0xFF;
    pData[2] = (Value >> (8 * 2)) & 0xFF;
    pData[3] = (Value >> (8 * 3)) & 0xFF;
}

/*******************************************************************************
* Name: static void Ql_LCX6G_NumberToArray(uint32_t Value, uint8_t* pData)
* Brief: Converts array to integer numbers (lsb first)
* Input: 
*   pData: array to store the data
*   ByteSize: size of the array
* Output:
*   Void
* Return:
*   The integer numbers.
*******************************************************************************/
static uint32_t Ql_LCX6G_ArrayToNumber(const uint8_t* pData, uint8_t ByteSize)
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
* Name: static int8_t Ql_LCX6G_FWUPG_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
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
static int8_t Ql_LCX6G_FWUPG_SendAndRecvData(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength)
{
    Ql_IIC_Status_TypeDef iic_status;

    if((pSendData != NULL) && (SendLength != 0))
    {
        uint32_t send_length = 0;
        uint32_t had_send_length = 0;
        uint8_t* ptr_offset = (uint8_t *)pSendData;
        uint8_t number = 0;
        uint8_t send_data[8] = { 0 };
        Ql_LCX6G_NumberToArray(QL_LCX6G_I2C_READ_DATA_LEN_REG, (uint8_t *)send_data);
        Ql_LCX6G_NumberToArray(SendLength, (uint8_t *)&send_data[4]);
        vTaskDelay(pdMS_TO_TICKS(10));
        iic_status = Ql_IIC_Write(QL_LCX6G_WRITE_ADDRESS, 0, send_data, 8, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx6G_IIC_ErrHandle(iic_status);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
        while (had_send_length < SendLength)
        {
            send_length = (SendLength - had_send_length) > QL_LCX6G_SEGMENT_SIZE ? QL_LCX6G_SEGMENT_SIZE : (SendLength - had_send_length);
            iic_status = Ql_IIC_Write(QL_LCX6G_WRITE_ADDRESS, 0, ptr_offset, send_length, 1000);
            if (iic_status != QL_IIC_STATUS_OK)
            {
                QL_LOG_E("data write fail %d",iic_status);
                Ql_LCx6G_IIC_ErrHandle(iic_status);
                return -1;
            }
            

            number++;
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
        Ql_LCX6G_NumberToArray(QL_LCX6G_I2C_WRITE_DATA_LEN_REG, (uint8_t *)send_data);
        iic_status = Ql_IIC_Write(QL_LCX6G_WRITE_ADDRESS, 0, send_data, 4, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx6G_IIC_ErrHandle(iic_status);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        iic_status = Ql_IIC_Read(QL_LCX6G_READ_ADDRESS, 0, (uint8_t *)recv_data, 4, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx6G_IIC_ErrHandle(iic_status);
            return -2;
        }
        can_read_length = Ql_LCX6G_ArrayToNumber(recv_data, 4);
        if(can_read_length < RecvLength)
        {
            QL_LOG_E("can read length %d < RecvLength %d", can_read_length, RecvLength);
            return -3;
        }

        // read buff
        iic_status = Ql_IIC_Read(QL_LCX6G_READ_ADDRESS, 0, (uint8_t *)pRecvData, RecvLength, 1000);
        if (iic_status != QL_IIC_STATUS_OK)
        {
            Ql_LCx6G_IIC_ErrHandle(iic_status);
            return -2;
        }
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout)
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
static int8_t Ql_LCX6G_FWUPG_SendAndWaitingResponse(const uint8_t *pSendData, uint16_t SendLength, uint8_t *pRecvData, uint16_t RecvLength, uint32_t Timeout_ms)
{
    uint32_t start_tick = Ql_GetTick();
    if(RecvLength == 0)
    {
        return -1;
    }
    while(Ql_GetTick() - start_tick < (Timeout_ms * 1000))
    {
        if(Ql_LCX6G_FWUPG_SendAndRecvData(pSendData, SendLength, pRecvData, RecvLength) != 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        return 0;
    }

    return -2;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_HandShakeModule(void)
* Brief: Shaking hands with module
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_HandShakeModule(void)
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
        Ql_LCX6G_NumberToArray(HandShakeWord1, send_data);
        if(Ql_LCX6G_FWUPG_SendAndRecvData(send_data, 4, recv_data, 4) == 0)
        {
            if (Ql_LCX6G_ArrayToNumber(recv_data, 4) == RespHandShakeWord1)
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
    
    Ql_LCX6G_NumberToArray(HandShakeWord2, send_data);
    if(Ql_LCX6G_FWUPG_SendAndWaitingResponse(send_data, 4, recv_data, 4, 2000) == 0)
    {
        if (Ql_LCX6G_ArrayToNumber(recv_data, 4) != RespHandShakeWord2)
        {
            QL_LOG_E("get handshake word2 error, 0x%04X != 0x%04X", Ql_LCX6G_ArrayToNumber(recv_data, 4), RespHandShakeWord2);
            return -2;
        }
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(Ql_LCX6G_FWUPG_SendProtocol_Typedef* pProtocol)
* Brief: Encode the protocol and send it to the module
* Input: 
*   pProtocol: protocol data buffer
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(Ql_LCX6G_FWUPG_SendProtocol_Typedef* pProtocol)
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
        Ql_LCX6G_DataToHexString(send_data, need_send_length, LogBuffer);
        QL_LOG_D("-- send data: %s", LogBuffer);
    }
    
    if(Ql_LCX6G_FWUPG_SendAndRecvData(send_data, need_send_length, NULL, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(Ql_LCX6G_FWUPG_RespProtocol_Typedef *pProtocol)
* Brief: Receive and decode the protocol data
* Input: 
*   pProtocol: protocol data buffer
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(Ql_LCX6G_FWUPG_RespProtocol_Typedef *pProtocol)
{
    uint8_t recv_data[sizeof(Ql_LCX6G_FWUPG_RespProtocol_Typedef)] = { 0 };
    uint32_t crc32 = 0;
    uint32_t check_data_size = 0;

    if(Ql_LCX6G_FWUPG_SendAndWaitingResponse(NULL, 0, recv_data, sizeof(Ql_LCX6G_FWUPG_RespProtocol_Typedef), 180) != 0)
    {
        return -1;
    }

    memcpy(pProtocol, recv_data, sizeof(Ql_LCX6G_FWUPG_RespProtocol_Typedef));

    if(pProtocol->Header != 0xAA || pProtocol->Tail != 0x55)
    {
        Ql_LCX6G_DataToHexString(recv_data, sizeof(Ql_LCX6G_FWUPG_RespProtocol_Typedef), LogBuffer);
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

static int8_t Ql_LCX6G_FWUPG_RecvAndDecodeRespBootVerProtocol(Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef *pProtocol,uint8_t * boot_ver)
{
    uint8_t recv_data[sizeof(Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef)] = { 0 };
    uint32_t crc32 = 0;
    uint32_t check_data_size = 0;

    if(Ql_LCX6G_FWUPG_SendAndWaitingResponse(NULL, 0, recv_data, sizeof(Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef), 2000) != 0)
    {
        return -1;
    }
    memcpy(pProtocol, recv_data, sizeof(Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef));

    if(pProtocol->Header != 0xAA || pProtocol->Tail != 0x55)
    {
        Ql_LCX6G_DataToHexString(recv_data, sizeof(Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef), LogBuffer);
        QL_LOG_E("-- get response: %s", LogBuffer);
        QL_LOG_E("pProtocol Header or Tail get response error");
        return -2;
    }
    pProtocol->Payload.Status = TO_LITTLE_ENDIAN_16(pProtocol->Payload.Status);
    pProtocol->PayloadLength = TO_LITTLE_ENDIAN_16(pProtocol->PayloadLength);
    QL_LOG_I("PayloadLength: %04x",pProtocol->PayloadLength);
    pProtocol->CRC32 = TO_LITTLE_ENDIAN_32(pProtocol->CRC32);
    memcpy(boot_ver,pProtocol->Payload.BootVer,sizeof(pProtocol->Payload.BootVer));

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
* Name: static int8_t Ql_LCX6G_FWUPG_SendBootVer(uint8_t *boot_ver)
* Brief: Check the Boot Ver
* Input: 
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_SendBootVer(uint8_t *boot_ver)
{
    Ql_LCX6G_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX9H_FWUPG_RespBootVerProtocol_Typedef resp_protocol;
    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;
    send_protocol.MsgID = QL_LCX6G_FWUPG_MSGID_BL_VER;
    send_protocol.PayloadLength = 0x0000;

    send_protocol.Payload = NULL;

    
    if(Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    if(Ql_LCX6G_FWUPG_RecvAndDecodeRespBootVerProtocol(&resp_protocol, boot_ver) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}
/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_SendFwInfo(uint32_t FwSize, uint32_t FwCRC32, uint32_t DestAddr)
* Brief: Send the fw address
* Input: 
*   FwSize: fw size
*   FwCRC32: fw crc32
*   DestAddr: dest address
*   Reserve: reserved
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_SendFwInfo(uint32_t FwSize, uint32_t FwCRC32, uint32_t DestAddr)
{
    Ql_LCX6G_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX6G_FWUPG_RespProtocol_Typedef resp_protocol;
    uint8_t payload[0x0010] = { 0 };

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX6G_FWUPG_MSGID_FW_INFO;
    send_protocol.PayloadLength = 0x0010;

    QL_LOG_I("send fw info: size ---- %d", FwSize);
    QL_LOG_I("send fw info: crc32 --- 0x%08X", FwCRC32);
    QL_LOG_I("send fw info: dest ---- 0x%08X", DestAddr);
    Ql_LCX6G_NumberToArray(TO_BIG_ENDIAN_32(FwSize), payload);
    Ql_LCX6G_NumberToArray(TO_BIG_ENDIAN_32(FwCRC32), payload + 4);
    Ql_LCX6G_NumberToArray(TO_BIG_ENDIAN_32(DestAddr), payload + 8);
    Ql_LCX6G_NumberToArray(TO_BIG_ENDIAN_32(0x00000000), payload + 12);

    send_protocol.Payload = payload;
    if(Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if(Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}
/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_SendEraseCommand(void)
* Brief: Send the fw upgrade command
* Input: 
*   void
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_SendEraseCommand(void)
{
    Ql_LCX6G_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX6G_FWUPG_RespProtocol_Typedef resp_protocol;

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX6G_FWUPG_MSGID_ERASE;
    send_protocol.PayloadLength = 0x0000;

    send_protocol.Payload = NULL;


    if(Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if(Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

    if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_SendFwFile(const uint8_t* pFwFilePath)
* Brief: Send the fw file
* Input: 
*   pFwFilePath: fw file path
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_SendFwFile(const uint8_t* pFwFilePath)
{
    Ql_LCX6G_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX6G_FWUPG_RespProtocol_Typedef resp_protocol;
    static uint8_t payload[QL_LCX6G_FW_BLOCK_SIZE + 4] = { 0 };
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

    send_protocol.MsgID = QL_LCX6G_FWUPG_MSGID_FW_DATA;
    

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
        Ql_LCX6G_NumberToArray(TO_BIG_ENDIAN_32(packet_number), payload);
        read_length = QL_LCX6G_FW_BLOCK_SIZE;
        if (Ql_FatFs_ReadFile(&fp, file_data_buffer_ptr, QL_LCX6G_FW_BLOCK_SIZE, &read_length) != 0)
        {
            break;
        }
        send_protocol.PayloadLength = read_length + 4;
        // if (read_length < QL_LCX6G_FW_BLOCK_SIZE)
        // {
        //     memset((file_data_buffer_ptr + read_length), 0xFF, QL_LCX6G_FW_BLOCK_SIZE - read_length);
        // }
        packet_number += 1;
        had_read_length += read_length;
        //QL_LOG_I("packet_number ----- %d",packet_number);
        if(Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
        {
            is_send_success = false;
            QL_LOG_E("send protocol error");
            break;
        }

        //vTaskDelay(pdMS_TO_TICKS(100));
        if(Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
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

        if(resp_protocol.Payload.Status != QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS)
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
    } while (read_length >= QL_LCX6G_FW_BLOCK_SIZE);

    Ql_FatFs_CloseFile(&fp);

    if(is_send_success != true)
    {
        return -2;
    }

    return 0;
}
/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_Reboot(void)
* Brief: send reboot command
* Input: 
*   NONE
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_Reboot(void)
{
    Ql_LCX6G_FWUPG_SendProtocol_Typedef send_protocol;
    Ql_LCX6G_FWUPG_RespProtocol_Typedef resp_protocol;
    uint8_t payload[2] = { 0x02,0x00 };//current reset module is 0x02

    send_protocol.Header = 0xAA;
    send_protocol.ClassID = 0x02;
    send_protocol.Tail = 0x55;

    send_protocol.MsgID = QL_LCX6G_FWUPG_MSGID_REBOOT;
    send_protocol.PayloadLength = 0x0002;
    send_protocol.Payload = payload;

    if(Ql_LCX6G_FWUPG_EnCodeAndSendProtocol(&send_protocol) != 0)
    {
        QL_LOG_E("send protocol error");
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if(Ql_LCX6G_FWUPG_RecvAndDecodeRespProtocol(&resp_protocol) != 0)
    {
        QL_LOG_E("get protocol error");
        return -2;
    }

        if(resp_protocol.Payload.MsgID != send_protocol.MsgID)
    {
        QL_LOG_E("get protocol msgid error, 0x%02X != 0x%02X", resp_protocol.Payload.MsgID, send_protocol.MsgID);
        return -3;
    }

    if(resp_protocol.Payload.Status != QL_LCX6G_FWUPG_MODULE_STATUS_SUCCESS)
    {
        QL_LOG_E("get protocol status error, 0x%04X", resp_protocol.Payload.Status);
        return -4;
    }

    return 0;
}

/*******************************************************************************
* Name: static int8_t Ql_LCX6G_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX6G_FileInfo_Typedef *pFileInfo)
* Brief: Upgrade the fw file
* Input: 
*   pFwFileDir: fw file dir
*   pFileInfo: file info
* Output:
*   void
* Return:
*   0: success; oher: fail;
*******************************************************************************/
static int8_t Ql_LCX6G_FWUPG_UpgradeFwFile(const uint8_t* pFwFileDir, const Ql_LCX6G_FileInfo_Typedef *pFileInfo)
{
    uint8_t file_path[QL_LCX6G_FW_FILE_PATH_MAX_LEN] ={ 0 };

    int32_t file_size;

    uint8_t boot_ver[3];
    uint32_t fw_crc32 = 0;
    uint8_t fw_buffer[256] = {0};
    uint32_t read_length = 0;
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

    fw_crc32 = Ql_Check_CRC32(0, &file_size, 4);


    // get crc32
    while ((Ql_FatFs_ReadFile(&fp, fw_buffer, 256, &read_length) == 0) && (read_length > 0))
    {
        fw_crc32 = Ql_Check_CRC32(fw_crc32, fw_buffer, read_length);
    }

    Ql_FatFs_CloseFile(&fp);
    QL_LOG_I("-->get boot ver");
    if(Ql_LCX6G_FWUPG_SendBootVer(boot_ver) != 0)
    {
        QL_LOG_E("send boot ver error");
        return -1;
    }
    QL_LOG_I("Boot ver: V%02X.%02X.%02X", boot_ver[0], boot_ver[1], boot_ver[2]);
    QL_LOG_I("-->send fw info");
    if(Ql_LCX6G_FWUPG_SendFwInfo(file_size, fw_crc32, pFileInfo->StartAddress) != 0)
    {
        QL_LOG_E("send fw info error");
        return -2;
    }
    
    QL_LOG_I("-->send fw Erase command");
    if(Ql_LCX6G_FWUPG_SendEraseCommand() != 0)
    {
        QL_LOG_E("send erase command error");
        return -3;
    }

    QL_LOG_I("-->send fw file");
    if(Ql_LCX6G_FWUPG_SendFwFile(file_path) != 0)
    {
        QL_LOG_E("send fw file error");
        return -4;
    }
    QL_LOG_I("-->reboot");
    if (Ql_LCX6G_FWUPG_Reboot() != 0)
    {
        QL_LOG_E("reboot error");
        return -5;
        /* code */
    }
    

    return 0;
}


#endif
