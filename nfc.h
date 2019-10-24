
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

#ifndef _NFC_H_
#define _NFC_H_

#include "nfc_ndef_msg.h"

/**
 * @brief NFC is only inteface of OTK which allows external user input.
 * Since the input can be anything and malicious, sanity check on the 
 * input value is necessary to avoid hacking or accidental damage to OTK.
 */

/**
 * @brief Constants used in NFC module.
 */
#define NFC_NDEF_MSG_BUF_SZ    2048     /* Maximum NFC output NDEF message size. */
#define NFC_MAX_RECORD_SZ      1536      /* Maximum data parsing size of a NFC record. */

#define NFC_REQUEST_DATA_BUF_SZ  (NFC_MAX_RECORD_SZ - NLEN_FIELD_SIZE)
#define NFC_REQUEST_OPT_BUF_SZ   (64 - NLEN_FIELD_SIZE)

/**
 * @brief NFC session data record definitions.
 * 
 * NFC session records are constructed on the following order.
 *
 * This definition is not used, but defined as a reference of 
 * the structure of NFC session records.
 */
typedef enum {
    NFC_RECORD_DEF_APP_URI = 0,         /* App URI record. */
    NFC_RECORD_DEF_MINT_INFO,           /* OTK Mint information. */
    NFC_RECORD_DEF_OTK_STATE,          /* OTK lock state, 0 - unlocked, 1 - locked. */
    NFC_RECORD_DEF_PUBLIC_KEY,          /* OTK derivative key's public key, compressed hex. */
    NFC_RECORD_DEF_SESSION_DATA,        /* OTK session data, dynamic content based on conditions. */
    NFC_RECORD_DEF_SESSION_SIGNATURE,   /* Signature of session data signed by derivative public key. */
    NFC_RECORD_DEF_LAST,                /* Not used, for completion only. */
} NFC_RECORD_DEF;

#define NFC_MAX_RECORD_COUNT  NFC_RECORD_DEF_LAST  /* Maximum NFC output records. */

/**
 * @brief NFC request record type definitions.
 * A VALID NFC request should contain at least the FIRST THREE records
 * and VALID data or option, if required.
 * NFC module checks each record's validity and reject/ignore a request
 * which does not meet requirements.
 */
typedef enum {
    NFC_REQUEST_DEF_SESSION_ID = 0,     /* Session ID, mandatory and must meet the OTK session ID. */
    NFC_REQUEST_DEF_COMMAND_ID,         /* Request command ID, mandatory, UINT32 random number set by the clien. (To add external variables for signature's authenticity check. */
    NFC_REQUEST_DEF_COMMAD,             /* Request command, mandatory. */
    NFC_REQUEST_DEF_DATA,               /* Request data, mandator/optional based on request command. */
    NFC_REQUEST_DEF_OPTION,             /* Request option, optional. */
    NFC_REQUEST_DEF_LAST                /* Not used, for completion only. */
} NFC_REQUEST_DEF;

#define NFC_MAX_REQUEST_COUNT  NFC_REQUEST_DEF_LAST  /* Maximum NFC output records. */

/**
 * @brief NFC rquest command definitions.
 */
typedef enum {
    NFC_REQUEST_CMD_INVALID = 0,        /* 0 / 0x0, Invalid,  For check purpose. */ 
    NFC_REQUEST_CMD_LOCK = 0xA0,        /* 160 / 0xA0, Enroll fingerpint on OTK. */ 
    NFC_REQUEST_CMD_UNLOCK,             /* 161 / 0xA1, Erase enrolled fingerprint and reset secure PIN to default, OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SHOW_KEY,           /* 162 / 0xA2, Present master/derivative extend keys and derivative path and secure PIN code, OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SIGN,               /* 163 / 0xA3, Sign external data (32 bytes hash data), taking request option: 1/Using master key, 0/Using derivated key(default), OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SET_KEY,            /* 164 / 0xA4, Set/chagne derivative KEY (path), OTK (pre)authorization is required. */ 
    NFC_REQUEST_CMD_SET_PIN,            /* 165 / 0xA5, Set/change secure PIN setting, OTK (pre)authorization is required. */ 
    NFC_REQUEST_CMD_SET_NOTE,           /* 166 / 0xA6, Set customized user note. */ 
    NFC_REQUEST_CMD_CANCEL,             /* 167 / 0xA7, Cancel previous command request. */ 
    NFC_REQUEST_CMD_LAST                /*  -- /, Not used, only for completion. */ 
} NFC_REQUEST_COMMAND;

/**
 * @brief NFC command response definitions.
 */
typedef enum {
    NFC_CMD_EXEC_NA = 0,            /* 0, Command not applicable. */ 
    NFC_CMD_EXEC_SUCCESS,           /* 1, Command executed successfully. */ 
    NFC_CMD_EXEC_FAIL,              /* 2, Command executed failed. */  
    NFC_CMD_EXEC_LAST               /* -, Not used, only for completion. */ 
} NFC_COMMAND_EXEC_STATE;

/**
 * @brief NFC command failure reason definitions.
 */
typedef enum {
    NFC_REASON_INVALID = 0x00,  /*   0 / 0x00, Invalid,  For check purpose. */ 
    NFC_REASON_TIMEOUT = 0xC0,  /* 192 / 0xC0, Command timeout. */ 
    NFC_REASON_AUTH_FAILED,     /* 193 / 0xC1, Authentication failed. */  
    NFC_REASON_CMD_INVALID,     /* 194 / 0xC2, Invalid command. */  
    NFC_REASON_PARAM_INVALID,   /* 195 / 0xC3, Invalid parameter. */  
    NFC_REASON_PARAM_MISSING,   /* 196 / 0xC4, Mandatory parameter not presented. */ 
    NFC_REASON_LOCKED_ALREADY,  /* 196 / 0xC5, Device is already locked. */ 
    NFC_REASON_UNLOCKED_ALREADY,   /* 196 / 0xC5, Device is already locked. */ 
    NFC_REASON_LAST = 0xFF      /* 255 / 0xFF, Not used, only for completion. */ 
} NFC_COMMAND_EXEC_FAILURE_REASON;

/**
 * @brief Initialze NFC interface.
 *
 * @return       OTK_Return     OTK_RETURN_OK if no error, or OTK_RETURN_FAIL otherwise
 */
OTK_Return NFC_init(void);

/**
 * @brief Start NFC emulation. Will set NFC records.
 *
 * @return       OTK_Return     OTK_RETURN_OK if no error, or OTK_RETURN_FAIL otherwise
 */
OTK_Return NFC_start(void);

/**
 * @brief Stop NFC emulation.
 * @param[in]    restart        restart after NFC stopped if restart=ture.
 *
 * @return       OTK_Return     OTK_RETURN_OK if no error, or OTK_RETURN_FAIL otherwise
 */
OTK_Return NFC_stop(bool restart);

/**
 * @brief Force stop NFC emulation without scheduler.
 *
 * @return       OTK_Return     OTK_RETURN_OK if no error, or OTK_RETURN_FAIL otherwise
 */
OTK_Return NFC_forceStop(void);
#endif
