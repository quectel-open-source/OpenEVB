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

/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "event_groups.h"

/* Sockets wrapper includes. */
#include "sockets_wrapper.h"

/* FreeRTOS Cellular Library api includes. */
#include "cellular_config.h"
#include "cellular_config_defaults.h"
#include "cellular_api.h"
#include "cellular_types.h"
#include "cellular_common.h"
#include "cellular_wrapper.h"

#include "ql_cellular_dev.h"
#include "ql_application.h"

/*-----------------------------------------------------------*/
// log

#if defined(LogError)
#undef LogError
#endif
#if defined(LogWarn)
#undef LogWarn
#endif
#if defined(LogInfo)
#undef LogInfo
#endif
#if defined(LogDebug)
#undef LogDebug
#endif
#include "ql_log_undef.h"

#define LOG_TAG "CELLULAR_SOCKETS"
#define LOG_LVL QL_LOG_INFO
#include "ql_log.h"

#define LogError( message )     QL_LOG_E message
#define LogWarn( message )      QL_LOG_W message
#define LogInfo( message )      QL_LOG_I message
#define LogDebug( message )     QL_LOG_D message

#define IotLogError(...)        QL_LOG_E(__VA_ARGS__)
#define IotLogWarn(...)         QL_LOG_W(__VA_ARGS__)
#define IotLogInfo(...)         QL_LOG_I(__VA_ARGS__)
#define IotLogDebug(...)        QL_LOG_D(__VA_ARGS__)

/*-----------------------------------------------------------*/

/* Cellular socket wrapper needs application provide the cellular handle and pdn context id. */
/* User of cellular socket wrapper should provide this variable. */
/* coverity[misra_c_2012_rule_8_6_violation] */
extern CellularHandle_t CellularHandle;

/* User of cellular socket wrapper should provide this variable. */
/* coverity[misra_c_2012_rule_8_6_violation] */
extern uint8_t CellularSocketPdnContextId;

/*-----------------------------------------------------------*/

/* Windows simulator implementation. */
#if defined( _WIN32 ) || defined( _WIN64 )
    #define strtok_r                         strtok_s
#endif

#define CELLULAR_SOCKET_OPEN_FLAG            ( 1UL << 0 )
#define CELLULAR_SOCKET_CONNECT_FLAG         ( 1UL << 1 )

#define SOCKET_DATA_RECEIVED_CALLBACK_BIT    ( 0x00000001U )
#define SOCKET_OPEN_CALLBACK_BIT             ( 0x00000002U )
#define SOCKET_OPEN_FAILED_CALLBACK_BIT      ( 0x00000004U )
#define SOCKET_CLOSE_CALLBACK_BIT            ( 0x00000008U )

/* Ticks MS conversion macros. */
#define TICKS_TO_MS( xTicks )    ( ( ( xTicks ) * 1000U ) / ( ( uint32_t ) configTICK_RATE_HZ ) )
#define UINT32_MAX_DELAY_MS                    ( 0xFFFFFFFFUL )
#define UINT32_MAX_MS_TICKS                    ( UINT32_MAX_DELAY_MS / ( TICKS_TO_MS( 1U ) ) )

/* Cellular socket access mode. */
#define CELLULAR_SOCKET_ACCESS_MODE            CELLULAR_ACCESSMODE_BUFFER

///* Cellular socket open timeout. */
#define CELLULAR_SOCKET_OPEN_TIMEOUT_TICKS     ( pdMS_TO_TICKS( 15 * 1000U ) )//( portMAX_DELAY )//150 * 1000U
//#define CELLULAR_SOCKET_CLOSE_TIMEOUT_TICKS    ( pdMS_TO_TICKS( 10000U ) )

/* Cellular socket AT command timeout. */
#define CELLULAR_SOCKET_RECV_TIMEOUT_MS        ( 1000UL )

/* Time conversion constants. */
#define _MILLISECONDS_PER_SECOND               ( 1000 )                                          /**< @brief Milliseconds per second. */
#define _MILLISECONDS_PER_TICK                 ( _MILLISECONDS_PER_SECOND / configTICK_RATE_HZ ) /**< Milliseconds per FreeRTOS tick. */

/*-----------------------------------------------------------*/

typedef struct xSOCKET
{
    CellularSocketHandle_t cellularSocketHandle;
    uint32_t ulFlags;

    TickType_t receiveTimeout;
    TickType_t sendTimeout;

    EventGroupHandle_t socketEventGroupHandle;
} cellularSocketWrapper_t;

/*-----------------------------------------------------------*/
#ifdef __EXAMPLE_NTRIP_CLIENT__
//extern EventGroupHandle_t Ql_NtripClientEvent;
#endif

/**
 * @brief Get the count of milliseconds since vTaskStartScheduler was called.
 *
 * @return The count of milliseconds since vTaskStartScheduler was called.
 */
/*static*/ uint64_t getTimeMs( void );

/**
 * @brief Receive data from cellular socket.
 *
 * @param[in] pCellularSocketContext Cellular socket wrapper context for socket operations.
 * @param[out] buf The data buffer for receiving data.
 * @param[in] len The length of the data buffer
 *
 * @note This function receives data. It returns when non-zero bytes of data is received,
 * when an error occurs, or when timeout occurs. Receive timeout unit is TickType_t.
 * Any timeout value bigger than portMAX_DELAY will be regarded as portMAX_DELAY.
 * In this case, this function waits portMAX_DELAY until non-zero bytes of data is received
 * or until an error occurs.
 *
 * @return Positive value indicate the number of bytes received. Otherwise, error code defined
 * in sockets_wrapper.h is returned.
 */
static BaseType_t prvNetworkRecvCellular( const cellularSocketWrapper_t * pCellularSocketContext,
                                          uint8_t * buf,
                                          size_t len );

/**
 * @brief Callback used to inform about the status of socket open.
 *
 * @param[in] urcEvent URC Event that happened.
 * @param[in] socketHandle Socket handle for which data is ready.
 * @param[in] pCallbackContext pCallbackContext parameter in
 * Cellular_SocketRegisterSocketOpenCallback function.
 */
static void prvCellularSocketOpenCallback( CellularUrcEvent_t urcEvent,
                                           CellularSocketHandle_t socketHandle,
                                           void * pCallbackContext );

/**
 * @brief Callback used to inform that data is ready for reading on a socket.
 *
 * @param[in] socketHandle Socket handle for which data is ready.
 * @param[in] pCallbackContext pCallbackContext parameter in
 * Cellular_SocketRegisterDataReadyCallback function.
 */
static void prvCellularSocketDataReadyCallback( CellularSocketHandle_t socketHandle,
                                                void * pCallbackContext );


/**
 * @brief Callback used to inform that remote end closed the connection for a
 * connected socket.
 *
 * @param[in] socketHandle Socket handle for which remote end closed the
 * connection.
 * @param[in] pCallbackContext pCallbackContext parameter in
 * Cellular_SocketRegisterClosedCallback function.
 */
static void prvCellularSocketClosedCallback( CellularSocketHandle_t socketHandle,
                                             void * pCallbackContext );

/**
 * @brief Setup socket receive timeout.
 *
 * @param[in] pCellularSocketContext Cellular socket wrapper context for socket operations.
 * @param[out] receiveTimeout Socket receive timeout in TickType_t.
 *
 * @return On success, SOCKETS_ERROR_NONE is returned. If an error occurred, error code defined
 * in sockets_wrapper.h is returned.
 */
static BaseType_t prvSetupSocketRecvTimeout( cellularSocketWrapper_t * pCellularSocketContext,
                                             TickType_t receiveTimeout );

/**
 * @brief Setup socket send timeout.
 *
 * @param[in] pCellularSocketContext Cellular socket wrapper context for socket operations.
 * @param[out] sendTimeout Socket send timeout in TickType_t.
 *
 * @note Send timeout unit is TickType_t. The underlying cellular API uses miliseconds for timeout.
 * Any send timeout greater than UINT32_MAX_MS_TICKS( UINT32_MAX_DELAY_MS/MS_PER_TICKS ) or
 * portMAX_DELAY is regarded as UINT32_MAX_DELAY_MS for cellular API.
 *
 * @return On success, SOCKETS_ERROR_NONE is returned. If an error occurred, error code defined
 * in sockets_wrapper.h is returned.
 */
static BaseType_t prvSetupSocketSendTimeout( cellularSocketWrapper_t * pCellularSocketContext,
                                             TickType_t sendTimeout );

/**
 * @brief Setup cellular socket callback function.
 *
 * @param[in] CellularSocketHandle_t Cellular socket handle for cellular socket operations.
 * @param[in] pCellularSocketContext Cellular socket wrapper context for socket operations.
 *
 * @return On success, SOCKETS_ERROR_NONE is returned. If an error occurred, error code defined
 * in sockets_wrapper.h is returned.
 */
