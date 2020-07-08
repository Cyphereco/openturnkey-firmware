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

#ifndef _KEY_H_
#define _KEY_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "otk_const.h"
#include "crypto.h"

/**
 * @brief Constants used in NFC module.
 */
#define KEY_DERIVATIVE  (0)
#define KEY_MASTER      (1)
#define KEY_DEFAULT_PIN (0xFFFFFFFF)
#define KEY_NOTE_LENGTH (64)

/**
 * @brief Key object struct.
 */
typedef struct {
    CRYPTO_HDNode           master;             /* Master Key */
    CRYPTO_HDNode           derivative;         /* Derivative Child Key */
    CRYPTO_derivativePath   path;               /* Derivative Child Key Path */
    CRYPTO_signature        *signature_ptr;     /* Signed Signature */
    uint32_t                pin;                /* PIN Code */
    uint8_t                 pin_auth_failures;  /* times of pin auth failures */
    uint32_t                pin_retry_after;    /* time of reboots before pin auth available */
    char                    keyNote[KEY_NOTE_LENGTH + 1];    /* Customized User Note for Key */
} KEY_Object;


/**
 * @brief Get derivative path
 *
 * @return      CRYPTO_derivativePath*   Get derivative path.
 */
CRYPTO_derivativePath *KEY_getDerivativePath(void);

/**
 * @brief Get derivative path in string format.
 *
 * @return      char*          String of derivative path.
 */
char *KEY_getStrDerivativePath(void); 

/**
 * @brief Set new derivative path
 * @param[in]   CRYPTO_derivativePath*    New derivative path
 * 
 * Call KEY_recalcDerivative to update keys, otherwise, it is not taking any effect.
 */
void KEY_setNewDerivativePath(
    CRYPTO_derivativePath  *derivativePath);

/**
 * @brief Re-calculate mater and derivative key.
 *
 * @return      OTK_Return      OTK_RETURN_OK if no error, OTK_RETURN _FAIL otherwise.
 */
OTK_Return KEY_recalcDerivative(void);

/**
 * @brief Get signed signature
 *
 * @return      CRYPTO_signature*    Signature, NULL is error or signing not completed.
 */
CRYPTO_signature *KEY_getSignature(void);

/**
 * @brief Get signed signature in  HEX string
 *
 * @return      char*           Signature hex string.
 */
char *KEY_getHexSignature(void);

/**
 * @brief Clear signed signature.
 */
void KEY_eraseSignature(void);

/**
 * @brief Get Public Key in HEX string
 * @param[in]   getMaster       Option, 1 - ger master's, 0 - get derivative's
 *
 * @return      CRYPTO_publicKey*   Public key of master or derivative key
 */
CRYPTO_publicKey *KEY_getPublicKey(
    bool getMaster);

/**
 * @brief Get Public Key in HEX string
 * @param[in]   getMaster       Option, 1 - ger master's, 0 - get derivative's
 *
 * @return      char*           String of public key of master or derivative key
 */
char *KEY_getHexPublicKey(
    bool getMaster);

/**
 * @brief Get Private Key in WIF string
 * @param[in]   getMaster       Option, 1 - ger master's, 0 - get derivative's
 *
 * @return      char*           String of public key of master or derivative key
 */
char *KEY_getWIFPrivateKey(
    bool getMaster);

/**
 * @brief Get Full Extended Key String 
 * @param[in]   getMaster       Option, 1 - ger master's, 0 - get derivative's
 *
 * @return      char*           String of extended public key of master or derivative key
 */
char *KEY_getExtPublicKey(
    bool getMaster);

/**
 * @brief Get Key's BTC address
 * @param[in]   getMaster       Option, 1 - ger master's, 0 - get derivative's
 *
 * @return      char*           String of BTC address of master or derivative key
 */
char *KEY_getBtcAddr(
    bool getMaster);

/**
 * @brief Get current secure PIN code setting
 *
 * @return      UINT32          Current PIN code
 */
uint32_t KEY_getPin(void);

/**
 * @brief Set new secure PIN code
 * @param[in]   pin             New secure PIN code to be set
 *
 * @return      OTK_Return      OTK_RETURN_OK if no error, OTK_RETURN _FAIL otherwise.
 */
OTK_Return KEY_setPin(uint32_t pin);

/**
 * @brief Initialize KEY module
 *
 * @return      OTK_Return      OTK_RETURN_OK if no error, OTK_RETURN _FAIL otherwise.
 */
OTK_Return KEY_init(void);

/**
 * @brief Usign master or derivative key to sign a hash value.
 * @param[in]   hash_ptr        Pointer to hex string
 * @param[in]   hashLen         Length of hex string
 * @param[in]   usingMaster     Option of using master key, 1 - master, 0 - derivative
 *
 * @return      OTK_Return      OTK_RETURN_OK if no error, OTK_RETURN _FAIL otherwise.
 *
 * Usign KEY_getSignature to get singed signature output, 
 * KEY_getSignature returns NULL before signature is calculated or calculated failed.
 *
 * Using KEY_getHexSignature get signature in hex string format, call it when 
 * KEY_getSignature returns not NULL value.
 * 
 * When signature is calculated, copy it somewhere else and calling KEY_eraseSignature
 * to allow another signing action.
 *
 */
OTK_Return KEY_sign(
    const uint8_t       *hash_ptr,
    size_t              hashLen,
    bool                usingMaster);

/**
 * @brief Get customized user note for Key
 *
 * @return      char*          Pointer of key note
 */
char *KEY_getNote(void);

/**
 * @brief Set customized user note for Key
 * @param[in]   note            String of Customized User Note
 *
 * @return      OTK_Return      OTK_RETURN_OK if no error, OTK_RETURN _FAIL otherwise.
 */
OTK_Return KEY_setNote(
    char *note);

uint8_t KEY_getPinAuthFailures();

void KEY_setPinAuthFailures(uint8_t failures);

uint32_t KEY_getPinAuthRetryAfter();

void KEY_setPinAuthRetryAfter(uint32_t retryAfter);

#endif

