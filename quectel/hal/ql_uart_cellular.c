
#include <string.h>
#include "cellular_platform.h"
#include "cellular_config.h"
#include "cellular_config_defaults.h"
#include "cellular_comm_interface.h"
#include "ql_cellular_dev.h"

/* Comm status. */
#define CELLULAR_COMM_OPEN_BIT               ( 0x01U )

/* Comm task event. */
#define COMMTASK_EVT_MASK_STARTED            ( 0x0001UL )
#define COMMTASK_EVT_MASK_ABORT              ( 0x0002UL )
#define COMMTASK_EVT_MASK_ABORTED            ( 0x0004UL )
#define COMMTASK_EVT_MASK_ALL_EVENTS \
    ( COMMTASK_EVT_MASK_STARTED      \
      | COMMTASK_EVT_MASK_ABORT      \
      | COMMTASK_EVT_MASK_ABORTED )
#define COMMTASK_POLLING_TIME_MS             ( 1000UL )

/* Platform thread stack size and priority. */
#define COMM_IF_THREAD_DEFAULT_STACK_SIZE    ( 2048U )
#define COMM_IF_THREAD_DEFAULT_PRIORITY      ( tskIDLE_PRIORITY + 8U )

#define FALSE               0
#define TRUE                1

typedef void           *HANDLE;
typedef int             BOOL;

typedef struct _cellularCommContext
{
    CellularCommInterfaceReceiveCallback_t commReceiveCallback;
    HANDLE commReceiveCallbackThread;
    uint8_t commStatus;
    void * pUserData;
    HANDLE commFileHandle;
    CellularCommInterface_t * pCommInterface;
    bool commTaskThreadStarted;
    EventGroupHandle_t pCommTaskEvent;
} _cellularCommContext_t;

/*-----------------------------------------------------------*/

/**
 * @brief CellularCommInterfaceOpen_t implementation.
 */
static CellularCommInterfaceError_t _prvCommIntfOpen( CellularCommInterfaceReceiveCallback_t receiveCallback,
                                                      void * pUserData,
                                                      CellularCommInterfaceHandle_t * pCommInterfaceHandle );

/**
 * @brief CellularCommInterfaceSend_t implementation.
 */
static CellularCommInterfaceError_t _prvCommIntfSend( CellularCommInterfaceHandle_t commInterfaceHandle,
                                                      const uint8_t * pData,
                                                      uint32_t dataLength,
                                                      uint32_t timeoutMilliseconds,
                                                      uint32_t * pDataSentLength );

/**
 * @brief CellularCommInterfaceRecv_t implementation.
 */
static CellularCommInterfaceError_t _prvCommIntfReceive( CellularCommInterfaceHandle_t commInterfaceHandle,
                                                         uint8_t * pBuffer,
                                                         uint32_t bufferLength,
                                                         uint32_t timeoutMilliseconds,
                                                         uint32_t * pDataReceivedLength );

/**
 * @brief CellularCommInterfaceClose_t implementation.
 */
static CellularCommInterfaceError_t _prvCommIntfClose( CellularCommInterfaceHandle_t commInterfaceHandle );

/**
 * @brief Get default comm interface context.
 *
 * @return On success, SOCKETS_ERROR_NONE is returned. If an error occurred, error code defined
 * in sockets_wrapper.h is returned.
 */
static _cellularCommContext_t * _getCellularCommContext( void );

/**
 * @brief Thread routine to generate simulated interrupt.
 *
 * @param[in] pUserData Pointer to _cellularCommContext_t allocated in comm interface open.
 */
static void commTaskThread( void * pUserData );

/**
 * @brief Helper function to setup and create commTaskThread.
 *
 * @param[in] pCellularCommContext Cellular comm interface context allocated in open.
 *
 * @return On success, IOT_COMM_INTERFACE_SUCCESS is returned. If an error occurred, error code defined
 * in CellularCommInterfaceError_t is returned.
 */