static BaseType_t prvCellularSocketRegisterCallback( CellularSocketHandle_t cellularSocketHandle,
                                                     cellularSocketWrapper_t * pCellularSocketContext );

/**
 * @brief Calculate elapsed time from current time and input parameters.
 *
 * @param[in] entryTimeMs The entry time to be compared with current time.
 * @param[in] timeoutValueMs Timeout value for the comparison between entry time and current time.
 * @param[out] pElapsedTimeMs The elapsed time if timeout condition is true.
 *
 * @return True if the difference between entry time and current time is bigger or
 * equal to timeoutValueMs. Otherwise, return false.
 */
/*static*/ bool _calculateElapsedTime( uint64_t entryTimeMs,
                                   uint32_t timeoutValueMs,
                                   uint64_t * pElapsedTimeMs );

/*-----------------------------------------------------------*/

/*static*/ uint64_t getTimeMs( void )
{
    TimeOut_t xCurrentTime = { 0 };

    /* This must be unsigned because the behavior of signed integer overflow is undefined. */
    uint64_t ullTickCount = 0ULL;

    /* Get the current tick count and overflow count. vTaskSetTimeOutState()
     * is used to get these values because they are both static in tasks.c. */
    vTaskSetTimeOutState( &xCurrentTime );

    /* Adjust the tick count for the number of times a TickType_t has overflowed. */
    ullTickCount = ( uint64_t ) ( xCurrentTime.xOverflowCount ) << ( sizeof( TickType_t ) * 8 );

    /* Add the current tick count. */
    ullTickCount += xCurrentTime.xTimeOnEntering;

    /* Return the ticks converted to milliseconds. */
    return ullTickCount * _MILLISECONDS_PER_TICK;
}

/*-----------------------------------------------------------*/

static BaseType_t prvNetworkRecvCellular( const cellularSocketWrapper_t * pCellularSocketContext,
                                          uint8_t * buf,
                                          size_t len )
{
    CellularSocketHandle_t cellularSocketHandle = NULL;
    BaseType_t retRecvLength = 0;
    uint32_t recvLength = 0;
    TickType_t recvTimeout = 0;
    TickType_t recvStartTime = 0;
    CellularError_t socketStatus = CELLULAR_SUCCESS;
    EventBits_t waitEventBits = 0;

    cellularSocketHandle = pCellularSocketContext->cellularSocketHandle;

    if( pCellularSocketContext->receiveTimeout >= portMAX_DELAY )
    {
        recvTimeout = portMAX_DELAY;
    }
    else
    {
        recvTimeout = pCellularSocketContext->receiveTimeout;
    }

    recvStartTime = xTaskGetTickCount();

    ( void ) xEventGroupClearBits( pCellularSocketContext->socketEventGroupHandle,
                                   SOCKET_DATA_RECEIVED_CALLBACK_BIT );
    socketStatus = Cellular_SocketRecv( CellularHandle, cellularSocketHandle, buf, len, &recvLength );

    /* Calculate remain recvTimeout. */
    if( recvTimeout != portMAX_DELAY )
    {
        if( ( recvStartTime + recvTimeout ) > xTaskGetTickCount() )
        {
            recvTimeout = recvTimeout - ( xTaskGetTickCount() - recvStartTime );
        }
        else
        {
            recvTimeout = 0;
        }
    }

    if( ( socketStatus == CELLULAR_SUCCESS ) && ( recvLength == 0U ) &&
        ( recvTimeout != 0U ) )
    {
        waitEventBits = xEventGroupWaitBits( pCellularSocketContext->socketEventGroupHandle,
                                             SOCKET_DATA_RECEIVED_CALLBACK_BIT | SOCKET_CLOSE_CALLBACK_BIT,
                                             pdTRUE,
                                             pdFALSE,
                                             recvTimeout );

        if( ( waitEventBits & SOCKET_CLOSE_CALLBACK_BIT ) != 0U )
        {
            socketStatus = CELLULAR_SOCKET_CLOSED;
        }
        else if( ( waitEventBits & SOCKET_DATA_RECEIVED_CALLBACK_BIT ) != 0U )
        {
            socketStatus = Cellular_SocketRecv( CellularHandle, cellularSocketHandle, buf, len, &recvLength );
        }
        else
        {
            IotLogDebug( "prvNetworkRecv timeout" );
            socketStatus = CELLULAR_SUCCESS;
            recvLength = 0;
        }
    }

    if( socketStatus == CELLULAR_SUCCESS )
    {
        retRecvLength = ( BaseType_t ) recvLength;
    }
    else
    {
        IotLogError( "prvNetworkRecv failed %d", socketStatus );
        retRecvLength = SOCKETS_SOCKET_ERROR;
    }

    IotLogDebug( "prvNetworkRecv expect %d read %d", len, recvLength );
    return retRecvLength;
}

/*-----------------------------------------------------------*/

static void prvCellularSocketOpenCallback( CellularUrcEvent_t urcEvent,
                                           CellularSocketHandle_t socketHandle,
                                           void * pCallbackContext )
{
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) pCallbackContext;

    ( void ) socketHandle;

    if( pCellularSocketContext != NULL )
    {
        IotLogDebug( "Socket open callback on Socket %p %d.",
                     pCellularSocketContext, urcEvent );

        if( urcEvent == CELLULAR_URC_SOCKET_OPENED )
        {
            pCellularSocketContext->ulFlags = pCellularSocketContext->ulFlags | CELLULAR_SOCKET_CONNECT_FLAG;
            ( void ) xEventGroupSetBits( pCellularSocketContext->socketEventGroupHandle,
                                         SOCKET_OPEN_CALLBACK_BIT );
        }
        else
        {
            /* Socket open failed. */
            ( void ) xEventGroupSetBits( pCellularSocketContext->socketEventGroupHandle,
                                         SOCKET_OPEN_FAILED_CALLBACK_BIT );
        }
    }
    else
    {
        IotLogError( "Spurious socket open callback." );
    }
}

/*-----------------------------------------------------------*/

static void prvCellularSocketDataReadyCallback( CellularSocketHandle_t socketHandle,
                                                void * pCallbackContext )
{
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) pCallbackContext;

    /*  added for notifing the upper layer to read data -- BEGIN*/
    //( void ) socketHandle;
    uint32_t socket_id = (*socketHandle).socketId;
    /*  added for notifing the upper layer to read data -- END */

    if( pCellularSocketContext != NULL )
    {
        IotLogDebug( "Data ready on Socket %p", pCellularSocketContext );
        ( void ) xEventGroupSetBits( pCellularSocketContext->socketEventGroupHandle,
                                     SOCKET_DATA_RECEIVED_CALLBACK_BIT );

         //Ql_SendRecvIndToApp(socket_id);/* added for notifing the upper layer to read data*/
         Ql_TCP_SendRecvReq(socket_id);
    }
    else
    {
        IotLogError( "spurious data callback" );
    }
}

/*-----------------------------------------------------------*/

static void prvCellularSocketClosedCallback( CellularSocketHandle_t socketHandle,
                                             void * pCallbackContext )
{
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) pCallbackContext;

    /* added for notifing the upper layer connection is closed -- BEGIN*/
    //( void ) socketHandle;
    uint32_t socket_id = (*socketHandle).socketId;
    /* added for notifing the upper layer connection is closed -- END*/

    if( pCellularSocketContext != NULL )
    {
        IotLogDebug( "Socket Close on Socket %p", pCellularSocketContext );
        pCellularSocketContext->ulFlags = pCellularSocketContext->ulFlags & ( ~CELLULAR_SOCKET_CONNECT_FLAG );
        ( void ) xEventGroupSetBits( pCellularSocketContext->socketEventGroupHandle,
                                     SOCKET_CLOSE_CALLBACK_BIT );

        /* added for notifing the upper layer the connection is closed*/
        Ql_TCP_SendClosedReq(socket_id);
    }
    else
    {
        IotLogError( "spurious socket close callback" );
    }
}

/*-----------------------------------------------------------*/

