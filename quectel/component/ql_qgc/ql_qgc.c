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
  Name: ql_qgc.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "ql_qgc.h"
#include "ql_check.h"
#include "ql_application.h"

#include "ql_log_undef.h"
#define LOG_TAG "qgc"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

extern QueueHandle_t LG695H_QGC_IMU_Quene;

static uint16_t Ql_QGC_Frame_Cache_Move(uint8_t *Buf, uint16_t DataLen, uint16_t Step)
{
    if ((DataLen == 0) || (Step == 0))
    {
        return DataLen;
    }

    if (DataLen < Step)
    {
        return 0;
    }
    else if (DataLen == Step)
    {
        memset(Buf, 0, DataLen);
        return 0;
    }

    memmove(Buf, &Buf[Step], DataLen - Step);
    memset(&Buf[DataLen - Step], 0, Step);

    return (DataLen - Step);
}

/*****************************************************************************
* @brief  
* ex:
* @par
* @retval
*****************************************************************************/
static void Ql_QGC_Frame_Message_Match(Ql_QGC_Handle_TypeDef *Handle, const Ql_QGC_Frame_TypeDef *RecvFrame)
{
    const Ql_QGC_MsgType_Table_TypeDef *table = NULL;
//    uint16_t payload_len = ((uint16_t)RecvFrame->MsgLen_H << 8) | RecvFrame->MsgLen_L;
    uint8_t msg_group_num = RecvFrame->MsgGroupNum;
    uint8_t msg_num = RecvFrame->MsgNum;
//    uint16_t msg_len     = payload_len;

    //if (Handle->Debug)
    {
        QL_LOG_D("QGC msg_group_num: 0X%02X,len: %d", msg_group_num, msg_len);

//        for (uint16_t i = 0; i < Msg_Len + 6 + 2; i++)
//        {
//            ql_printf("%02X ", ((uint8_t *)Recv_Frame)[i]);
//        }
//        ql_printf("\r\n\r\n");
    }

    if (NULL == Handle->Table)
    {
        return;
    }

    for (uint8_t i = 0; ; i++)
    {
        table = &Handle->Table[i];

        /* Message Type */
        if (table->MsgGroupNum == 0 || table->Frame_Handle_Func == NULL)
        {
            break;
        }

        if (table->MsgGroupNum != msg_group_num || table->MsgNum != msg_num)
        {
            continue;
        }

        /* Processing */
        table->Frame_Handle_Func(RecvFrame);
        break;
    }
}

