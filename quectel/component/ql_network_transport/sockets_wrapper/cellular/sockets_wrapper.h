/*
 * FreeRTOS V202112.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

// Change Log
// 2024-06, Quectel GNSS: Modify and add some code for this project

/**
 * @file sockets_wrapper.h
 * @brief FreeRTOS Sockets connect and disconnect function wrapper.
 */

#ifndef SOCKETS_WRAPPER_H
#define SOCKETS_WRAPPER_H

///**************************************************/
///******* DO NOT CHANGE the following order ********/
///**************************************************/

///* Logging related header files are required to be included in the following order:
// * 1. Include the header file "logging_levels.h".
// * 2. Define LIBRARY_LOG_NAME and  LIBRARY_LOG_LEVEL.
// * 3. Include the header file "logging_stack.h".
// */

///* Include header that defines log levels. */
//#include "logging_levels.h"

///* Logging configuration for the Sockets. */
//#ifndef LIBRARY_LOG_NAME
//    #define LIBRARY_LOG_NAME     "Sockets"
//#endif
//#ifndef LIBRARY_LOG_LEVEL
//    #define LIBRARY_LOG_LEVEL    LOG_INFO
//#endif

//#include "logging_stack.h"
///************ End of logging configuration ****************/
#include <stdint.h>
#include <stdio.h>

#define SOCKETS_ERROR_NONE          ( 0 )          /*!< No error. */
#define SOCKETS_SOCKET_ERROR        ( -1 )         /*!< Catch-all sockets error code. */
#define SOCKETS_EWOULDBLOCK         ( -11 )        /*!< A resource is temporarily unavailable. */
#define SOCKETS_ENOMEM              ( -12 )        /*!< Memory allocation failed. */
#define SOCKETS_EINVAL              ( -22 )        /*!< Invalid argument. */
#define SOCKETS_ENOPROTOOPT         ( -109 )       /*!< A bad option was specified . */
#define SOCKETS_ENOTCONN            ( -126 )       /*!< The supplied socket is not connected. */
#define SOCKETS_EISCONN             ( -127 )       /*!< The supplied socket is already connected. */
#define SOCKETS_ECLOSED             ( -128 )       /*!< The supplied socket has already been closed. */
#define SOCKETS_PERIPHERAL_RESET    ( -1006 )      /*!< Communications peripheral has been reset. */

#define SOCKETS_INVALID_SOCKET      ( ( Socket_t ) ~0U )

struct xSOCKET;
typedef struct xSOCKET * Socket_t; /**< @brief Socket handle data type. */
typedef long             BaseType_t; //soina added for avoiding compilation error

/**
 * @brief Establish a connection to server.
 *
 * @param[out] pTcpSocket The output parameter to return the created socket descriptor.
 * @param[in] pHostName Server hostname to connect to.
 * @param[in] pServerInfo Server port to connect to.
 * @param[in] receiveTimeoutMs Timeout (in milliseconds) for transport receive.
 * @param[in] sendTimeoutMs Timeout (in milliseconds) for transport send.
 *
 * @note A timeout of 0 means infinite timeout.
 *
 * @return Non-zero value on error, 0 on success.
 */
BaseType_t Sockets_Connect( Socket_t * pTcpSocket,
                            const char * pHostName,
                            uint16_t port,
                            uint32_t receiveTimeoutMs,
                            uint32_t sendTimeoutMs );

/**
 * @brief End connection to server.
 *
 * @param[in] tcpSocket The socket descriptor.
 */
void Sockets_Disconnect( Socket_t tcpSocket );

/**
 * @brief Transmit data to the remote socket.
 *
 * The socket must have already been created using a call to Sockets_Connect().
 *
 * @param[in] xSocket The handle of the sending socket.
 * @param[in] pvBuffer The buffer containing the data to be sent.
 * @param[in] xDataLength The length of the data to be sent.
 *
 * @return
 * * On success, the number of bytes actually sent is returned.
 * * If an error occurred, a negative value is returned. @ref SocketsErrors
 */
int32_t Sockets_Send( Socket_t xSocket,
                      const void * pvBuffer,
                      size_t xDataLength );