static BaseType_t prvSetupSocketRecvTimeout( cellularSocketWrapper_t * pCellularSocketContext,
                                             TickType_t receiveTimeout )
{
    BaseType_t retSetSockOpt = SOCKETS_ERROR_NONE;

    if( pCellularSocketContext == NULL )
    {
        retSetSockOpt = SOCKETS_EINVAL;
    }
    else
    {
        if( receiveTimeout >= portMAX_DELAY )
        {
            pCellularSocketContext->receiveTimeout = portMAX_DELAY;
        }
        else
        {
            pCellularSocketContext->receiveTimeout = receiveTimeout;
        }
    }

    return retSetSockOpt;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSetupSocketSendTimeout( cellularSocketWrapper_t * pCellularSocketContext,
                                             TickType_t sendTimeout )
{
    CellularError_t socketStatus = CELLULAR_SUCCESS;
    BaseType_t retSetSockOpt = SOCKETS_ERROR_NONE;
    uint32_t sendTimeoutMs = 0;
    CellularSocketHandle_t cellularSocketHandle = NULL;

    if( pCellularSocketContext == NULL )
    {
        retSetSockOpt = SOCKETS_EINVAL;
    }
    else
    {
        cellularSocketHandle = pCellularSocketContext->cellularSocketHandle;

        if( sendTimeout >= UINT32_MAX_MS_TICKS )
        {
            /* Check if the ticks cause overflow. */
            pCellularSocketContext->sendTimeout = portMAX_DELAY;
            sendTimeoutMs = UINT32_MAX_DELAY_MS;
        }
        else if( sendTimeout >= portMAX_DELAY )
        {
            IotLogWarn( "Sendtimeout %d longer than portMAX_DELAY, %ld ms is used instead",
                        sendTimeout, UINT32_MAX_DELAY_MS );
            pCellularSocketContext->sendTimeout = portMAX_DELAY;
            sendTimeoutMs = UINT32_MAX_DELAY_MS;
        }
        else
        {
            pCellularSocketContext->sendTimeout = sendTimeout;
            sendTimeoutMs = TICKS_TO_MS( sendTimeout );
        }

        socketStatus = Cellular_SocketSetSockOpt( CellularHandle,
                                                  cellularSocketHandle,
                                                  CELLULAR_SOCKET_OPTION_LEVEL_TRANSPORT,
                                                  CELLULAR_SOCKET_OPTION_SEND_TIMEOUT,
                                                  ( const uint8_t * ) &sendTimeoutMs,
                                                  sizeof( uint32_t ) );

        if( socketStatus != CELLULAR_SUCCESS )
        {
            retSetSockOpt = SOCKETS_EINVAL;
        }
    }

    return retSetSockOpt;
}

/*-----------------------------------------------------------*/

static BaseType_t prvCellularSocketRegisterCallback( CellularSocketHandle_t cellularSocketHandle,
                                                     cellularSocketWrapper_t * pCellularSocketContext )
{
    BaseType_t retRegCallback = SOCKETS_ERROR_NONE;
    CellularError_t socketStatus = CELLULAR_SUCCESS;

    if( cellularSocketHandle == NULL )
    {
        retRegCallback = SOCKETS_EINVAL;
    }

    if( retRegCallback == SOCKETS_ERROR_NONE )
    {
        socketStatus = Cellular_SocketRegisterDataReadyCallback( CellularHandle, cellularSocketHandle,
                                                                 prvCellularSocketDataReadyCallback, ( void * ) pCellularSocketContext );

        if( socketStatus != CELLULAR_SUCCESS )
        {
            IotLogError( "Failed to SocketRegisterDataReadyCallback. Socket status %d.", socketStatus );
            retRegCallback = SOCKETS_SOCKET_ERROR;
        }
    }

    if( retRegCallback == SOCKETS_ERROR_NONE )
    {
        socketStatus = Cellular_SocketRegisterSocketOpenCallback( CellularHandle, cellularSocketHandle,
                                                                  prvCellularSocketOpenCallback, ( void * ) pCellularSocketContext );

        if( socketStatus != CELLULAR_SUCCESS )
        {
            IotLogError( "Failed to SocketRegisterSocketOpenCallbac. Socket status %d.", socketStatus );
            retRegCallback = SOCKETS_SOCKET_ERROR;
        }
    }

    if( retRegCallback == SOCKETS_ERROR_NONE )
    {
        socketStatus = Cellular_SocketRegisterClosedCallback( CellularHandle, cellularSocketHandle,
                                                              prvCellularSocketClosedCallback, ( void * ) pCellularSocketContext );

        if( socketStatus != CELLULAR_SUCCESS )
        {
            IotLogError( "Failed to SocketRegisterClosedCallback. Socket status %d.", socketStatus );
            retRegCallback = SOCKETS_SOCKET_ERROR;
        }
    }

    return retRegCallback;
}

/*-----------------------------------------------------------*/

/*static*/ bool _calculateElapsedTime( uint64_t entryTimeMs,
                                   uint32_t timeoutValueMs,
                                   uint64_t * pElapsedTimeMs )
{
    uint64_t currentTimeMs = getTimeMs();
    bool isExpired = false;

    /* timeoutValueMs with UINT32_MAX_DELAY_MS means wait for ever, same behavior as freertos_plus_tcp. */
    if( timeoutValueMs == UINT32_MAX_DELAY_MS )
    {
        isExpired = false;
    }

    /* timeoutValueMs = 0 means none blocking mode. */
    else if( timeoutValueMs == 0U )
    {
        isExpired = true;
    }
    else
    {
        *pElapsedTimeMs = currentTimeMs - entryTimeMs;

        if( ( currentTimeMs - entryTimeMs ) >= timeoutValueMs )
        {
            isExpired = true;
        }
        else
        {
            isExpired = false;
        }
    }

    return isExpired;
}

/* added for assoicating sockid with appid -- BEGIN*/
typedef struct
{
    int sockfd;
    Ql_AppIndex_TypeDef app_id;
    Ql_SocketState_t sock_state;
}Ql_SockidApp_TypeDef;

Ql_SockidApp_TypeDef Ql_Sock_App[CELLULAR_NUM_SOCKET_MAX] = {0};

Ql_AppIndex_TypeDef Ql_GetAppFromSockid(uint8_t Sockfd)
{
    if(Sockfd < CELLULAR_NUM_SOCKET_MAX)
    {
        for(int i = 0; i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].sockfd == Sockfd)
            {
                IotLogDebug("sockfd[%d],Ql_Sock_App[%d].app_id=[%d]",Sockfd,i,Ql_Sock_App[i].app_id);
                return Ql_Sock_App[i].app_id;
            }
        }
    }

    IotLogError("Sock_App doesn't has member matched to sockfd[%d]",Sockfd);
    return APP_INVALID;
}

int Ql_GetSockidFromApp(Ql_AppIndex_TypeDef AppId)
{
    if((AppId < APP_MAX) && (AppId > APP_INVALID))
    {
        for(int i = 0; i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].app_id == AppId)
                return Ql_Sock_App[i].sockfd;
        }
    }

    LogError(("appID[%d] has no matched sockid",AppId));
    return QL_SOCKET_INVALID;
}

Ql_SocketState_t Ql_GetSockStateFromApp(Ql_AppIndex_TypeDef AppId)
{
    if((AppId < APP_MAX) && (AppId > APP_INVALID))
    {
        for(int i = 0; i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].app_id == AppId)
                return Ql_Sock_App[i].sock_state;
        }
    }

    return QL_SOCKETSTATE_INVALID;
}

int Ql_SetSockStateToApp(Ql_AppIndex_TypeDef AppId,Ql_SocketState_t sockState)
{
    if((AppId < APP_MAX) && (AppId > APP_INVALID))
    {
        for(int i = 0; i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].app_id == AppId)
            {
                Ql_Sock_App[i].sock_state = sockState;
                return 0;
            }
        }
    }

    IotLogError("appID[%d] in Sock_App doesn't exist",AppId);
    return -1;
}

int Ql_RegSockApp(Ql_AppIndex_TypeDef AppId)
{
    int i = 0;

    if((AppId < APP_MAX) && (AppId > APP_INVALID))
    {
        for(i = 0;i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].app_id == AppId)
            {
                IotLogError("appid[%d] is already alloted by Sock_App[%d]",AppId,i);

                return -1;
            }
        }

        for(i = 0;i < CELLULAR_NUM_SOCKET_MAX;i++)
        {
            if(Ql_Sock_App[i].app_id == APP_INVALID)
            {
                Ql_Sock_App[i].app_id = AppId;
                Ql_Sock_App[i].sockfd = QL_SOCKET_INVALID;
                Ql_Sock_App[i].sock_state = QL_SOCKETSTATE_INITIAL;

                IotLogDebug("Assign Sock_App[%d] to appid[%d]",i,Ql_Sock_App[i].app_id);

                return 0;
            }
        }
    }

    IotLogError("RegSockApp failed,AppId[%d]",AppId);
    for(i = 0;i < CELLULAR_NUM_SOCKET_MAX;i++)
    {
        if(Ql_Sock_App[i].app_id != APP_INVALID)
        {
            IotLogInfo("Sock_App[%d] is assined:appid[%d],sockfd[%d],sock_state[%d]",
                    i,Ql_Sock_App[i].app_id,Ql_Sock_App[i].sockfd,Ql_Sock_App[i].sock_state);
        }
    }
    return -1;
}