static CellularCommInterfaceError_t setupCommTaskThread( _cellularCommContext_t * pCellularCommContext );

/**
 * @brief Helper function to clean commTaskThread.
 *
 * @param[in] pCellularCommContext Cellular comm interface context allocated in open.
 *
 * @return On success, IOT_COMM_INTERFACE_SUCCESS is returned. If an error occurred, error code defined
 * in CellularCommInterfaceError_t is returned.
 */
static CellularCommInterfaceError_t cleanCommTaskThread( _cellularCommContext_t * pCellularCommContext );

/*-----------------------------------------------------------*/

CellularCommInterface_t CellularCommInterface =
{
    .open  = _prvCommIntfOpen,
    .send  = _prvCommIntfSend,
    .recv  = _prvCommIntfReceive,
    .close = _prvCommIntfClose
};

static _cellularCommContext_t _iotCellularCommContext =
{
    .commReceiveCallback       = NULL,
    .commReceiveCallbackThread = NULL,
    .pCommInterface            = &CellularCommInterface,
    .commFileHandle            = NULL,
    .pUserData                 = NULL,
    .commStatus                = 0U,
    .commTaskThreadStarted     = false,
    .pCommTaskEvent            = NULL
};

/*-----------------------------------------------------------*/

static _cellularCommContext_t * _getCellularCommContext( void )
{
    return &_iotCellularCommContext;
}

/*-----------------------------------------------------------*/

uint32_t prvProcessUartInt( void )
{
    _cellularCommContext_t * pCellularCommContext = _getCellularCommContext();
    CellularCommInterfaceError_t callbackRet = IOT_COMM_INTERFACE_FAILURE;
    uint32_t retUartInt = pdTRUE;

    if( pCellularCommContext->commReceiveCallback != NULL )
    {
        callbackRet = pCellularCommContext->commReceiveCallback( pCellularCommContext->pUserData,
                                                                 ( CellularCommInterfaceHandle_t ) pCellularCommContext );
    }

    if( callbackRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        retUartInt = pdTRUE;
    }
    else
    {
        retUartInt = pdFALSE;
    }

    return retUartInt;
}

/*-----------------------------------------------------------*/

