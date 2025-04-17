/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2023
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ql_error.h 
 *
 * Project:
 * --------
 *   Qntrip
 *
 * Description:
 * ------------
 *   ERROR code defines.
 *
 * Author:Hayden.Wang
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifndef __QOCPU_ERROR_H__
#define __QOCPU_ERROR_H__


/****************************************************************************
 * Error Code Definition
 ***************************************************************************/

typedef enum {
    QL_NTRIP_OK = 0,
    // 0x 01111111        11111111 11111111 11111111
    //    manufacturer    err code(max 16777215)
    // general process error
    QL_NTRIP_ERR_PARAM                = 1,
    QL_NTRIP_ERR_INIT                 = 2,
    QL_NTRIP_ERR_NO_INIT              = 3,
    QL_NTRIP_ERR_RE_INIT              = 4,
    QL_NTRIP_ERR_USED                 = 5,
    QL_NTRIP_ERR_BUSY                 = 6,
    QL_NTRIP_ERR_PROGRESS             = 7,
    QL_NTRIP_ERR_NOT_SUPPORT          = 8,
    QL_NTRIP_ERR_BUFFER_SIZE          = 9,
    QL_NTRIP_ERR_PARAM_UNCONFIG       = 10,
    QL_NTRIP_ERR_JSON                 = 11,

    // system error
    QL_NTRIP_ERR_MEMORY               = 100,
    QL_NTRIP_ERR_OPEN                 = 101,
    QL_NTRIP_ERR_READ                 = 102,
    QL_NTRIP_ERR_WRITE                = 103,
    QL_NTRIP_ERR_CLOSE                = 104,
    QL_NTRIP_ERR_TIMEOUT              = 105,
    QL_NTRIP_ERR_PTHREAD              = 106,


	QL_NTRIP_ERR_BS_FAIL              =200,

    // quectl
    QL_NTRIP_ERR_QL_BASE              = 0x01000000,
    QL_NTRIP_ERR_QL_PARAM             = 0x01001389,
    QL_NTRIP_ERR_QL_KEY               = 0x0100138B,
    QL_NTRIP_ERR_QL_URL               = 0x0100138C,
    QL_NTRIP_ERR_QL_FIX_COUNT         = 0x0100138D,
    QL_NTRIP_ERR_QL_TIME_POOL         = 0x0100138E,
    QL_NTRIP_ERR_QL_DUE               = 0x0100138F,
    QL_NTRIP_ERR_QL_DATA_PARSE        = 0x01013880,
    QL_NTRIP_ERR_QL_ACCOUNT_MATCH     = 0x01013881,
    QL_NTRIP_ERR_QL_DEACTIVATE        = 0x01013882,
    QL_NTRIP_ERR_QL_ABOVE_QUOTA       = 0x01013883,

    QL_NTRIP_ERR_VENDOR1_BASE                           = 0x02000000,
    QL_NTRIP_ERR_VENDOR1_OUT_OF_SERVICE_AREA            = 0x020003E9,
    QL_NTRIP_ERR_VENDOR1_INVALID_GGA                    = 0x020003EA,
    QL_NTRIP_ERR_VENDOR1_UNDEFINED                      = 0x020007CF,
    QL_NTRIP_ERR_VENDOR1_CAP_START_SUCCESS              = 0x020007D0,
    QL_NTRIP_ERR_VENDOR1_AUTH_SUCCESS                   = 0x020007D1,
    QL_NTRIP_ERR_VENDOR1_FREQ_ERROR                     = 0x020007D2,
    QL_NTRIP_ERR_VENDOR1_NETWORK_ERROR                  = 0x02000BB9,
    QL_NTRIP_ERR_VENDOR1_ACCT_ERROR                     = 0x02000BBA,
    QL_NTRIP_ERR_VENDOR1_ACCT_EXPIRED                   = 0x02000BBB,
    QL_NTRIP_ERR_VENDOR1_ACCT_NEED_BIND                 = 0x02000BBC,
    QL_NTRIP_ERR_VENDOR1_ACCT_NEED_ACTIVE               = 0x02000BBD,
    QL_NTRIP_ERR_VENDOR1_NO_AVAILABLE_ACCT              = 0x02000BBE,
    QL_NTRIP_ERR_VENDOR1_SERVICE_STOP                   = 0x02000BBF,
    QL_NTRIP_ERR_VENDOR1_INVALID_ACCOUNT                = 0x02000BC0,
    QL_NTRIP_ERR_VENDOR1_AUTH_EXCEPTION                 = 0x02000BC1,
    QL_NTRIP_ERR_VENDOR1_ACCTID_ERROR                   = 0x02000BC2,
    QL_NTRIP_ERR_VENDOR1_ACCT_ALREADY_ACTIVATED         = 0x02000BC3,
    QL_NTRIP_ERR_VENDOR1_ACCT_DISABLED                  = 0x02000BC4,
    QL_NTRIP_ERR_VENDOR1_MOUNTPOINT_INVALID             = 0x02000BC8,
    QL_NTRIP_ERR_VENDOR1_TOO_MANY_FAILURE               = 0x02000BC9,
    QL_NTRIP_ERR_VENDOR1_NOT_IN_SERVICE_EREA            = 0x02000BCA,
    QL_NTRIP_ERR_VENDOR1_NO_RTCM_IN_SERVICE_EREA        = 0x02000BCB,
    QL_NTRIP_ERR_VENDOR1_DNSTOIP_ERROR                  = 0x02000BCC,
    QL_NTRIP_ERR_VENDOR1_TIMEPOOL_BIND_FAIL             = 0x02000BCD,
    QL_NTRIP_ERR_VENDOR1_ACCOUNT_LOGIN_RPCFAIL          = 0x02000BCE,
    QL_NTRIP_ERR_VENDOR1_TIMEPOOL_TIME_EXCEED           = 0x02000BCF,
    QL_NTRIP_ERR_VENDOR1_TIMEPOOL_NOTEXIST              = 0x02000BD0,
    QL_NTRIP_ERR_VENDOR1_LOGIN_TYPE_ERROR               = 0x02000BD1,
    QL_NTRIP_ERR_VENDOR1_ACCOUNT_NO_SERVICE_PERIOD      = 0x02000BD2,
    QL_NTRIP_ERR_VENDOR1_ACCOUNT_NO_MOUNTPOINT          = 0x02000BD3,
    QL_NTRIP_ERR_VENDOR1_ALREADY_CONNECTED              = 0x02000BD4,
    QL_NTRIP_ERR_VENDOR1_OVER_NODE_MAX_CAPACITY         = 0x02000BD5,
    QL_NTRIP_ERR_VENDOR1_CORS_AUTH_FAILED               = 0x02000BD6,
    QL_NTRIP_ERR_VENDOR1_BAD_DEVICEID                   = 0x02000BD7,
    QL_NTRIP_ERR_VENDOR1_TERMINAL_ERROR                 = 0x02000BD8,
    QL_NTRIP_ERR_VENDOR1_AUTH_TYPE_ERROR                = 0x02000BD9,
    QL_NTRIP_ERR_VENDOR1_ENCRYPT_TYPE_ERROR             = 0x02000BDA,
    QL_NTRIP_ERR_VENDOR1_CORS_CONNECT_FAILED            = 0x02000BDB,
    QL_NTRIP_ERR_VENDOR1_CORS_LOGIN_FAILED              = 0x02000BDC,
    QL_NTRIP_ERR_VENDOR1_MANUFACTURER_ERROR             = 0x02000BDD,
    QL_NTRIP_ERR_VENDOR1_OVER_POOL_MAX_CAPACITY         = 0x02000BDE,
    QL_NTRIP_ERR_VENDOR1_BRAND_ERROR                    = 0x02000BDF,
    QL_NTRIP_ERR_VENDOR1_PRODUCT_ERROR                  = 0x02000BE0,
    QL_NTRIP_ERR_VENDOR1_ACCOUNTTYPE_ERROR              = 0x02000BE1,
    QL_NTRIP_ERR_VENDOR1_LOGIN_PARAS_ERROR              = 0x02000BE2,
    QL_NTRIP_ERR_VENDOR1_GGA_NOSESSION_ERROR            = 0x02000BE3,
    QL_NTRIP_ERR_VENDOR1_ACCPOOL_BIND_FAIL              = 0x02000BE4,
    QL_NTRIP_ERR_VENDOR1_ACCPOOL_NOTEXIST               = 0x02000BE5,
    QL_NTRIP_ERR_VENDOR1_POOL_NOTEXIST                  = 0x02000BE6,
    QL_NTRIP_ERR_VENDOR1_POOL_NOTALLOW_LOGIN            = 0x02000BE7,
    QL_NTRIP_ERR_VENDOR1_AUTHRIZATION_INVALID           = 0x02000BE8,
    QL_NTRIP_ERR_VENDOR1_INVALID_FRAME_OR_EPOCH         = 0x02000BE9,
    QL_NTRIP_ERR_VENDOR1_ACCOUNT_KICKING_ERROR          = 0x02000BEE,


    QL_NTRIP_ERR_VENDOR1_UNKNOWN_ERROR                  = 0x02000C1B,
    QL_NTRIP_ERR_VENDOR1_NULL_ACCT                      = 0x02000C1C,
    QL_NTRIP_ERR_VENDOR1_INVALID_PARA                   = 0x02000C1D,
    QL_NTRIP_ERR_VENDOR1_SERVER_ABORT                   = 0x02001388,
    QL_NTRIP_ERR_VENDOR1_CAP_START_FAILED               = 0x02001389,
} QL_NTRIP_G;


#endif  //__QL_ERROR_H__