int Ql_AssociateSockidWithAppid(Socket_t SockWrapper,Ql_AppIndex_TypeDef AppId)
{
    unsigned int sockfd = SockWrapper->cellularSocketHandle->socketId;

    for(int i = 0;i < CELLULAR_NUM_SOCKET_MAX;i++)
    {
        if(Ql_Sock_App[i].app_id == AppId)
        {
            Ql_Sock_App[i].sockfd = sockfd;
            Ql_Sock_App[i].sock_state = QL_SOCKETSTATE_CONNECTED;

            return 0;
        }
    }

    IotLogError("Ql_Sock_App is full");
    return -1;
}

int Ql_FreeSockidWithAppid(Ql_AppIndex_TypeDef AppId)
{
    for(int i = 0;i < CELLULAR_NUM_SOCKET_MAX;i++)
    {
        if(Ql_Sock_App[i].app_id == AppId)
        {
            Ql_Sock_App[i].sockfd = QL_SOCKET_INVALID;
            Ql_Sock_App[i].app_id = APP_INVALID;

            return 0;
        }
    }

    IotLogError("Ql_Sock_App doesn't has member matched to AppId[%d] ",AppId);
    return -1;
}

void Ql_SendRecvIndToApp(Ql_AppIndex_TypeDef AppId)
{
    Ql_AppIndex_TypeDef app_id = AppId;

    switch (app_id)
    {
        default:
        {
            LogDebug(("The appid[%d] is invalid or Upper layer direct read",app_id));
        }
        break;
    }
}

void Ql_SendClosedIndToApp(Ql_AppIndex_TypeDef AppId)
{
    Ql_AppIndex_TypeDef app_id = AppId;

//    LogInfo(("appid[%d] is closed,notify upper layer",app_id));//sock_state[%d]

    /*When the connection is closed,let the upper layer handle first*/
    switch (app_id)
    {
#ifdef __EXAMPLE_NTRIP_CLIENT__
//        case APP_NTRIPCLIENT_NTRIPLOOP_STEP:
//        {
//            if(NULL != Ql_NtripClientEvent)
//            {
//                xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_RECONN);
//            }
//        }
//        break;
#endif
        default:
        {
//            LogInfo(("The appid[%d] is invalid or has no corresponding implementation",app_id));
        }
        break;
    }
}
/* added for assoicating sockid with appid -- END*/

void Ql_SendClosedRetToApp(Ql_AppIndex_TypeDef AppId)
{
    Ql_AppIndex_TypeDef app_id = AppId;

    LogDebug(("The appid[%d] is closed,upper layer handle the relevant scene ",app_id));

    /*When the connection is closed,let the upper layer handle first*/
    switch (app_id)
    {
#ifdef __EXAMPLE_NTRIP_CLIENT__
//        case APP_NTRIPCLIENT_NTRIPLOOP_STEP:
//        {
//            if(NULL != Ql_NtripClientEvent)
//            {
//                xEventGroupSetBits(Ql_NtripClientEvent, NTRIP_RTK_EVENT_CLOSE);
//            }
//        }
//        break;
#endif
        default:
        {
//            LogError(("The appid[%d] is invalid or has no corresponding implementation",app_id));
        }
        break;
    }
}

CellularError_t Cellular_Reboot(void);

extern bool setupCellular(void);

static QueueHandle_t Ql_TcpMultiConnectQueue = NULL;

typedef struct 
{
    int op;
    void * Args;
}Ql_TcpMultiConnQueue_TypeDef;

typedef struct
{
    int context_id;
}Ql_TcpPdpDeact_TypeDef;

typedef struct
{
    Ql_AppIndex_TypeDef appID;
    char host[BUFFSIZE64];
}Ql_TcpConnFailed_TypeDef;

typedef struct
{
    int sock_fd;
}Ql_TcpSockFd_TypeDef;

#define CELLULAR_MAX_SIM_RETRY                   ( 5U )

bool Ql_TCP_SendPdpDeactReq(int ContextId)
{
    BaseType_t status = pdTRUE;

    Ql_TcpPdpDeact_TypeDef *info = pvPortMalloc(sizeof(Ql_TcpPdpDeact_TypeDef));
    if(info == NULL)
    {
        LogError(("pvPortMalloc info failed"));
        return pdFALSE;
    }

    memset(info,0,sizeof(Ql_TcpPdpDeact_TypeDef));
    info->context_id = ContextId;

    Ql_TcpMultiConnQueue_TypeDef tcp_info_quene;
    tcp_info_quene.op = QL_TCP_QIURC_PDP_DEACT_IND;
    tcp_info_quene.Args = (void *)info;

    if(Ql_TcpMultiConnectQueue == NULL)
    {
        LogDebug(("TCP_MultiConn_Queue is NULL"));
        return pdFALSE;
    }

    status = xQueueSend(Ql_TcpMultiConnectQueue, (void *) &tcp_info_quene, 0);
    if(!status)
    {
        LogError(("send Ql_TcpMultiConnectQueue msg failed,err[%ld]",status));
        return false;
    }

    return true;
}

bool Ql_TCP_ConnFailedInd(Ql_AppIndex_TypeDef AppId,char * Host)
{
    BaseType_t status = pdTRUE;

    Ql_TcpConnFailed_TypeDef *info = pvPortMalloc(sizeof(Ql_TcpConnFailed_TypeDef));
    if(info == NULL)
    {
        LogError(("pvPortMalloc info failed"));
        return pdFALSE;
    }

    memset(info,0,sizeof(Ql_TcpConnFailed_TypeDef));
    info->appID = AppId;
    if(NULL != Host)
    {
        strncpy(info->host,Host,BUFFSIZE64);
    }

    Ql_TcpMultiConnQueue_TypeDef tcp_info_quene;
    tcp_info_quene.op = QL_TCP_QIURC_CONN_RETRY_MAX_FAILED_IND;
    tcp_info_quene.Args = (void *)info;

    if(Ql_TcpMultiConnectQueue == NULL)
    {
        LogError(("Ql_TcpMultiConnectQueue is NULL"));
        return pdFALSE;
    }

    status = xQueueSend(Ql_TcpMultiConnectQueue, (void *) &tcp_info_quene, 0);
    if(!status)
    {
        LogError(("send Ql_TcpMultiConnectQueue msg failed,err[%ld]",status));
        return false;
    }

    return true;
}

bool Ql_TCP_SendClosedReq(int SockFd)
{
    BaseType_t status = pdTRUE;

    Ql_TcpSockFd_TypeDef *info = pvPortMalloc(sizeof(Ql_TcpSockFd_TypeDef));
    if(info == NULL)
    {
        LogError(("pvPortMalloc info failed"));
        return pdFALSE;
    }

    memset(info,0,sizeof(Ql_TcpSockFd_TypeDef));
    info->sock_fd = SockFd;

    Ql_TcpMultiConnQueue_TypeDef tcp_info_quene;
    tcp_info_quene.op = QL_TCP_QIURC_CLOSED_CONNID_IND;
    tcp_info_quene.Args = (void *)info;

    if(Ql_TcpMultiConnectQueue == NULL)
    {
        LogError(("Ql_TcpMultiConnectQueue is NULL"));
        return pdFALSE;
    }

    status = xQueueSend(Ql_TcpMultiConnectQueue, (void *) &tcp_info_quene, 0);
    if(!status)
    {
        LogError(("send Ql_TcpMultiConnectQueue msg failed,err[%ld]",status));
        return false;
    }

    return true;
}

bool Ql_TCP_SendRecvReq(int SockFd)
{
    BaseType_t status = pdTRUE;

    Ql_TcpSockFd_TypeDef *info = pvPortMalloc(sizeof(Ql_TcpSockFd_TypeDef));
    if(info == NULL)
    {
        LogError(("pvPortMalloc info failed"));
        return pdFALSE;
    }

    memset(info,0,sizeof(Ql_TcpSockFd_TypeDef));
    info->sock_fd = SockFd;

    Ql_TcpMultiConnQueue_TypeDef tcp_info_quene;
    tcp_info_quene.op = QL_TCP_QIURC_RECV_CONNID_IND;
    tcp_info_quene.Args = (void *)info;

    if(Ql_TcpMultiConnectQueue == NULL)
    {
        LogError(("Ql_TcpMultiConnectQueue is NULL"));
        return pdFALSE;
    }

    status = xQueueSend(Ql_TcpMultiConnectQueue, (void *) &tcp_info_quene, 0 );
    if(!status)
    {
        LogError(("send Ql_TcpMultiConnectQueue msg failed,err[%ld]",status));
        return false;
    }

    return true;
}
#define CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS    ( 1000UL )

