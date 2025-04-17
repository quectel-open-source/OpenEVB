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
  Name: ql_qgc.h
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#ifndef _QL_QGC_H
#define _QL_QGC_H

#include <stdint.h>
#include <stdlib.h>

#define QGC_FRAME_MINIMUM_SIZE    (8U)
#define QGC_FRAME_HEADER1         (0X51U)
#define QGC_FRAME_HEADER2         (0X47U)

typedef enum
{
    QL_QGC_CMD_RET_OK            = 0x00,
    QL_QGC_CMD_RET_INVALID_PAR   = 0x01,
    QL_QGC_CMD_RET_EXEC_FAIL     = 0x02,
} Ql_QGC_CmdRet_TypeDef,Ql_ACK_State_TypeDef;

typedef struct
{
    uint8_t        Header1;
    uint8_t        Header2;

    uint8_t        MsgGroupNum;
    uint8_t        MsgNum;

    uint8_t        MsgLen_L;
    uint8_t        MsgLen_H;

    uint8_t        Content[0];
} __attribute__((packed)) Ql_QGC_Frame_TypeDef;

typedef struct
{
    uint8_t    MsgGroupNum;
    uint8_t    MsgNum;
    void       (*Frame_Handle_Func)(const Ql_QGC_Frame_TypeDef *RecvFrame);
} Ql_QGC_MsgType_Table_TypeDef;

typedef struct
{
    const Ql_QGC_MsgType_Table_TypeDef   *Table;
    uint8_t                              *Buf;
    uint16_t                              Buf_Len;
    uint16_t                              Buf_Size;
    uint16_t                              Frame_Tail_Index;
    void                                 (*Global_Func)(const uint8_t *Buf, uint32_t Len);
    uint8_t                               Debug;
} Ql_QGC_Handle_TypeDef;

int Ql_QGC_Parse(Ql_QGC_Handle_TypeDef *Handle, const uint8_t *RecvBuf, uint16_t RecvLen);
int Ql_QGC_Init(Ql_QGC_Handle_TypeDef *Handle,
                  Ql_QGC_MsgType_Table_TypeDef *Table,
                  void (*Global_Func)(const uint8_t *Buf, uint32_t Len),
                  uint16_t BufSize);

#endif