static void commTaskThread( void * pUserData )
{
    _cellularCommContext_t * pCellularCommContext = ( _cellularCommContext_t * ) pUserData;
//    EventBits_t uxBits = 0;

    /* Inform thread ready. */
    LogInfo( ( "Cellular commTaskThread started" ) );

    if( pCellularCommContext != NULL )
    {
        ( void ) xEventGroupSetBits( pCellularCommContext->pCommTaskEvent,
                                     COMMTASK_EVT_MASK_STARTED );
    }

    vTaskDelete( NULL );
#if 0
    while( true )
    {
        /* Wait for notification from eventqueue. */
        uxBits = xEventGroupWaitBits( ( pCellularCommContext->pCommTaskEvent ),
                                      ( ( EventBits_t ) COMMTASK_EVT_MASK_ABORT ),
                                      pdTRUE,
                                      pdFALSE,
                                      pdMS_TO_TICKS( COMMTASK_POLLING_TIME_MS ) );

        if( ( uxBits & ( EventBits_t ) COMMTASK_EVT_MASK_ABORT ) != 0U )
        {
            LogDebug( ( "Abort received, cleaning up!" ) );
            break;
        }
        else
        {
            /* Polling the global share variable to trigger the interrupt. */
            if( rxEvent == true )
            {
                rxEvent = false;
            }
        }
    }

    /* Inform thread ready. */
    if( pCellularCommContext != NULL )
    {
        ( void ) xEventGroupSetBits( pCellularCommContext->pCommTaskEvent, COMMTASK_EVT_MASK_ABORTED );
    }

    LogInfo( ( "Cellular commTaskThread exit" ) );
#endif
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t setupCommTaskThread( _cellularCommContext_t * pCellularCommContext )
{
    BOOL Status = TRUE;
    EventBits_t uxBits = 0;
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;

    pCellularCommContext->pCommTaskEvent = xEventGroupCreate();

    if( pCellularCommContext->pCommTaskEvent != NULL )
    {
        /* Create the FreeRTOS thread to generate the simulated interrupt. */
        Status = Platform_CreateDetachedThread( commTaskThread,
                                                ( void * ) pCellularCommContext,
                                                COMM_IF_THREAD_DEFAULT_PRIORITY,
                                                COMM_IF_THREAD_DEFAULT_STACK_SIZE );

        if( Status != true )
        {
            commIntRet = IOT_COMM_INTERFACE_FAILURE;
        }
    }
    else
    {
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }

    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        uxBits = xEventGroupWaitBits( ( pCellularCommContext->pCommTaskEvent ),
                                      ( ( EventBits_t ) COMMTASK_EVT_MASK_STARTED | ( EventBits_t ) COMMTASK_EVT_MASK_ABORTED ),
                                      pdTRUE,
                                      pdFALSE,
                                      portMAX_DELAY );

        if( ( uxBits & ( EventBits_t ) COMMTASK_EVT_MASK_STARTED ) == COMMTASK_EVT_MASK_STARTED )
        {
            pCellularCommContext->commTaskThreadStarted = true;
        }
        else
        {
            commIntRet = IOT_COMM_INTERFACE_FAILURE;
            pCellularCommContext->commTaskThreadStarted = false;
        }
    }

    return commIntRet;
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t cleanCommTaskThread( _cellularCommContext_t * pCellularCommContext )
{
    EventBits_t uxBits = 0;
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;

    /* Wait for the commTaskThreadStarted exit. */
    if( ( pCellularCommContext->commTaskThreadStarted == true ) && ( pCellularCommContext->pCommTaskEvent != NULL ) )
    {
        ( void ) xEventGroupSetBits( pCellularCommContext->pCommTaskEvent,
                                     COMMTASK_EVT_MASK_ABORT );
        uxBits = xEventGroupWaitBits( ( pCellularCommContext->pCommTaskEvent ),
                                      ( ( EventBits_t ) COMMTASK_EVT_MASK_ABORTED ),
                                      pdTRUE,
                                      pdFALSE,
                                      portMAX_DELAY );

        if( ( uxBits & ( EventBits_t ) COMMTASK_EVT_MASK_ABORTED ) != COMMTASK_EVT_MASK_ABORTED )
        {
            LogDebug( ( "Cellular close wait commTaskThread fail" ) );
            commIntRet = IOT_COMM_INTERFACE_FAILURE;
        }

        pCellularCommContext->commTaskThreadStarted = false;
    }

    /* Clean the event group. */
    if( pCellularCommContext->pCommTaskEvent != NULL )
    {
        vEventGroupDelete( pCellularCommContext->pCommTaskEvent );
        pCellularCommContext->pCommTaskEvent = NULL;
    }

    return commIntRet;
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t _prvCommIntfOpen( CellularCommInterfaceReceiveCallback_t receiveCallback,
                                                      void * pUserData,
                                                      CellularCommInterfaceHandle_t * pCommInterfaceHandle )
{
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;
    _cellularCommContext_t * pCellularCommContext = _getCellularCommContext();
    
    if( pCellularCommContext == NULL )
    {
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }
    else if( ( pCellularCommContext->commStatus & CELLULAR_COMM_OPEN_BIT ) != 0 )
    {
        LogError( ( "Cellular comm interface opened already" ) );
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }
    else
    {
        /* Clear the context. */
        memset( pCellularCommContext, 0, sizeof( _cellularCommContext_t ) );
        pCellularCommContext->pCommInterface = &CellularCommInterface;
    }
    
    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        Ql_EC600U_PowerOff();
        Ql_EC600U_PowerOn();
        Ql_EC600U_Init();
    }

    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        pCellularCommContext->commReceiveCallback = receiveCallback;
        commIntRet = setupCommTaskThread( pCellularCommContext );
    }

    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        pCellularCommContext->pUserData = pUserData;
        *pCommInterfaceHandle = ( CellularCommInterfaceHandle_t ) pCellularCommContext;
        pCellularCommContext->commStatus |= CELLULAR_COMM_OPEN_BIT;
    }

    return commIntRet;
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t _prvCommIntfClose( CellularCommInterfaceHandle_t commInterfaceHandle )
{
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;
    _cellularCommContext_t * pCellularCommContext = ( _cellularCommContext_t * ) commInterfaceHandle;

    if( pCellularCommContext == NULL )
    {
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }
    else if( ( pCellularCommContext->commStatus & CELLULAR_COMM_OPEN_BIT ) == 0 )
    {
        LogError( ( "Cellular close comm interface is not opened before." ) );
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }
    else
    {
        /* clean the receive callback. */
        pCellularCommContext->commReceiveCallback = NULL;

        Ql_EC600U_PowerOff();
        Ql_EC600U_DeInit();

        pCellularCommContext->commFileHandle = NULL;

        pCellularCommContext->commReceiveCallbackThread = NULL;

        /* Clean the commTaskThread. */
        ( void ) cleanCommTaskThread( pCellularCommContext );

        /* clean the data structure. */
        pCellularCommContext->commStatus &= ~( CELLULAR_COMM_OPEN_BIT );
    }

    return commIntRet;
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t _prvCommIntfSend( CellularCommInterfaceHandle_t commInterfaceHandle,
                                                      const uint8_t * pData,
                                                      uint32_t dataLength,
                                                      uint32_t timeoutMilliseconds,
                                                      uint32_t * pDataSentLength )
{
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;
    _cellularCommContext_t * pCellularCommContext = ( _cellularCommContext_t * ) commInterfaceHandle;

    if( pCellularCommContext == NULL )
    {
        commIntRet = IOT_COMM_INTERFACE_BAD_PARAMETER;
    }
    else if( ( pCellularCommContext->commStatus & CELLULAR_COMM_OPEN_BIT ) == 0 )
    {
        LogError( ( "Cellular send comm interface is not opened before." ) );
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }

    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        Ql_EC600U_Write(pData, dataLength, timeoutMilliseconds);
        *pDataSentLength = ( uint32_t ) dataLength;
    }

    return commIntRet;
}