CellularError_t Ql_RescanCellNet(CellularHandle_t CellularHandle)
{
    CellularError_t cellular_status = CELLULAR_SUCCESS;
    CellularSimCardStatus_t sim_status;
    CellularServiceStatus_t service_status;
    //uint32_t timeoutCountLimit = ( CELLULAR_PDN_CONNECT_TIMEOUT / CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS ) + 1U;
    uint32_t timeout_count_limit = 30 + 1U;
    uint32_t timeout_count = 0;

    /* Rescan network. */
    cellular_status = Cellular_RfOff(CellularHandle);
    if(cellular_status == CELLULAR_SUCCESS)
    {
        cellular_status = Cellular_RfOn(CellularHandle);
    }

    if(cellular_status == CELLULAR_SUCCESS)
    {
        /* wait until SIM is ready */
        for(int i = 0; i < CELLULAR_MAX_SIM_RETRY; i++ )
        {
            cellular_status = Cellular_GetSimCardStatus(CellularHandle, &sim_status);
            if((cellular_status == CELLULAR_SUCCESS)
                && ((sim_status.simCardState == CELLULAR_SIM_CARD_INSERTED)
                    && (sim_status.simCardLockState == CELLULAR_SIM_CARD_READY)))
            {
                LogInfo(("sim status ok"));
                break;
            }
            else
            {
                LogError(("sim card state %d,lock state %d",sim_status.simCardState,sim_status.simCardLockState));
                cellular_status = CELLULAR_UNKNOWN;
            }
        }
    }

    if(cellular_status == CELLULAR_SUCCESS)
    {
        while(timeout_count < timeout_count_limit)
        {
            cellular_status = Cellular_GetServiceStatus(CellularHandle, &service_status);

            if((cellular_status == CELLULAR_SUCCESS)
                && ((service_status.psRegistrationStatus == REGISTRATION_STATUS_REGISTERED_HOME)
                    || (service_status.psRegistrationStatus == REGISTRATION_STATUS_ROAMING_REGISTERED)))
            {
                LogInfo(("Cell module registered"));
                break;
            }
            else
            {
                LogWarn(("GetServiceStatus ret:%d,reg_status :%d",
                          cellular_status, service_status.psRegistrationStatus));
            }

            timeout_count++;

            if(timeout_count >= timeout_count_limit)
            {
                cellular_status = CELLULAR_INVALID_HANDLE;
                LogError(("Cellular module can't be registered"));
            }
            else
            {
                LogInfo(("timeout_count[%d],timeout_count_limit[%d]",timeout_count,timeout_count_limit));
            }

            vTaskDelay(pdMS_TO_TICKS(CELLULAR_PDN_CONNECT_WAIT_INTERVAL_MS));
        }
    }

    if(cellular_status != CELLULAR_SUCCESS)
    {
        LogError(("try to reboot the cell"));

        cellular_status = Cellular_Reboot();

        vTaskDelay(pdMS_TO_TICKS( 5 * 1000 ));

        if(true == setupCellular())
        {
            cellular_status = CELLULAR_SUCCESS;
        }
        else
        {
            cellular_status = CELLULAR_UNKNOWN;
        }

        LogInfo(("after reboot & setupCellular"));
    }

    return cellular_status;
}

bool Ql_TCP_HandlePDP_Deact(uint8_t ContextId)
{
    bool cellularRet = true;
    uint8_t  context_id = ContextId;
    CellularError_t cellular_status = CELLULAR_SUCCESS;
    CellularPdnStatus_t pdn_status = {0};
    uint8_t num_status = 1;

    cellular_status = Ql_GetCellularActiveStatus();
    if(cellular_status == CELLULAR_SUCCESS)
    {
        //execute "AT+QIDEACT = <contextID>" first
        cellular_status = Cellular_DeactivatePdn(CellularHandle, context_id);
    }

//    if( cellular_status == CELLULAR_SUCCESS )
//    {
//        cellular_status = Cellular_ActivatePdn( CellularHandle,context_id);
//    }

    // Rescan network regardless of the result above
    cellular_status = Ql_RescanCellNet(CellularHandle);
    if( cellular_status == CELLULAR_SUCCESS )
    {
        Cellular_ActivatePdn(CellularHandle, CellularSocketPdnContextId);
    }

    if( cellular_status == CELLULAR_SUCCESS )
    {
        cellular_status = Cellular_GetPdnStatus(CellularHandle, &pdn_status, context_id, &num_status);
    }

    if((cellular_status == CELLULAR_SUCCESS) && (pdn_status.state == 1))
    {
        LogInfo(("GetPdn ok"));

        Ql_SystemPtr->CellularNetReg = true;
        cellularRet = true;
    }
    else
    {
        LogInfo(("GetPdn failed,status[%d],state[%d]",cellular_status,pdn_status.state));
        Ql_SystemPtr->CellularNetReg = false;
        cellularRet = false;
    }

    LogInfo(("cellularRet [%d]",cellularRet));
    return cellularRet;
}

void Ql_ReconnAllClosedApp(void)
{
    Ql_AppIndex_TypeDef app_id;

    for(int i = 0; i < CELLULAR_NUM_SOCKET_MAX;i++)
    {
        if((Ql_Sock_App[i].app_id < APP_MAX) && (Ql_Sock_App[i].app_id > APP_INVALID))
        {
            app_id = Ql_Sock_App[i].app_id;

            LogInfo(("app_id[%d] needs to reconnect",app_id));
            Ql_SendClosedIndToApp(app_id);
        }
    }
}

static void Ql_ManageMultiConnTask(void * Parameters)
{
    (void)Parameters;

    int sock_fd;
    int context_id;

    Ql_TcpMultiConnQueue_TypeDef recv_info;
    CellularError_t status = CELLULAR_SUCCESS;
    CellularPdnStatus_t pdn_status = { 0 };
    uint8_t num_status = 1;
    bool ret = true;
    Ql_TcpPdpDeact_TypeDef *pdp_info = NULL;
    Ql_TcpConnFailed_TypeDef *app_info = NULL;
    Ql_AppIndex_TypeDef app_id;
    Ql_TcpSockFd_TypeDef *socfd_info = NULL;
    char conn_host[BUFFSIZE64 + 1] = {0};

    for( ; ; )
    {
        if (pdTRUE != xQueueReceive(Ql_TcpMultiConnectQueue, ( void * ) &recv_info, portMAX_DELAY))
        {
            continue;
        }

        Ql_EC600U_Open();
        Ql_EC600U_Release();

        LogDebug(("op[%d]",recv_info.op));
        switch(recv_info.op)
        {
            case QL_TCP_QIURC_PDP_DEACT_IND:
            {
                /*all connections set up in pdp 1*/
                pdp_info = (Ql_TcpPdpDeact_TypeDef *)(recv_info.Args);
                context_id = pdp_info->context_id;
                vPortFree(pdp_info);
                pdp_info = NULL;

                Ql_SystemPtr->CellularNetReg = false;
                LogWarn(("context_id[%d] is deactived",context_id));
    
                //re_active pdp & register net
                ret = Ql_TCP_HandlePDP_Deact( context_id );
                if(ret == true)
                {
                    Ql_ReconnAllClosedApp();
                }
                else
                {
                    LogWarn(("workmode [%d],failed,retry again",Ql_SystemPtr->WorkMode));
                    Ql_TCP_SendPdpDeactReq(context_id);
                }
            }
            break;
            case QL_TCP_QIURC_CONN_RETRY_MAX_FAILED_IND:
            {
                /*If the connection fails to be opened for five consecutive times,
                deactivate the PDP Context, then re_activate the PDP Context and open the connection again*/

                app_info = (Ql_TcpConnFailed_TypeDef *)(recv_info.Args);
                app_id = app_info->appID;
                memset(conn_host,0,BUFFSIZE64 + 1);
                if(NULL != app_info->host)
                {
                    strncpy(conn_host,app_info->host,BUFFSIZE64);
                }

                vPortFree(app_info);
                app_info = NULL;
                LogInfo(("app_id[%d] connect failed",app_id));

                status = Cellular_GetPdnStatus(CellularHandle, &pdn_status, context_id, &num_status);
                if((status == CELLULAR_SUCCESS) && (pdn_status.state == 1))
                {
                    ret = true;
                }

                if(ret == true)
                {
                    LogInfo(("pdp context is normal at now, the link occurs error"));

                    status = Cellular_DeactivatePdn(CellularHandle, CellularSocketPdnContextId);
                    if( status == CELLULAR_SUCCESS )
                    {
                        //it causes other link be closed -- +QIURC: "closed",<connectID>
                        LogInfo(("the \"closed\" urc will trigger the remain steps for other links"));

                        Ql_SendClosedIndToApp(app_id);
                        break;
                    }
                    //if falied,go to remain steps
                }

                if((ret != true ) || (status != CELLULAR_SUCCESS))
                {
                    Ql_SystemPtr->CellularNetReg = false;
                    LogInfo(("GetPdn or deact pdn failed,cellularRet[%d],cellularStatus[%d]",ret,status));

                    //deactivate the PDP Context failed,rescan the network
                    status = Ql_RescanCellNet(CellularHandle);
                    if(status == CELLULAR_SUCCESS)
                    {
                        status = Cellular_GetPdnStatus(CellularHandle, &pdn_status, context_id, &num_status);
                    }

                    if((status == CELLULAR_SUCCESS) && (pdn_status.state == 1))
                    {
                        LogInfo(("Cellular module registered"));
                        ret = true;
                        Ql_SystemPtr->CellularNetReg = true;
                    }
                    else
                    {
                        ret = false;
                        Ql_SystemPtr->CellularNetReg = false;
                    }

                    if(ret == true)
                    {
                        Ql_ReconnAllClosedApp();
                    }
                    else
                    {
                        LogWarn(("retry again"));
                        Ql_TCP_ConnFailedInd(app_id,conn_host);
                    }
                }
            }
            break;
            case QL_TCP_QIURC_CLOSED_CONNID_IND:
            {
                socfd_info = (Ql_TcpSockFd_TypeDef *)(recv_info.Args);
                sock_fd = socfd_info->sock_fd;

                vPortFree(socfd_info);
                socfd_info = NULL;
                LogWarn(("sock_fd[%d] is closed by remote peer or network",sock_fd));

                app_id = Ql_GetAppFromSockid(sock_fd);

                Ql_SendClosedIndToApp(app_id);
            }
            break;
            case QL_TCP_QIURC_RECV_CONNID_IND:
            {
                socfd_info = (Ql_TcpSockFd_TypeDef *)(recv_info.Args);
                sock_fd = socfd_info->sock_fd;

                vPortFree(socfd_info);
                socfd_info = NULL;
                LogDebug(("sock_fd[%d] recv data",sock_fd));

                app_id = Ql_GetAppFromSockid(sock_fd);

                Ql_SendRecvIndToApp(app_id);
            }
            break;
            default:
            break;
        }
    }
}

