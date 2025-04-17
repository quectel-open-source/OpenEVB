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
  Name: ql_ctype.c
  History:
    Version  Date         Author   Description
    v1.0     2024-0908    Hayden   Create file
*/

#include <stdio.h>
#include <string.h>

#include "ql_ctype.h"

static const char EncodingTable[BUFFSIZE64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

/* does not buffer overrun, but breaks directly after an error */
/* returns the number of required bytes */
uint16_t Ql_Base64_Encode(int8_t *Buf, uint16_t Size, const int8_t *User, const int8_t *Pwd)
{
    uint8_t inbuf[3] = {0};
    int8_t *out = Buf;
    uint16_t i = 0;
    uint16_t sep = 0;
    uint16_t fill = 0;
    uint16_t bytes = 0;

    while((NULL != *User) || (NULL != *Pwd))
    {
        i = 0;

        while((i < 3) && (NULL != *User))
        {
            inbuf[i++] = *(User++);
        }

        if((i < 3) && (0 == sep))
        {
            inbuf[i++] = ':';
            ++sep;
        }

        while((i < 3) && (NULL != *Pwd))
        {
            inbuf[i++] = *(Pwd++);
        }

        while(i < 3)
        {
            inbuf[i++] = 0;
            ++fill;
        }

        if((out - Buf) < (Size - 1))
        {
            *(out++) = EncodingTable[(inbuf[0] & 0xFC) >> 2];
        }

        if((out - Buf) < (Size - 1))
        {
            *(out++) = EncodingTable[((inbuf[0] & 0x03) << 4) | ((inbuf[1] & 0xF0) >> 4)];
        }

        if((out - Buf) < (Size - 1))
        {
            if (2 == fill)
            {
                *(out++) = '=';
            }
            else
            {
                *(out++) = EncodingTable[((inbuf[1] & 0x0F) << 2) | ((inbuf[2] & 0xC0) >> 6)];
            }
        }

        if(out - Buf < Size - 1)
        {
            if(fill >= 1)
            {
                *(out++) = '=';
            }
            else
            {
                *(out++) = EncodingTable[inbuf[2] & 0x3F];
            }
        }
        bytes += 4;
    }

    if((out - Buf) < Size)
    {
        *out = 0;
    }

    return bytes;
}

int Ql_Base64_Decode(const int8_t * base64, uint8_t * bindata)
{
    int32_t i;
    int32_t j;
    uint8_t k;
    uint8_t temp[4];

    for(i = 0, j = 0; base64[i] != '\0' ; i += 4)
    {
        memset(temp, 0xFF, sizeof(temp));

        for(k = 0 ; k < 64 ; k ++)
        {
            if(EncodingTable[k] == base64[i])
            {
                temp[0] = k;
            }
        }

        for(k = 0 ; k < 64 ; k ++)
        {
            if(EncodingTable[k] == base64[i + 1])
            {
                temp[1] = k;
            }
        }

        for(k = 0 ; k < 64 ; k ++)
        {
            if(EncodingTable[k] == base64[i + 2])
            {
                temp[2] = k;
            }
        }

        for(k = 0 ; k < 64 ; k ++)
        {
            if(EncodingTable[k] == base64[i + 3])
            {
                temp[3] = k;
            }
        }

        bindata[j++] = (((unsigned char)(temp[0] << 2)) & 0xFC) | (((unsigned char)(temp[1] >> 4)) & 0x03);
        if (base64[i + 2] == '=')
        {
            break;
        }

        bindata[j++] = (((unsigned char)(temp[1] << 4)) & 0xF0) | (((unsigned char)(temp[2] >> 2)) & 0x0F);
        if (base64[i + 3] == '=')
        {
            break;
        }

        bindata[j++] = (((unsigned char)(temp[2] << 6)) & 0xF0) | ((unsigned char)(temp[3] & 0x3F));
    }

    return j;
}

