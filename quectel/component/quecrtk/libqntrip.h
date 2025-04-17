/*
 * @Team: Quectel-GNSS
 * @Author: xxx
 * @Date: 2022-10-21
 * @LastEditTime: 2022-10-21
 * @Description: define json struct type for parser.
 */

#ifndef __LIB_QNTRIP__
#define __LIB_QNTRIP__

#include <stdbool.h>
#include "ql_ntrip_error.h"

#define QL_BUFFER_SIZE                  1024

#define QL_NTRIP_UPLOADGGA_FREQ         1   //Hz

#define QL_UART_REVNMEA_BUFFER_SIZE     (1*QL_BUFFER_SIZE)   

#define QL_SOCKET_REVRTCM_BUFFER_SIZE   (2*QL_BUFFER_SIZE)

typedef struct Mem_Hooks
{
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} Mem_Hooks;

typedef struct Time_Hooks
{
      int (*sleep)(unsigned int microsecond);
} Time_Hooks;

typedef int (*FSOCKETLINK)(char*, int); 
typedef int (*FSOCKETSEND)(int,char*, int); 
typedef int (*FSOCKETREV)(int,char*, int, int); 
typedef int (*FSOCKETCLOSE)(int);
typedef bool (*HTTPS_PROCESS)(char * ,char * ,int ,char *,int);

extern int Ql_BS_Init(const char *MCC,const char *DeviceID, const char *EnterpriseID, const char *LicenseKey);

extern int Ql_BS_Auth(void);

int Ql_Ntrip_SetSystemAPI(Mem_Hooks *hook,Time_Hooks *time_hook,HTTPS_PROCESS pf_https);

extern int Ql_Ntrip_SetSocketAPI(FSOCKETLINK flink,FSOCKETREV frev,FSOCKETSEND fsend,FSOCKETCLOSE fclos);

extern int Ql_Ntrip_ClientInit(int type,int revbuf);

extern int Ql_Ntrip_ClientStart(void);

extern int Ql_Ntrip_UploadGGA(void *data,int len);

extern int Ql_Ntrip_ClientAuth(void);

extern int Ql_Ntrip_RecvData(char *out_buf,int out_buf_len,int timeout);

extern int Ql_Ntrip_GetRtcmData(char *in_buf, int in_buf_len, char *out_buf, int *out_buf_len);

extern int Ql_Ntrip_ClientClose(void);

extern char *Ql_Get_NmeaLine(const char* s, const char *sentence, char *pbuf,int pbuf_size);

extern char *Ql_GetSDK_Version(void);

#endif

