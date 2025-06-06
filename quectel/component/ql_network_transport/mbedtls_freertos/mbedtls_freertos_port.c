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

/**
 * @file mbedtls_freertos_port.c
 * @brief Implements mbed TLS platform functions for FreeRTOS.
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
//#include "FreeRTOS_Sockets.h"

/* mbed TLS includes. */
#include "mbedtls_config.h"
#include "threading_alt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"

/* added to avoiding compile warning for memset() and uxRand()  --BEGIN*/
#include "string.h"
#include "ql_trng.h"
/* added to avoiding compile warning for memset() and uxRand()  --END*/

/*-----------------------------------------------------------*/

/**
 * @brief The number of bytes in a standard TLS record header.
 *
 * @note This only applies to a standard TCP+TLS connection.
 *       DTLS has a different length for its record headers.
 *
 */
#define TLS_RECORD_HEADER_BYTE_LENGTH    5

/*-----------------------------------------------------------*/

/**
 * @brief Allocates memory for an array of members.
 *
 * @param[in] nmemb Number of members that need to be allocated.
 * @param[in] size Size of each member.
 *
 * @return Pointer to the beginning of newly allocated memory.
 */
void * mbedtls_platform_calloc( size_t nmemb,
                                size_t size )
{
    size_t totalSize = nmemb * size;
    void * pBuffer = NULL;

    /* Check that neither nmemb nor size were 0. */
    if( totalSize > 0 )
    {
        /* Overflow check. */
        if( ( totalSize / size ) == nmemb )
        {
            pBuffer = pvPortMalloc( totalSize );

            if( pBuffer != NULL )
            {
                ( void ) memset( pBuffer, 0x00, totalSize );
            }
        }
    }

    return pBuffer;
}

/*-----------------------------------------------------------*/

/**
 * @brief Frees the space previously allocated by calloc.
 *
 * @param[in] ptr Pointer to the memory to be freed.
 */
void mbedtls_platform_free( void * ptr )
{
    vPortFree( ptr );
}

/*-----------------------------------------------------------*/

/**
 * @brief Creates a mutex.
 *
 * @param[in, out] pMutex mbedtls mutex handle.
 */
void mbedtls_platform_mutex_init( mbedtls_threading_mutex_t * pMutex )
{
    configASSERT( pMutex != NULL );

    /* Create a statically-allocated FreeRTOS mutex. This should never fail as
     * storage is provided. */
    pMutex->mutexHandle = xSemaphoreCreateMutexStatic( &( pMutex->mutexStorage ) );
    configASSERT( pMutex->mutexHandle != NULL );
}

/*-----------------------------------------------------------*/

/**
 * @brief Frees a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @note This function is an empty stub as nothing needs to be done to free
 * a statically allocated FreeRTOS mutex.
 */
void mbedtls_platform_mutex_free( mbedtls_threading_mutex_t * pMutex )
{
    /* Nothing needs to be done to free a statically-allocated FreeRTOS mutex. */
    ( void ) pMutex;
}

/*-----------------------------------------------------------*/

/**
 * @brief Function to lock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 (success) is always returned as any other failure is asserted.
 */
int mbedtls_platform_mutex_lock( mbedtls_threading_mutex_t * pMutex )
{
    BaseType_t mutexStatus = 0;

    configASSERT( pMutex != NULL );

    /* mutexStatus is not used if asserts are disabled. */
    ( void ) mutexStatus;

    /* This function should never fail if the mutex is initialized. */
    mutexStatus = xSemaphoreTake( pMutex->mutexHandle, portMAX_DELAY );
    configASSERT( mutexStatus == pdTRUE );

    return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Function to unlock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 is always returned as any other failure is asserted.
 */
int mbedtls_platform_mutex_unlock( mbedtls_threading_mutex_t * pMutex )
{
    BaseType_t mutexStatus = 0;

    configASSERT( pMutex != NULL );
    /* mutexStatus is not used if asserts are disabled. */
    ( void ) mutexStatus;

    /* This function should never fail if the mutex is initialized. */
    mutexStatus = xSemaphoreGive( pMutex->mutexHandle );
    configASSERT( mutexStatus == pdTRUE );

    return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Function to generate a random number.
 *
 * @param[in] data Callback context.
 * @param[out] output The address of the buffer that receives the random number.
 * @param[in] len Maximum size of the random number to be generated.
 * @param[out] olen The size, in bytes, of the #output buffer.
 *
 * @return 0 if no critical failures occurred,
 * MBEDTLS_ERR_ENTROPY_SOURCE_FAILED otherwise.
 */
int mbedtls_platform_entropy_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen )
{
    int status = 0;
//    NTSTATUS rngStatus = 0;

    configASSERT( output != NULL );
    configASSERT( olen != NULL );

    /* Context is not used by this function. */
    ( void ) data;

    *output = uxRand();
    *olen = sizeof(uint32_t);
//    
//    /* TLS requires a secure random number generator; use the RNG provided
//     * by Windows. This function MUST be re-implemented for other platforms. */
//    rngStatus =
//        BCryptGenRandom( NULL, output, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG );

//    if( rngStatus == 0 )
//    {
//        /* All random bytes generated. */
//        *olen = len;
//    }
//    else
//    {
//        /* RNG failure. */
//        *olen = 0;
//        status = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
//    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Function to generate a random number based on a hardware poll.
 *
 * For this FreeRTOS Windows port, this function is redirected by calling
 * #mbedtls_platform_entropy_poll.
 *
 * @param[in] data Callback context.
 * @param[out] output The address of the buffer that receives the random number.
 * @param[in] len Maximum size of the random number to be generated.
 * @param[out] olen The size, in bytes, of the #output buffer.
 *
 * @return 0 if no critical failures occurred,
 * MBEDTLS_ERR_ENTROPY_SOURCE_FAILED otherwise.
 */
int mbedtls_hardware_poll( void * data,
                           unsigned char * output,
                           size_t len,
                           size_t * olen )
{
    return mbedtls_platform_entropy_poll( data, output, len, olen );
}

/*-----------------------------------------------------------*/