void Ql_ManageMultiConnTaskCreate(void)
{
    static bool is_created = false;

    if(is_created == false)
    {
        Ql_TcpMultiConnectQueue = xQueueCreate(20, sizeof(Ql_TcpMultiConnQueue_TypeDef));
        if(Ql_TcpMultiConnectQueue != NULL)
        {
            xTaskCreate( Ql_ManageMultiConnTask,
                         "ManageMultiConn",
                         configMINIMAL_STACK_SIZE * 5,
                         NULL,
                         tskIDLE_PRIORITY + 12,
                         NULL );
        }
        else
        {
            LogError(("xQueueCreate Ql_TcpMultiConnectQueue failed"));
        }

        is_created = true;
    }
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_Connect( Socket_t * pTcpSocket,
                            const char * pHostName,
                            uint16_t port,
                            uint32_t receiveTimeoutMs,
                            uint32_t sendTimeoutMs )
{
    CellularSocketHandle_t cellularSocketHandle = NULL;
    cellularSocketWrapper_t * pCellularSocketContext = NULL;
    CellularError_t cellularSocketStatus = CELLULAR_INVALID_HANDLE;

    CellularSocketAddress_t serverAddress;// = { 0 };
    EventBits_t waitEventBits = 0;
    BaseType_t retConnect = SOCKETS_ERROR_NONE;
    const uint32_t defaultReceiveTimeoutMs = CELLULAR_SOCKET_RECV_TIMEOUT_MS;

    /* Create a new TCP socket. */
    cellularSocketStatus = Cellular_CreateSocket( CellularHandle,
                                                  CellularSocketPdnContextId,
                                                  CELLULAR_SOCKET_DOMAIN_AF_INET,
                                                  CELLULAR_SOCKET_TYPE_STREAM,
                                                  CELLULAR_SOCKET_PROTOCOL_TCP,
                                                  &cellularSocketHandle );

    if( cellularSocketStatus != CELLULAR_SUCCESS )
    {
        IotLogError( "Failed to create cellular sockets. %d", cellularSocketStatus );
        retConnect = SOCKETS_SOCKET_ERROR;
    }

    /* Allocate socket context. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        pCellularSocketContext = pvPortMalloc( sizeof( cellularSocketWrapper_t ) );

        if( pCellularSocketContext == NULL )
        {
            IotLogError( "Failed to allocate new socket context." );
            ( void ) Cellular_SocketClose( CellularHandle, cellularSocketHandle );
            retConnect = SOCKETS_ENOMEM;
        }
        else
        {
            /* Initialize all the members to sane values. */
            IotLogDebug( "Created CELLULAR Socket %p.", pCellularSocketContext );
            ( void ) memset( pCellularSocketContext, 0, sizeof( cellularSocketWrapper_t ) );
            pCellularSocketContext->cellularSocketHandle = cellularSocketHandle;
            pCellularSocketContext->ulFlags |= CELLULAR_SOCKET_OPEN_FLAG;
            pCellularSocketContext->socketEventGroupHandle = NULL;
        }
    }

    /* Allocate event group for callback function. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        pCellularSocketContext->socketEventGroupHandle = xEventGroupCreate();

        if( pCellularSocketContext->socketEventGroupHandle == NULL )
        {
            IotLogError( "Failed create cellular socket eventGroupHandle %p.", pCellularSocketContext );
            retConnect = SOCKETS_ENOMEM;
        }
    }

    /* Register cellular socket callback function. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        serverAddress.ipAddress.ipAddressType = CELLULAR_IP_ADDRESS_V4;
        strncpy( serverAddress.ipAddress.ipAddress, pHostName, CELLULAR_IP_ADDRESS_MAX_SIZE );
        serverAddress.port = port;

        IotLogDebug( "Ip address %s port %u\r\n", serverAddress.ipAddress.ipAddress, serverAddress.port );
        retConnect = prvCellularSocketRegisterCallback( cellularSocketHandle, pCellularSocketContext );
    }

    /* Setup cellular socket recv AT command default timeout. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        cellularSocketStatus = Cellular_SocketSetSockOpt( CellularHandle,
                                                          cellularSocketHandle,
                                                          CELLULAR_SOCKET_OPTION_LEVEL_TRANSPORT,
                                                          CELLULAR_SOCKET_OPTION_RECV_TIMEOUT,
                                                          ( const uint8_t * ) &defaultReceiveTimeoutMs,
                                                          sizeof( uint32_t ) );

        if( cellularSocketStatus != CELLULAR_SUCCESS )
        {
            IotLogError( "Failed to setup cellular AT command receive timeout %d.", cellularSocketStatus );
            retConnect = SOCKETS_SOCKET_ERROR;
        }
    }

    /* Setup cellular socket send/recv timeout. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        retConnect = prvSetupSocketSendTimeout( pCellularSocketContext, pdMS_TO_TICKS( sendTimeoutMs ) );
    }

    if( retConnect == SOCKETS_ERROR_NONE )
    {
        retConnect = prvSetupSocketRecvTimeout( pCellularSocketContext, pdMS_TO_TICKS( receiveTimeoutMs ) );
    }

    /* Cellular socket connect. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        ( void ) xEventGroupClearBits( pCellularSocketContext->socketEventGroupHandle,
                                       SOCKET_DATA_RECEIVED_CALLBACK_BIT | SOCKET_OPEN_FAILED_CALLBACK_BIT );
        cellularSocketStatus = Cellular_SocketConnect( CellularHandle, cellularSocketHandle, CELLULAR_SOCKET_ACCESS_MODE, &serverAddress );

        if( cellularSocketStatus != CELLULAR_SUCCESS )
        {
            IotLogError( "Failed to establish new connection. Socket status %d.", cellularSocketStatus );
            retConnect = SOCKETS_SOCKET_ERROR;
        }
    }

    /* Wait the socket connection. */
    if( retConnect == SOCKETS_ERROR_NONE )
    {
        waitEventBits = xEventGroupWaitBits( pCellularSocketContext->socketEventGroupHandle,
                                             SOCKET_OPEN_CALLBACK_BIT | SOCKET_OPEN_FAILED_CALLBACK_BIT,
                                             pdTRUE,
                                             pdFALSE,
                                             CELLULAR_SOCKET_OPEN_TIMEOUT_TICKS/*portMAX_DELAY*/ );

        if( waitEventBits != SOCKET_OPEN_CALLBACK_BIT )
        {
            IotLogError( "Socket connect timeout." );
            retConnect = SOCKETS_ENOTCONN;
        }
    }

    /* Cleanup the socket if any error. */
    if( retConnect != SOCKETS_ERROR_NONE )
    {
        if( cellularSocketHandle != NULL )
        {
            ( void ) Cellular_SocketClose( CellularHandle, cellularSocketHandle );
            ( void ) Cellular_SocketRegisterDataReadyCallback( CellularHandle, cellularSocketHandle, NULL, NULL );
            ( void ) Cellular_SocketRegisterSocketOpenCallback( CellularHandle, cellularSocketHandle, NULL, NULL );
            ( void ) Cellular_SocketRegisterClosedCallback( CellularHandle, cellularSocketHandle, NULL, NULL );

            if( pCellularSocketContext != NULL )
            {
                pCellularSocketContext->cellularSocketHandle = NULL;
            }
        }

        if( ( pCellularSocketContext != NULL ) && ( pCellularSocketContext->socketEventGroupHandle != NULL ) )
        {
            vEventGroupDelete( pCellularSocketContext->socketEventGroupHandle );
            pCellularSocketContext->socketEventGroupHandle = NULL;
        }

        if( pCellularSocketContext != NULL )
        {
            vPortFree( pCellularSocketContext );
            pCellularSocketContext = NULL;
        }
    }

    *pTcpSocket = pCellularSocketContext;

    return retConnect;
}

