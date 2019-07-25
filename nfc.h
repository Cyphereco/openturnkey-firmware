
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
    NFC_RECORD_DEF_LOCK_STATE,          /* OTK lock state, 0 - unlocked, 1 - locked. */
    NFC_RECORD_DEF_PUBLIC_KEY,          /* OTK derivative key's public key, compressed hex. */
    NFC_RECORD_DEF_SESSION_DATA,        /* OTK session data, dynamic content based on conditions. */
    NFC_RECORD_DEF_SESSION_SIGNATURE,   /* Signature of session data signed by derivative public key. */
    NFC_RECORD_DEF_LAST,                /* Not used, for completion only. */
} NFC_RECORD_DEF_Def;

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
} NFC_RequestdDef;

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
    NFC_REQUEST_CMD_PRE_AUTH_WITH_PIN,  /* 166 / 0xA6, Pre-authorize OTK with secure PIN via NFC request. */ 
    NFC_REQUEST_CMD_SET_NOTE,           /* 167 / 0xA7, Set customized user note. */ 
    NFC_REQUEST_CMD_LAST                /* 168 / 0xA8, Not used, only for completion. */ 
} NFC_RequestCommand;

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
