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
  Name: ql_nmea.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef __QL_NMEA_H__
#define __QL_NMEA_H__

#include <stdint.h>
#include <stdlib.h>

#define QL_NMEA_OUT_MSG_BUFFER_SIZE                (256U)

typedef struct
{
    char   *Cmd;
    void  (*FrameHandleFunc)(const char *Str, uint32_t Len);
} Ql_NMEA_Table_TypeDef;

typedef struct
{
    const Ql_NMEA_Table_TypeDef    *Table;
    int8_t                         *Buf;
    uint32_t                        BufLen;
    uint32_t                        BufSize;
    uint16_t                        FrameTailIndex;
    void                          (*GlobalFunc)(const int8_t *Buf, uint32_t Len);
    uint8_t                         Debug;
} Ql_NMEA_Handle_TypeDef;

int32_t Ql_NMEA_Parse(Ql_NMEA_Handle_TypeDef *Handle, const int8_t *Buf, uint32_t Len);
int32_t Ql_NMEA_Init(Ql_NMEA_Handle_TypeDef *Handle,Ql_NMEA_Table_TypeDef *Table,
                        void (*GlobalFunc)(const int8_t *Buf, uint32_t Len),uint16_t BufSize);
int Ql_NMEA_Option_Parse(char *NMEA_Str, uint32_t NMEA_Len, int *argc, char *argv[]);

uint8_t Ql_NMEA_SupportChecksum(const int8_t *Data);

#endif