/*-----------------------------------------------------------*/

void Sockets_Disconnect( Socket_t xSocket )
{
    int32_t retClose = SOCKETS_ERROR_NONE;
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) xSocket;
    CellularSocketHandle_t cellularSocketHandle = NULL;
    //uint32_t recvLength = 0;
    //uint8_t buf[ 128 ] = { 0 };
    //CellularError_t cellularSocketStatus = CELLULAR_SUCCESS;

    /* xSocket need to be check against SOCKET_INVALID_SOCKET. */
    /* coverity[misra_c_2012_rule_11_4_violation] */
    if( ( pCellularSocketContext == NULL ) || ( xSocket == SOCKETS_INVALID_SOCKET ) )
    {
        IotLogError( "Invalid xSocket %p", pCellularSocketContext );
        retClose = SOCKETS_EINVAL;
    }
    else
    {
        cellularSocketHandle = pCellularSocketContext->cellularSocketHandle;
    }

    if( retClose == SOCKETS_ERROR_NONE )
    {
        if( cellularSocketHandle != NULL )
        {
//for avoiding report  closed urc again
#if 0
            /* Receive all the data before socket close. */
            do
            {
                recvLength = 0;
                cellularSocketStatus = Cellular_SocketRecv( CellularHandle, cellularSocketHandle, buf, 128, &recvLength );
                IotLogDebug( "%u bytes received in close", recvLength );
            } while( ( recvLength != 0 ) && ( cellularSocketStatus == CELLULAR_SUCCESS ) );
#endif

            LogDebug( ( "BEFORE Cellular_SocketClose") );

            /* Close sockets. */
            if( Cellular_SocketClose( CellularHandle, cellularSocketHandle ) != CELLULAR_SUCCESS )
            {
                IotLogWarn( "Failed to destroy connection." );
                retClose = SOCKETS_SOCKET_ERROR;
            }

            ( void ) Cellular_SocketRegisterDataReadyCallback( CellularHandle, cellularSocketHandle, NULL, NULL );
            ( void ) Cellular_SocketRegisterSocketOpenCallback( CellularHandle, cellularSocketHandle, NULL, NULL );
            ( void ) Cellular_SocketRegisterClosedCallback( CellularHandle, cellularSocketHandle, NULL, NULL );
            pCellularSocketContext->cellularSocketHandle = NULL;
        }

        if( pCellularSocketContext->socketEventGroupHandle != NULL )
        {
            vEventGroupDelete( pCellularSocketContext->socketEventGroupHandle );
            pCellularSocketContext->socketEventGroupHandle = NULL;
        }

        if(NULL != pCellularSocketContext)
        {
            vPortFree( pCellularSocketContext );
            pCellularSocketContext = NULL;
        }
    }

    IotLogDebug( "Sockets close exit with code %d", retClose );
}

/*-----------------------------------------------------------*/

int32_t Sockets_Recv( Socket_t xSocket,
                      void * pvBuffer,
                      size_t xBufferLength )
{
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) xSocket;
    uint8_t * buf = ( uint8_t * ) pvBuffer;
    BaseType_t retRecvLength = 0;

    if( pCellularSocketContext == NULL )
    {
        IotLogError( "Cellular prvNetworkRecv Invalid xSocket %p", pCellularSocketContext );
        retRecvLength = ( BaseType_t ) SOCKETS_EINVAL;
    }
    else if( ( ( pCellularSocketContext->ulFlags & CELLULAR_SOCKET_OPEN_FLAG ) == 0U ) ||
             ( ( pCellularSocketContext->ulFlags & CELLULAR_SOCKET_CONNECT_FLAG ) == 0U ) )
    {
        IotLogError( "Cellular prvNetworkRecv Invalid xSocket flag %p %u",
                     pCellularSocketContext, pCellularSocketContext->ulFlags );
        retRecvLength = ( BaseType_t ) SOCKETS_ENOTCONN;
    }
    else
    {
        retRecvLength = ( BaseType_t ) prvNetworkRecvCellular( pCellularSocketContext, buf, xBufferLength );
    }

    return retRecvLength;
}

/*-----------------------------------------------------------*/

/* This function sends the data until timeout or data is completely sent to server.
 * Send timeout unit is TickType_t. Any timeout value greater than UINT32_MAX_MS_TICKS
 * or portMAX_DELAY will be regarded as MAX delay. In this case, this function
 * will not return until all bytes of data are sent successfully or until an error occurs. */
int32_t Sockets_Send( Socket_t xSocket,
                      const void * pvBuffer,
                      size_t xDataLength )
{
    uint8_t * buf = ( uint8_t * ) pvBuffer;
    CellularSocketHandle_t cellularSocketHandle = NULL;
    BaseType_t retSendLength = 0;
    uint32_t sentLength = 0;
    CellularError_t socketStatus = CELLULAR_SUCCESS;
    cellularSocketWrapper_t * pCellularSocketContext = ( cellularSocketWrapper_t * ) xSocket;
    uint32_t bytesToSend = xDataLength;
    uint64_t entryTimeMs = getTimeMs();
    uint64_t elapsedTimeMs = 0;
    uint32_t sendTimeoutMs = 0;

    if( pCellularSocketContext == NULL )
    {
        IotLogError( "Cellular Sockets_Send Invalid xSocket %p", pCellularSocketContext );
        retSendLength = ( BaseType_t ) SOCKETS_SOCKET_ERROR;
    }
    else if( ( ( pCellularSocketContext->ulFlags & CELLULAR_SOCKET_OPEN_FLAG ) == 0U ) ||
             ( ( pCellularSocketContext->ulFlags & CELLULAR_SOCKET_CONNECT_FLAG ) == 0U ) )
    {
        IotLogError( "Cellular Sockets_Send Invalid xSocket flag %p 0x%08x",
                     pCellularSocketContext, pCellularSocketContext->ulFlags );
        retSendLength = ( BaseType_t ) SOCKETS_SOCKET_ERROR;
    }
    else
    {
        cellularSocketHandle = pCellularSocketContext->cellularSocketHandle;

        /* Convert ticks to ms delay. */
        if( ( pCellularSocketContext->sendTimeout >= UINT32_MAX_MS_TICKS ) || ( pCellularSocketContext->sendTimeout >= portMAX_DELAY ) )
        {
            /* Check if the ticks cause overflow. */
            sendTimeoutMs = UINT32_MAX_DELAY_MS;
        }
        else
        {
            sendTimeoutMs = TICKS_TO_MS( pCellularSocketContext->sendTimeout );
        }

        /* Loop sending data until data is sent completely or timeout. */
        while( bytesToSend > 0U )
        {
            socketStatus = Cellular_SocketSend( CellularHandle,
                                                cellularSocketHandle,
                                                &buf[ retSendLength ],
                                                bytesToSend,
                                                &sentLength );

            if( socketStatus == CELLULAR_SUCCESS )
            {
                retSendLength = retSendLength + ( BaseType_t ) sentLength;
                bytesToSend = bytesToSend - sentLength;
            }

            /* Check socket status or timeout break. */
            if( ( socketStatus != CELLULAR_SUCCESS ) ||
                ( _calculateElapsedTime( entryTimeMs, sendTimeoutMs, &elapsedTimeMs ) ) )
            {
                if( socketStatus == CELLULAR_SOCKET_CLOSED )
                {
                    /* Socket already closed. No data is sent. */
                    retSendLength = 0;
                }
                else if( socketStatus != CELLULAR_SUCCESS )
                {
                    retSendLength = ( BaseType_t ) SOCKETS_SOCKET_ERROR;
                }

                break;
            }
        }

        IotLogDebug( "Sockets_Send expect %d write %d", xDataLength, sentLength );
    }

    return retSendLength;
}