/**
 * @brief Receive data from a TCP socket.
 *
 * The socket must have already been created using a call to Sockets_Connect().
 *
 * @param[in] xSocket The handle of the socket from which data is being received.
 * @param[out] pvBuffer The buffer into which the received data will be placed.
 * @param[in] xBufferLength The maximum number of bytes which can be received.
 * pvBuffer must be at least xBufferLength bytes long.
 *
 * @return
 * * If the receive was successful then the number of bytes received (placed in the
 *   buffer pointed to by pvBuffer) is returned.
 * * If a timeout occurred before data could be received then 0 is returned (timeout
 *   is set using @ref SOCKETS_SO_RCVTIMEO).
 * * If an error occurred, a negative value is returned. @ref SocketsErrors
 */
int32_t Sockets_Recv( Socket_t xSocket,
                      void * pvBuffer,
                      size_t xBufferLength );


#include "cellular_types.h"

typedef enum 
{
    APP_INVALID = 0,

    APP_NTRIPCLIENT_BOOTSTRAP_STEP = 1, //equals to BOOTSTRAP_STEP,
    APP_NTRIPCLIENT_DEVICEAUTH_STEP = 2,//equals to DEVICEAUTH_STEP,
    APP_NTRIPCLIENT_NTRIPLOOP_STEP = 3, //equals to NTRIPLOOP_STEP,

    APP_MAX = 13,
} Ql_AppIndex_TypeDef;


Ql_AppIndex_TypeDef Ql_GetAppFromSockid(uint8_t Sockfd);
int Ql_AssociateSockidWithAppid(Socket_t SockWrapper,Ql_AppIndex_TypeDef AppId);
void Ql_SendRecvIndToApp(Ql_AppIndex_TypeDef AppId);
void Ql_SendClosedRetToApp(Ql_AppIndex_TypeDef AppId);
void Ql_SendClosedIndToApp(Ql_AppIndex_TypeDef AppId);
int Ql_FreeSockidWithAppid(Ql_AppIndex_TypeDef AppId);

typedef enum
{
    QL_TCP_QIURC_PDP_DEACT_IND = 0,    //"+QIURC: "pdpdeact"
    QL_TCP_QIURC_CONN_RETRY_MAX_FAILED_IND,
    QL_TCP_QIURC_CLOSED_CONNID_IND,    //"+QIURC: "closed",<connectID>"
    QL_TCP_QIURC_RECV_CONNID_IND,      //"+QIURC: "recv",<connectID>"
} Ql_TcpQiurcOp_TypeDef;

typedef enum
{
    QL_SOCKETSTATE_INVALID = -1, /* no correspond appid. */
    QL_SOCKETSTATE_INITIAL = 0,
    QL_SOCKETSTATE_CONNECTING,    /* Socket is connecting in progress with remote peer. */
    QL_SOCKETSTATE_CONNECT_FAIL,
    QL_SOCKETSTATE_CONNECTED,     /* Socket is connected. */
    QL_SOCKETSTATE_DISCONNECTED_PASSIVELY,   /* Socket is disconnected by remote peer or due to network error,need to close it "AT+QILCOSE" */
    QL_SOCKETSTATE_DISCONNECTED_END   /**< Socket is disconnected by remote peer or due to network error. */
} Ql_SocketState_t;

#define QL_SOCKET_INVALID   (-1)

bool Ql_TCP_SendPdpDeactReq(int ContextId);
bool Ql_TCP_ConnFailedInd(Ql_AppIndex_TypeDef AppId,char * Host);
bool Ql_TCP_SendClosedReq(int SockFd);
bool Ql_TCP_SendRecvReq(int SockFd);
int Ql_RegSockApp(Ql_AppIndex_TypeDef AppId);

Ql_SocketState_t Ql_GetSockStateFromApp(Ql_AppIndex_TypeDef AppId);
int Ql_SetSockStateToApp(Ql_AppIndex_TypeDef AppId,Ql_SocketState_t sockState);
void Ql_ManageMultiConnTaskCreate(void);

#include "transport_interface.h"

int32_t Ql_SendTcpData( const TransportInterface_t * Transport,const uint8_t * Data,uint64_t DataLen);
int32_t Ql_RecvTcpData(const TransportInterface_t * Transport,uint8_t * Data,uint64_t DataLen);

CellularError_t Cellular_GetRssiByCSQ(CellularHandle_t CellularHandle,uint32_t * Rssi);
CellularError_t Cellular_QICFG_SetTcpRetranscfg(CellularHandle_t CellularHandle,int RetransCnt,int RetransPeriod);
CellularError_t Ql_GetCellularActiveStatus(void);

#endif /* ifndef SOCKETS_WRAPPER_H */
