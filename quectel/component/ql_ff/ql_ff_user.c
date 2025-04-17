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
  Name: ql_ff_user.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "ql_ff_user.h"
#include "FreeRTOS.h"
#include "task.h"

#define LOG_TAG "ql_ff_user"
#define LOG_LVL QL_LOG_DEBUG
#include "ql_log.h"

FATFS *qlfs = NULL;

//static FRESULT delete_old_files(const char *path);
//static void delete_no_time_files(const char *path);

int32_t Ql_FatFs_Mount(void)
{
    FATFS *fs;
    char label[16];
    FRESULT Res;
    
    if (qlfs != NULL)
    {
        return 0;
    }
    
    fs = (FATFS *)pvPortMalloc(sizeof(FATFS));
    if (fs == NULL)
    {
        // QL_LOG_W("malloc fail");
        return -1;
    }
    
    Res = f_mount(fs, "1:", 1);
    if (Res == FR_NO_FILESYSTEM)
    {
        #if 0
        const MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        BYTE *work = NULL;
        
        work = (BYTE *)pvPortMalloc(FF_MAX_SS);
        if (work == NULL)
        {
            QL_LOG_W("malloc fail");
            vPortFree(fs);
            return -1;
        }
        
        Res = f_mkfs("1:", &opt, work, FF_MAX_SS);
        if (Res != FR_OK)
        {
            FsRes = Res;
            vPortFree(work);
            vPortFree(fs);
            QL_LOG_W("f_mkfs fail, %d", Res);
            return -1;
        }
        
        Res = f_mount(fs, "1:", 1);
        vPortFree(work);
        #endif
    }
    
    if (Res != FR_OK)
    {
        vPortFree(fs);
        return -1;
    }
    
    Res = f_getlabel("1:", label, NULL);
    if (Res == FR_OK && strcmp("QUECTEL", label) != 0)
    {
        f_setlabel("1:QUECTEL");
    }
    
    qlfs = fs;
    
    return 0;
}

int32_t Ql_FatFs_UnMount(void)
{
    FRESULT Res;
    
    if (qlfs == NULL)
    {
        return 0;
    }
    
    Res = f_mount(NULL, "1:", 0);
    if (Res != FR_OK)
    {
        return -1;
    }
    
    vPortFree(qlfs);
    qlfs = NULL;
    
    return 0;
}

int32_t Ql_FatFs_Mount_State(void)
{
    if (qlfs == NULL)
    {
        return -1;
    }
    return 0;
}

static int32_t Ql_FatFs_Write_Internal(const char *path, const uint8_t *str, uint32_t len, uint32_t *file_size)
{
    FIL *fp = NULL;
    UINT ptr;
    char *folder;
    char *p = (char *)path;
    
    FRESULT Res;
    
    if ((qlfs == NULL) || (path == NULL) || (str == NULL) || (len == 0))
    {
        return -1;
    }
    
    folder = pvPortMalloc(strlen(path) + 1);
    if (folder == NULL)
    {
        QL_LOG_E("Ql_FatFs_Write, folder malloc fail. path: %s", path);
        return -1;
    }
    memset(folder, 0, strlen(path) + 1);
    
    // Check the remaining SD card size
    check_sd_remain_size();
    
    // Create a folder if it does not exist
    for ( ; ; )
    {
        p = strstr(p, "/");
        if (p == NULL)
        {
            break;
        }
        memcpy(folder, path, p - path);
        Res = f_mkdir(folder);
        if ((Res == FR_OK) || (Res == FR_EXIST))
        {
            p++;
        }
        else
        {
            QL_LOG_E("f_mkdir fail, %d. path: %s", Res, path);
            vPortFree(folder);
            return -1;
        }
    }
    
    // append
    fp = (FIL *)pvPortMalloc(sizeof(FIL));
    if (fp == NULL)
    {
        QL_LOG_E("Ql_FatFs_Write, fp malloc fail. path: %s", path);
        vPortFree(folder);
        return -1;
    }
    
    memset(fp, 0, sizeof(FIL));
    Res = f_open(fp, path, FA_OPEN_APPEND | FA_WRITE);
    if (Res != FR_OK)
    {
        QL_LOG_E("f_open fail, %d. path: %s", Res, path);
        vPortFree(fp);
        vPortFree(folder);
        return -1;
    }
    
    Res = f_write(fp, str, len, &ptr);
    if (Res != FR_OK)
    {
        QL_LOG_E("f_write fail, %d. path: %s", Res, path);
    }
    
    if (file_size != NULL)
    {
        *file_size = f_size(fp);
    }
    
    Res = f_close(fp);
    if (Res != FR_OK)
    {
        QL_LOG_E("f_close fail, %d. path: %s", Res, path);
        vPortFree(fp);
        vPortFree(folder);
        return -1;
    }
    
    vPortFree(fp);
    vPortFree(folder);
    
    return 0;
}