int Ql_QGC_Parse(Ql_QGC_Handle_TypeDef *Handle, const uint8_t *RecvBuf, uint16_t RecvLen)
{
    Ql_QGC_Frame_TypeDef *recv_frame = NULL;
    uint16_t payload_len = 0;
    uint16_t index = 0;
    uint8_t qgc_num = 0;
    uint16_t checksum = 0;

    if ((Handle->Buf_Len + RecvLen) < Handle->Buf_Size)
    {
        memcpy(Handle->Buf + Handle->Buf_Len, RecvBuf, RecvLen);
        Handle->Buf_Len += RecvLen;
    }
    Handle->Frame_Tail_Index = 0;

    for ( ; ; )
    {
        if (Handle->Buf_Len < (index + QGC_FRAME_MINIMUM_SIZE))
        {
            break;
        }

        recv_frame = (Ql_QGC_Frame_TypeDef *)(Handle->Buf + index);

        /* 1. find frame preamble */
        if (recv_frame->Header1 != QGC_FRAME_HEADER1)
        {
            index++;
            continue;
        }

        if (recv_frame->Header2 != QGC_FRAME_HEADER2)
        {
            index ++;
            continue;
        }

        //avoid GQGSV
        if (recv_frame->MsgGroupNum == 0X53)
        {
            index ++;
            continue;
        }

        QL_LOG_D("Header1    :0X%02X,Header2:0X%02X",recv_frame->Header1,recv_frame->Header2);
        QL_LOG_D("MsgGroupNum:0X%02X,MsgNum :0X%02X",recv_frame->MsgGroupNum,recv_frame->MsgNum);
        QL_LOG_D("MsgLen_L   :0X%02X,MsgLen_H:0X%02X",recv_frame->MsgLen_L,recv_frame->MsgLen_H);

        /* 2. Check the length */
        payload_len = ((uint16_t)recv_frame->MsgLen_H << 8) | recv_frame->MsgLen_L;
        QL_LOG_D("Payload_Len[0X%04X]",payload_len);

        QL_LOG_D("Handle->Buf_Len[%d],Payload_Len[%d],Index[%d]",Handle->Buf_Len,payload_len,index);

        if (Handle->Buf_Len < (index + QGC_FRAME_MINIMUM_SIZE + payload_len))
        {
            index++;
            continue;
        }

        /* 3. Check the sum */
        QL_LOG_D("Recv_Frame[6 + Payload_Len]:0X%02X",((uint8_t *)recv_frame)[6 + payload_len]);
        QL_LOG_D("Recv_Frame[6 + Payload_Len + 1]:0X%02X",((uint8_t *)recv_frame)[6 + payload_len + 1]);
        checksum = Ql_Check_Fletcher((uint8_t *)recv_frame + 2,2 + 2 + payload_len);
        QL_LOG_D("checksum:0X%04X",checksum);
        QL_LOG_D("checksum & 0xFF:0X%04X",checksum & 0xFF);
        QL_LOG_D("((checksum >> 8) & 0xFF):0X%04X",((checksum >> 8) & 0xFF));
        if((checksum & 0xFF) != ((uint8_t *)recv_frame)[6 + payload_len]  //CHK1
            || ((checksum >> 8) & 0xFF) != ((uint8_t *)recv_frame)[6 + payload_len + 1]) //CHK2
        {
            index++;
            continue;
        }

//        ql_printf("QGC recv:");
//        for (uint8_t i = 0; i < 20; i++)
//        {
//            ql_printf("%02X ", ((uint8_t *)Recv_Frame)[i]);
//        }
//        ql_printf("\r\n");

        /* 4. Message matching and processing */
        Ql_QGC_Frame_Message_Match(Handle, recv_frame);

        if (index > Handle->Frame_Tail_Index)
        {
            Ql_QGC_Frame_Cache_Move(Handle->Buf + Handle->Frame_Tail_Index,
                                      Handle->Buf_Len - Handle->Frame_Tail_Index,
                                      index - Handle->Frame_Tail_Index);
            Handle->Buf_Len -= (index - Handle->Frame_Tail_Index);
        }
        index = Handle->Frame_Tail_Index + QGC_FRAME_MINIMUM_SIZE + payload_len;
        Handle->Frame_Tail_Index = index;

        qgc_num++;
//        vTaskDelay(1);
    }

    if (Handle->Frame_Tail_Index > 0)
    {
        if (Handle->Global_Func != NULL)
        {
            Handle->Global_Func((const uint8_t *)Handle->Buf, Handle->Frame_Tail_Index);
        }

        Ql_QGC_Frame_Cache_Move(Handle->Buf, Handle->Buf_Len, Handle->Frame_Tail_Index);
        Handle->Buf_Len = Handle->Buf_Len - Handle->Frame_Tail_Index;
    }
    else
    {
        if (Handle->Buf_Len * 2 > Handle->Buf_Size)
        {
            memset(Handle->Buf, 0, Handle->Buf_Len);
            Handle->Buf_Len = 0;
        }
    }

    return qgc_num;
}

int Ql_QGC_Init(Ql_QGC_Handle_TypeDef *Handle,
                  Ql_QGC_MsgType_Table_TypeDef *Table,
                  void (*Global_Func)(const uint8_t *Buf, uint32_t Len),
                  uint16_t BufSize)
{
    if (Handle == NULL || BufSize == 0)
    {
        return -1;
    }

    Handle->Buf_Len  = 0;
    Handle->Buf_Size = BufSize;
    Handle->Buf  = (uint8_t *)pvPortMalloc(Handle->Buf_Size);
    if (Handle->Buf == NULL)
    {
        QL_LOG_E("Malloc fail");
        return -1;
    }

    Handle->Table = Table;
    Handle->Global_Func = Global_Func;
    Handle->Frame_Tail_Index = 0;
    Handle->Debug = 0;

    return 0;
}

