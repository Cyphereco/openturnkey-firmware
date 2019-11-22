/**
 * Copyright (c), Cyphereco OU, All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided under MIT license agreement.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, 
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 *
 * IN NO EVENT SHALL Cyphereco OU OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, ORCONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTEGOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * 
 */

#ifndef _OTK_CONST_H
#define _OTK_CONST_H

#define OTK_HW_V_1_3

#ifdef OTK_HW_V_1_3
    #define OTK_HW_VER "1.3"
    #define OTK_FW_VER_CODE "1.3"
    #define OTK_FW_VER OTK_FW_VER_CODE"."BUILD_NUM
    #define OTK_MINT_DATE "2019/11/26"

    /* OpenTurnkey Mint Information */
    #define OTK_MINT_INFO \
    "Key Mint: Cyphereco OU\r\n" \
    "Mint Date: " OTK_MINT_DATE "\r\n" \
    "H/W Version: " OTK_HW_VER "\r\n" \
    "F/W Version: " OTK_FW_VER
#endif

#ifdef OTK_HW_V_1_2
    #define OTK_HW_VER "1.2"
    #define OTK_FW_VER_CODE "1.1"
    #define OTK_FW_VER OTK_FW_VER_CODE"."BUILD_NUM
    #define OTK_MINT_DATE "2019/06/09"

    /* OpenTurnkey Mint Information */
    #define OTK_MINT_INFO \
    "Key Mint: Cyphereco OU\r\n" \
    "Mint Date: " OTK_MINT_DATE "\r\n" \
    "H/W Version: " OTK_HW_VER "\r\n" \
    "F/W Version: " OTK_FW_VER
#endif

#ifdef OTK_HW_V_1_1
    #define OTK_HW_VER "1.1"
    #define OTK_FW_VER_CODE "1.0"
    #define OTK_FW_VER OTK_FW_VER_CODE"."BUILD_NUM
    #define OTK_MINT_DATE "2019/03/01"

    /* OpenTurnkey Mint Information */
    #define OTK_MINT_INFO \
    "Key Mint: Cyphereco OU\r\n" \
    "Mint Date: " OTK_MINT_DATE "\r\n" \
    "H/W Version: " OTK_HW_VER "\r\n" \
    "F/W Version: " OTK_FW_VER
#endif

#ifdef OTK_HW_V_1_0
    #define OTK_HW_VER "1.0"
    #define OTK_FW_VER_CODE "0.9"
    #define OTK_FW_VER OTK_FW_VER_CODE"."BUILD_NUM
    #define OTK_MINT_DATE "2018/12/21"

    /* OpenTurnkey Mint Information */
    #define OTK_MINT_INFO \
    "Key Mint: Cyphereco OU\r\n" \
    "Mint Date: " OTK_MINT_DATE "\r\n" \
    "H/W Version: " OTK_HW_VER "\r\n" \
    "F/W Version: " OTK_FW_VER "\r\n"
#endif

/* Prompt labels */
#define OTK_LABEL_APP_URI           "APP_URI"
#define OTK_LABEL_MINT_INFO         "Mint_Information"
#define OTK_LABEL_OTK_STATE         "OTK_State"
#define OTK_LABEL_PUBLIC_KEY        "Public_Key"
#define OTK_LABEL_SESSION_DATA      "Session_Data"
#define OTK_LABEL_SESSION_ID        "Session_ID"
#define OTK_LABEL_REQUEST_ID        "Request_ID"
#define OTK_LABEL_BITCOIN_ADDR      "BTC_Addr"
#define OTK_LABEL_MASTER_EXT_KEY    "Master_Extended_Key"
#define OTK_LABEL_DERIVATIVE_EXT_KEY "Derivative_Exteded_Key"
#define OTK_LABEL_DERIVATIVE_PATH   "Derivative_Path"
#define OTK_LABEL_WIF_KEY           "WIF_Key"
#define OTK_LABEL_SECURE_PIN   		"Secure_Pin"
#define OTK_LABEL_REQUEST_SIG       "Request_Signature"
#define OTK_LABEL_SESSION_SIG       "Session_Signature"


/* Return value enumeration. */
typedef enum {
    OTK_RETURN_OK = 0,
    OTK_RETURN_FAIL = 1,
} OTK_Return;

/* Return value enumeration. */
typedef enum {
    OTK_ERROR_NO_ERROR = 0,
    OTK_ERROR_AUTH_FAILED,
    OTK_ERROR_NFC_INVALID_REQUEST,
    OTK_ERROR_NFC_INVALID_SIGN_DATA,
    OTK_ERROR_NFC_SIGN_FAIL,
    OTK_ERROR_NFC_TOO_MANY_SIGNATURES,
    OTK_ERROR_INIT_FPS,
    OTK_ERROR_INIT_NFC,
    OTK_ERROR_INIT_CRYPTO,
    OTK_ERROR_INIT_KEY,
    OTK_ERROR_INIT_PWRMGMT,
    OTK_ERROR_INIT_UART,
    OTK_ERROR_INIT_TIMER,
    OTK_ERROR_INIT_LED,
    OTK_ERROR_PIN_UNSET,
    OTK_ERROR_PIN_NOT_MATCH,
    OTK_ERROR_INVALID_PIN,
    OTK_ERROR_INVALID_KEYPATH,
    OTK_ERROR_NOTE_TOO_LONG,
    OTK_ERROR_UNKNOWN
} OTK_Error;
#endif