int32_t Ql_FatFs_Write(const char *path, const uint8_t *str, uint32_t len,  uint32_t *file_size)
{
    int32_t Res;
    
    // Ql_Led_SdStateInd(true);
    
    Res = Ql_FatFs_Write_Internal(path, str, len, file_size);
    
    // Ql_Led_SdStateInd(false);
    
    return Res;
}

uint32_t Ql_FatFs_GetTotal(void)
{
    DWORD tot_size;
    
    if (qlfs == NULL)
    {
        return 0;
    }
    
    tot_size = (qlfs->n_fatent - 2) * qlfs->csize / 2048;  // Mbyte
    
    return tot_size;
}

uint32_t Ql_FatFs_GetFree(void)
{
    DWORD fre_clust;
    DWORD fre_size;
    FRESULT Res;
    
    if (qlfs == NULL)
    {
        return 0;
    }
    
    Res = f_getfree( "1:", &fre_clust, &qlfs );
    if (Res != FR_OK)
    {
        return 0;
    }
    
    fre_size = fre_clust * qlfs->csize / 2048;  // Mbyte
    
    return fre_size;
}

char* Ql_FatFs_Size(void)
{
    DWORD tot_size = 0;
    DWORD fre_size = 0;
    static char buf[40];
    
    tot_size = Ql_FatFs_GetTotal();
    fre_size = Ql_FatFs_GetFree();
    
    if(tot_size > 0)
    {       
        if (fre_size < 1024)
        {
            snprintf(buf, sizeof(buf), "SD Total: %.2f GB, Free: %d MB", tot_size / 1024.f, fre_size);
        }
        else
        {
            snprintf(buf, sizeof(buf), "SD Total: %.2f GB, Free: %.2f GB", tot_size / 1024.f, fre_size / 1024.f);
        }
    }
    else
    {
        snprintf(buf, sizeof(buf), "SD Total: 0 GB, Free: 0 GB");
    }
    return buf;
}

int Ql_FatFs_Format(void)
{
    const MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
    BYTE work[FF_MAX_SS];
    FRESULT Res;

    Ql_FatFs_UnMount();
    Res = f_mkfs("1:", &opt, work, FF_MAX_SS);
    if (Res != FR_OK)
    {
        QL_LOG_E("f_mkfs fail, %d", Res);
        return -1;
    }
    Ql_FatFs_Mount();
    
    QL_LOG_I("ok\r\n");
    
    return 0;
}

void check_sd_remain_size(void)
{
    DWORD fre_size = 0;

    fre_size = Ql_FatFs_GetFree();
    if(fre_size < QL_SD_REMAIN_MINIMUM_SIZE / 4)
    {
        QL_LOG_W("Insufficient remaining space: %d MB\r\n", fre_size);
    }
    else if(fre_size < QL_SD_REMAIN_MINIMUM_SIZE)
    {
        QL_LOG_W("Insufficient remaining space: %d MB\r\n", fre_size);
    }
}

int32_t Ql_FatFs_OpenFile(const char *pPath, FIL *pFP, uint16_t OpenFileType)
{
    FRESULT file_res;
    char *folder;

    if(Ql_FatFs_Mount_State() != 0)
    {
        Ql_FatFs_Mount();
    }

    if ((qlfs == NULL) || (pPath == NULL))
    {
        QL_LOG_I("%s, Ql_FatFs_Mount fail, path: %s", __func__, pPath);
        return -1;
    }

    folder = pvPortMalloc(strlen(pPath) + 5);
    memset(folder, 0, strlen(pPath) + 5);
    
    sprintf(folder, "1:%s", pPath);

    file_res = f_open(pFP, folder, OpenFileType);

    if(folder != NULL)
    {
        vPortFree(folder);
        folder = NULL;
    }

    if (file_res != FR_OK)
    {
        QL_LOG_E("%s, f_open fail, %d. path: %s", __func__, file_res, pPath);
        return -2;
    }

    return 0;
}

int32_t Ql_FatFs_ReadFile(const FIL *pFP, uint8_t *pBuf, uint32_t Len, uint32_t *pBytesRead)
{
    FRESULT res;

    if(pFP == NULL)
    {
        return -1;
    }
    res = f_read(pFP, pBuf, Len, (UINT)pBytesRead);  
    if (res != FR_OK) 
    {
        QL_LOG_E("%s, f_read fail, %d", __func__, res);
        return -2;
    }

    return 0;
}

int32_t Ql_FatFs_ReadFileSize(const FIL *pFP)
{
    if(pFP == NULL)
    {
        return -1;
    }

    return f_size(pFP);
}

int32_t Ql_FatFs_CloseFile(const FIL *pFP)
{
    FRESULT res;
    //int32_t  ret_unmount = 0;

    if(pFP == NULL)
    {
        return -1;
    }

    res = f_close(pFP);
    if (res != FR_OK)
    {
        QL_LOG_E("f_close fail, %d", res);
        return -2;
    }

    return 0;
}