/*-----------------------------------------------------------*/

static CellularCommInterfaceError_t _prvCommIntfReceive( CellularCommInterfaceHandle_t commInterfaceHandle,
                                                         uint8_t * pBuffer,
                                                         uint32_t bufferLength,
                                                         uint32_t timeoutMilliseconds,
                                                         uint32_t * pDataReceivedLength )
{
    CellularCommInterfaceError_t commIntRet = IOT_COMM_INTERFACE_SUCCESS;
    _cellularCommContext_t * pCellularCommContext = ( _cellularCommContext_t * ) commInterfaceHandle;

    if( pCellularCommContext == NULL )
    {
        commIntRet = IOT_COMM_INTERFACE_BAD_PARAMETER;
    }
    else if( ( pCellularCommContext->commStatus & CELLULAR_COMM_OPEN_BIT ) == 0 )
    {
        LogError( ( "Cellular read comm interface is not opened before." ) );
        commIntRet = IOT_COMM_INTERFACE_FAILURE;
    }

    if( commIntRet == IOT_COMM_INTERFACE_SUCCESS )
    {
        bufferLength = Ql_EC600U_Read(pBuffer, bufferLength, timeoutMilliseconds);
        *pDataReceivedLength = ( uint32_t ) bufferLength;
    }

    return commIntRet;
}
/*-----------------------------------------------------------*/
