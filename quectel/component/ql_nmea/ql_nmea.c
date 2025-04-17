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
  Name: ql_nmea.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include "FreeRTOS.h"

#include "ql_nmea.h"
#include "ql_uart.h"
#include "ql_check.h"

#define LOG_TAG "nmea"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

static int8_t Ql_NMEA_MsgBuf[256];

static uint16_t Ql_NMEA_FrameCacheMove(int8_t *Buf, uint16_t BufLen, uint16_t Step)
{
    if ((BufLen == 0) || (Step == 0))
    {
        return BufLen;
    }

    if (BufLen < Step)
    {
        return 0;
    }
    else if (BufLen == Step)
    {
        memset(Buf, 0, BufLen);
        return 0;
    }

    memmove(Buf, &Buf[Step], BufLen - Step);
    memset(&Buf[BufLen - Step], 0, Step);

    return (BufLen - Step);
}

static void Ql_NMEA_Match(Ql_NMEA_Handle_TypeDef *Handle, const int8_t *Buf, uint32_t Len)
{
    const Ql_NMEA_Table_TypeDef *table = NULL;
    uint8_t i = 0;

    if (Handle->Table == NULL)
    {
        return;
    }

    if (Len > (sizeof(Ql_NMEA_MsgBuf) - 1))
    {
        return;
    }

    memcpy(Ql_NMEA_MsgBuf, Buf, Len);
    Ql_NMEA_MsgBuf[Len] = '\0';

    if (Handle->Debug)
    {
        Ql_Printf("%s", Ql_NMEA_MsgBuf);
    }
    
    for (i = 0; ; i++)
    {
        table = &Handle->Table[i];
        
        /* Message Type */
        if (table->Cmd == NULL)
        {
            break;
        }

        if (strstr(Ql_NMEA_MsgBuf, table->Cmd) == NULL)
        {
            continue;
        }

        if (table->FrameHandleFunc == NULL)
        {
            break;
        }

        /* Processing */
        table->FrameHandleFunc(Ql_NMEA_MsgBuf, Len);
        break;
    }
}

int32_t Ql_NMEA_Parse(Ql_NMEA_Handle_TypeDef *Handle, const int8_t *RecvBuf, uint32_t RecvBufLen)
{
    int8_t ch1 = '\0';
    int8_t ch2 = '\0';
    uint8_t check_xor_1 = 0;
    uint8_t check_xor_2 = 0;
    uint16_t index = 0;
    int32_t nmea_num = 0;
    int8_t *p = NULL;
    int8_t *buffer = NULL;
    int8_t *frame = NULL;
    uint32_t frame_len = 0;

    if ((Handle->BufLen + RecvBufLen) < Handle->BufSize)
    {
        memcpy(Handle->Buf + Handle->BufLen, RecvBuf, RecvBufLen);
        Handle->BufLen += RecvBufLen;
    }
    Handle->FrameTailIndex = 0;

    for ( ; ; )
    {
        if (Handle->BufLen <= index)
        {
            break;
        }

        buffer = Handle->Buf + index;
        if (*buffer == '$')
        {
            ch1 = '\0';
            ch2 = '\0';
            frame = buffer;
            frame_len = 0;
        }

        ch1 = ch2;
        ch2 = *buffer;
        frame_len++;

        if ((ch1 != '\r') || (ch2 != '\n'))
        {
            index++;
            continue;
        }

        // Check Len
        if (frame_len < 6)
        {
            index++;
            frame = NULL;
            continue;
        }

        // Check Header
        if (frame[0] != '$')
        {
            index++;
            frame = NULL;
            continue;
        }

        // Check XOR
        if (frame[frame_len - 5] != '*')
        {
            index++;
            frame = NULL;
            continue;
        }

        check_xor_1 = strtoul((const char *)frame + frame_len - 4, (char **)&p, 16);
        check_xor_2 = Ql_CheckXOR((const uint8_t *)frame + 1, frame_len - 6);
        if (check_xor_1 != check_xor_2)
        {
            index++;
            frame = NULL;
            continue;
        }

        Ql_NMEA_Match(Handle, (const int8_t *)frame, frame_len);

        if ((index + 1) > (Handle->FrameTailIndex + frame_len))
        {
            Ql_NMEA_FrameCacheMove(Handle->Buf + Handle->FrameTailIndex,
                                    Handle->BufLen - Handle->FrameTailIndex,
                                    index + 1 - frame_len - Handle->FrameTailIndex);
            Handle->BufLen -= (index + 1 - frame_len - Handle->FrameTailIndex);
        }

        index = Handle->FrameTailIndex + frame_len;
        Handle->FrameTailIndex = index;
        frame = NULL;

        nmea_num++;
    }

    if (Handle->FrameTailIndex > 0)
    {
        if (Handle->GlobalFunc != NULL)
        {
            Handle->GlobalFunc((const int8_t *)Handle->Buf, Handle->FrameTailIndex);
        }

        Ql_NMEA_FrameCacheMove(Handle->Buf, Handle->BufLen, Handle->FrameTailIndex);
        Handle->BufLen = Handle->BufLen - Handle->FrameTailIndex;
    }
    else
    {
        if (Handle->BufLen * 2 > Handle->BufSize)
        {
            memset(Handle->Buf, 0, Handle->BufLen);
            Handle->BufLen = 0;
        }
    }

    return nmea_num;
}

int Ql_NMEA_Init(Ql_NMEA_Handle_TypeDef *Handle,
                        Ql_NMEA_Table_TypeDef *Table,
                        void (*GlobalFunc)(const int8_t *Buf, uint32_t Len),
                        uint16_t BufSize)
{
    if (Handle == NULL || BufSize == 0)
    {
        return -1;
    }

    Handle->BufLen  = 0;
    Handle->BufSize = BufSize;
    Handle->Buf  = (int8_t *)pvPortMalloc(Handle->BufSize);
    if (Handle->Buf == NULL)
    {
        QL_LOG_E("Malloc fail");
        return -1;
    }

    Handle->Table = Table;
    Handle->GlobalFunc = GlobalFunc;
    Handle->FrameTailIndex = 0;
    Handle->Debug = 0;

    return 0;
}

int Ql_NMEA_Option_Parse(char *NMEA_Str, uint32_t NMEA_Len, int *argc, char *argv[])
{
    int argc_max = *argc;
    char *p = NMEA_Str;
    
    *argc = 0;
    if (argc_max <= 0)
    {
        return -1;
    }
    
    argv[0] = p;
    (*argc)++;
    
    for (uint32_t i = 0; i < NMEA_Len; i++, p++)
    {
        if (*p == '*' || *p == '\0')
        {
            *p = '\0';
            break;
        }
        else if (*p == ',')
        {
            *p = '\0';
            if (*argc >= argc_max)
            {
                return -1;
            }
            
            argv[*argc] = p + 1;
            (*argc)++;
        }
    }
    return 0;
}

uint8_t Ql_NMEA_SupportChecksum(const int8_t *Data)
{
    uint16_t len = strlen((const char *)Data);
    return Ql_CheckXOR((uint8_t *)Data + 1, len - 1);
}

