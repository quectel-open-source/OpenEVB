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
  Name: ql_ff_user.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef _QL_FF_USER_H__
#define _QL_FF_USER_H__

#include "gd32f4xx.h"
#include "ff.h"

#define QL_SD_REMAIN_MINIMUM_SIZE  (128)  // MB
//#define QL_SD_REMAIN_MINIMUM_SIZE1  (4096)  // MB
//#define QL_SD_REMAIN_MINIMUM_SIZE2  (2048)  // MB

//extern const char tempLOG[];
extern const char tempHCN[];
//extern const char tempRAW[];

#define	QL_FILE_READ			0x01
#define	QL_FILE_WRITE			0x02
#define	QL_FILE_OPEN_EXISTING	0x00
#define	QL_FILE_CREATE_NEW		0x04
#define	QL_FILE_CREATE_ALWAYS	0x08
#define	QL_FILE_OPEN_ALWAYS		0x10
#define	QL_FILE_OPEN_APPEND		0x30

int32_t  Ql_FatFs_Mount(void);
int32_t  Ql_FatFs_UnMount(void);
int32_t  Ql_FatFs_Mount_State(void);
int32_t  Ql_FatFs_Write(const char *path, const uint8_t *str, uint32_t len,  uint32_t *file_size);
uint32_t Ql_FatFs_GetTotal(void);
uint32_t Ql_FatFs_GetFree(void);
char* Ql_FatFs_Size(void);
int Ql_FatFs_Format(void);

int32_t Ql_FatFs_OpenFile(const char *pPath, FIL *pFP, uint16_t OpenFileType);
int32_t Ql_FatFs_ReadFile(const FIL *pFP, uint8_t *pBuf, uint32_t Len, uint32_t *pBytesRead);
int32_t Ql_FatFs_ReadFileSize(const FIL *pFP);
int32_t Ql_FatFs_CloseFile(const FIL *pFP);


void check_sd_remain_size(void);

#endif