/*-----------------------------------------------------------*/

CellularError_t Cellular_QICFG_SetTcpRetranscfg(CellularHandle_t CellularHandle,
                                                                  int RetransCnt,
                                                                  int RetransPeriod)
{
    CellularContext_t * p_context = (CellularContext_t *)CellularHandle;
    CellularError_t cellular_status = CELLULAR_SUCCESS;
    CellularPktStatus_t pkt_status = CELLULAR_PKT_STATUS_OK;
    char cmdBuf[ CELLULAR_AT_CMD_MAX_SIZE ] = {'\0'};
    CellularAtReq_t at_req_qicfg =
    {
        cmdBuf,
        CELLULAR_AT_NO_RESULT,
        NULL,
        NULL,
        NULL,
        0,
    };

    /* Make sure the library is open. */
    cellular_status = _Cellular_CheckLibraryStatus(p_context);
    if(cellular_status != CELLULAR_SUCCESS)
    {
        LogError(("_Cellular_CheckLibraryStatus failed"));
    }
    else
    {
        ( void ) snprintf( cmdBuf, CELLULAR_AT_CMD_MAX_SIZE, "%s%d,%d",
            "AT+QICFG=\"tcp/retranscfg\",", RetransCnt,RetransPeriod );

        pkt_status = _Cellular_TimeoutAtcmdRequestWithCallback(p_context, at_req_qicfg, SOCKET_DISCONNECT_PACKET_REQ_TIMEOUT_MS);
        if(pkt_status != CELLULAR_PKT_STATUS_OK)
        {
            LogError(("Set tcp retranscfg failed, cmdBuf:%s, PktRet: %d", cmdBuf, pkt_status));
        }
    }

    return cellular_status;
}

int32_t Ql_SendTcpData(const TransportInterface_t * Transport,const uint8_t * Data,uint64_t DataLen)
{
    LogDebug(("enter,dataLen[%d]",DataLen));

    int32_t status = pdPASS;
    const uint8_t * p_index = Data;
    int32_t bytes_sent = 0;
    uint64_t bytes_remaining = DataLen;

    /* Loop until all data is sent. */
    while((bytes_remaining > 0UL) && (status != pdFAIL))
    {
        bytes_sent = Transport->send(Transport->pNetworkContext,
                                      p_index,
                                      bytes_remaining);

        /* bytes_sent less than zero is an error. */
        if(bytes_sent < 0)
        {
            LogError(("Failed to send data,TransportStatus=%ld",bytes_sent));
            status = pdFAIL;
        }
        else if(bytes_sent > 0)
        {
            bytes_remaining -= bytes_sent;
            p_index += bytes_sent;
            LogDebug(("Sent data over the transport: sent=%d, remaining=%lu, total_sent=%lu",
                        bytes_sent,bytes_remaining,(DataLen - bytes_remaining)));
        }
        else
        {
            /* No bytes were sent over the network. */
            LogWarn(("No bytes were sent over the network"));
            status = pdFAIL;
        }
    }

    return status;
}

int32_t Ql_RecvTcpData(const TransportInterface_t * Transport,uint8_t * Data,uint64_t DataLen)
{
    int32_t total_recv = 0;

    total_recv = Transport->recv( Transport->pNetworkContext,
                                        Data,// + totalReceived,
                                        DataLen);// - totalReceived );

    /* Transport receive errors are negative. */
    if(total_recv < 0)
    {
        LogError(("Failed to receive tcp data,err = %ld",total_recv));
    }
    else if(total_recv == 0)
    {
        LogDebug(("No data was read"));
    }

    return total_recv;
}


typedef struct
{
    int Rssi;
    int Ber;
}Ql_CsqInfo_TypeDef;

static CellularPktStatus_t Ql_Cellular_GetRssiByCSQ_Cb(CellularContext_t * Context,
                                                          const CellularATCommandResponse_t * AtResp,
                                                          void * Data,
                                                          uint16_t DataLen)
{
    CellularPktStatus_t pkt_status = CELLULAR_PKT_STATUS_OK;
    CellularATError_t at_status = CELLULAR_AT_SUCCESS;
    char * rsp_line = NULL;

    if(Context == NULL)
    {
        IotLogError("pContext is invalid");
        pkt_status = CELLULAR_PKT_STATUS_INVALID_HANDLE;
    }
    else if((AtResp == NULL) || (AtResp->pItm == NULL) || (AtResp->pItm->pLine == NULL))
    {
        IotLogError("Response is invalid");
        pkt_status = CELLULAR_PKT_STATUS_BAD_PARAM;
    }
    else if((Data == NULL) || (DataLen != (sizeof(Ql_CsqInfo_TypeDef))))
    {
        IotLogError("pData is invalid or dataLen is wrong,dataLen[%d]",DataLen);
        pkt_status = CELLULAR_PKT_STATUS_BAD_PARAM;
    }
    else
    {
        rsp_line = AtResp->pItm->pLine;
        at_status = Cellular_ATRemoveAllWhiteSpaces(rsp_line);

        if(at_status == CELLULAR_AT_SUCCESS)
        {
            at_status = Cellular_ATRemovePrefix(&rsp_line);
        }

        if(at_status == CELLULAR_AT_SUCCESS)
        {
            if(strlen(rsp_line) < (sizeof(Ql_CsqInfo_TypeDef) + 1U))
            {
                Ql_CsqInfo_TypeDef * ptr = (Ql_CsqInfo_TypeDef *)Data;
                char * token;
                char *save_str;

                token = strtok_r(rsp_line,",",&save_str);
                ptr->Rssi = atoi(token);

                token = strtok_r(NULL,",",&save_str);
                ptr->Ber = atoi(token);
            }
            else
            {
                at_status = CELLULAR_AT_BAD_PARAMETER;
            }
        }

        pkt_status =_Cellular_TranslateAtCoreStatus(at_status);
    }

    return pkt_status;
}

CellularError_t Cellular_GetRssiByCSQ(CellularHandle_t CellularHandle,uint32_t * Rssi)
{
    CellularContext_t * p_context = (CellularContext_t *)CellularHandle;
    CellularError_t cellular_status = CELLULAR_SUCCESS;
    CellularPktStatus_t pkt_status = CELLULAR_PKT_STATUS_OK;
    Ql_CsqInfo_TypeDef get_info = {0};

    CellularAtReq_t at_req_csq =
    {
        "AT+CSQ",
        CELLULAR_AT_WITH_PREFIX,
        "+CSQ",
        Ql_Cellular_GetRssiByCSQ_Cb,
        &get_info,
        sizeof(Ql_CsqInfo_TypeDef),
    };

    cellular_status = _Cellular_CheckLibraryStatus(p_context);
    if(cellular_status != CELLULAR_SUCCESS)
    {
        IotLogError("_Cellular_CheckLibraryStatus failed");
    }
    else
    {
        pkt_status = _Cellular_AtcmdRequestWithCallback(p_context, at_req_csq);
        if(pkt_status != CELLULAR_PKT_STATUS_OK)
        {
            cellular_status = _Cellular_TranslatePktStatus(pkt_status);
            IotLogError("GetRssiByCSQ failed,err[%d]",cellular_status);
        }
        else
        {
            IotLogDebug("rssi:%d, ber:%d",get_info.Rssi,get_info.Ber);

            *Rssi = get_info.Rssi;
        }
    }

    return cellular_status;
}
#ifdef __EXAMPLE_NTRIP_CLIENT__
extern TaskHandle_t Ql_Cellular_TaskHandle;
#endif
extern CellularError_t sendAtCommandWithRetryTimeout( CellularContext_t * pContext,
                                                            const CellularAtReq_t * pAtReq );

CellularError_t Ql_GetCellularActiveStatus(void)
{
    CellularContext_t * pContext = CellularHandle;

    CellularError_t cellular_status = CELLULAR_UNKNOWN;
    CellularAtReq_t at_req_get_no_result =
    {
        NULL,
        CELLULAR_AT_NO_RESULT,
        NULL,
        NULL,
        NULL,
        0
    };

    if( pContext != NULL )
    {
        LogDebug(("send \"AT\" to check the cellular module"));
        at_req_get_no_result.pAtCmd = "AT";
        cellular_status = sendAtCommandWithRetryTimeout(pContext, &at_req_get_no_result);

        if( cellular_status == CELLULAR_SUCCESS )
        {
        }
        else
        {
            Ql_SystemPtr->CellularNetReg = false;
#ifdef __EXAMPLE_NTRIP_CLIENT__
            vTaskResume(Ql_Cellular_TaskHandle);
#endif
            LogError(("the cellular module is not active"));
        }
    }

    return cellular_status;
}
