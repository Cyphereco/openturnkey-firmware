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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdk_common.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "libbtc/utils.h"

#include "key.h"
#include "fps.h"
#include "file.h"
#include "crypto.h"

/* Fix seed index. */
#define FIX_SEED_INDEX_0           (0)
#define FIX_SEED_INDEX_1           (1)
#define FIX_SEED_INDEX_2           (2)
#define FIX_SEED_INDEX_3           (3)
/* 
 * Define this for fix root keys for developing phase so that the root keys won't change everytime on refreshing image.
 * Only define it to FIX_SEED_INDEX_0 ~ FIX_SEED_INDEX_3.
 */
#define FIX_SEED_INDEX FIX_SEED_INDEX_0

#define KEY_DEBUG_INFO (1)

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif

#define NRF_LOG_MODULE_NAME otk_key
NRF_LOG_MODULE_REGISTER();

#ifdef FIX_SEED_INDEX
static uint8_t _fixSeed[][CRYPTO_SEED_SIZE_OCTET] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4}};
#endif

static KEY_Object _keyObj;
static CRYPTO_signature _signature;
static char _keyDerivativePath[11 * CRYPTO_DERIVATIVE_DEPTH + 1];
static char _hexSignature[2 * CRYPTO_SIGNATURE_SZ + 1];

static OTK_Return key_updateFile()
{
    OTK_Return ret = OTK_RETURN_FAIL;

    ret = FILE_write(false, (uint8_t*)&_keyObj, sizeof(_keyObj));
    if (ret != OTK_RETURN_OK) {
        OTK_LOG_ERROR("Failed to update key file!!");
        return (OTK_RETURN_FAIL);
    }    
    return (OTK_RETURN_OK);    
}

static void key_dumpKey(CRYPTO_HDNode node) 
{
    NRF_LOG_INFO("  Bitcoin Address: %s", node.btcAddr.str_ptr);
    NRF_LOG_INFO("  Public Key: %s", node.hexPublickey.str_ptr);
    NRF_LOG_INFO("  Extended Public Key: %s", node.extPublickey.str_ptr);
    nrf_delay_ms(5);
    NRF_LOG_INFO("\r\n");
}

CRYPTO_derivativePath *KEY_getDerivativePath() 
{
    return (&_keyObj.path);
}

char *KEY_getStrDerivativePath() 
{
    memset(_keyDerivativePath, 0, (11 * CRYPTO_DERIVATIVE_DEPTH));
    int i;

    sprintf(_keyDerivativePath, "%s", "m");
    for (i = 0; i < CRYPTO_DERIVATIVE_DEPTH; i++) {
        sprintf(_keyDerivativePath, "%s/%lu", _keyDerivativePath, _keyObj.path.derivativeIndex[i]);
    }

    return (_keyDerivativePath);
}

void KEY_setNewDerivativePath(CRYPTO_derivativePath *derivativePath) 
{
    for (int i = 0; i < CRYPTO_DERIVATIVE_DEPTH; i++) {
        _keyObj.path.derivativeIndex[i] = derivativePath->derivativeIndex[i];
    }
}

OTK_Return KEY_recalcDerivative()
{
    OTK_Return ret = OTK_RETURN_FAIL;

    ret = CRYPTO_deriveHdNode(&_keyObj.master, &_keyObj.derivative, &_keyObj.path, NULL);

    if (ret != OTK_RETURN_OK) {
        OTK_LOG_ERROR("Failed to generate derivative node!!");
        return (OTK_RETURN_FAIL);
    }

    ret = key_updateFile();

    return (ret);
}

CRYPTO_signature *KEY_getSignature()
{
    return (_keyObj.signature_ptr);
}

char *KEY_getHexSignature()
{
    if (NULL == _keyObj.signature_ptr) {
        return (NULL);
    }

    utils_bin_to_hex(_keyObj.signature_ptr->octets, CRYPTO_SIGNATURE_SZ, _hexSignature);
    return (_hexSignature);
}

void KEY_eraseSignature()
{
    _keyObj.signature_ptr = NULL;
}

CRYPTO_publicKey *KEY_getPublicKey(bool getMaster) 
{
    return (getMaster ? &_keyObj.master.publicKey : &_keyObj.derivative.publicKey);
}

char *KEY_getHexPublicKey(bool getMaster) 
{
    return (getMaster ? _keyObj.master.hexPublickey.str_ptr : _keyObj.derivative.hexPublickey.str_ptr);
}

char *KEY_getExtPublicKey(bool getMaster) 
{
    return (getMaster ? _keyObj.master.extPublickey.str_ptr : _keyObj.derivative.extPublickey.str_ptr);
}

char *KEY_getBtcAddr(bool getMaster) 
{
    return (getMaster ? _keyObj.master.btcAddr.str_ptr : _keyObj.derivative.btcAddr.str_ptr);
}

uint32_t KEY_getPin()
{
    return (_keyObj.pin);
}

OTK_Return KEY_setPin(uint32_t pin)
{
    _keyObj.pin = pin;

    return (key_updateFile());
}

OTK_Return KEY_init() 
{
    OTK_Return ret = OTK_RETURN_FAIL;

    /* Initialize FILE. */  
    if (FILE_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("FILE_init failed!!");       
        return (OTK_RETURN_FAIL);
    }        

    /* Try to restore key object from saved file */
    int _len = 0;
    if (FILE_load((uint8_t*)&_keyObj, &_len) == OTK_RETURN_OK) {
        if (false == CRYPTO_isHDNodeValid(&(_keyObj.master))) {
            OTK_LOG_ERROR("Invalid master key.");
            CRYPTO_dumpHDNode(&_keyObj.master);
            nrf_delay_ms(5);
            return (OTK_RETURN_FAIL);        
        }
        if (false == CRYPTO_isHDNodeValid(&(_keyObj.derivative))) {
            OTK_LOG_ERROR("Invalid derivative key.");
            CRYPTO_dumpHDNode(&_keyObj.derivative);
            nrf_delay_ms(5);
            return (OTK_RETURN_FAIL);        
        }
    }           
    else {
        OTK_LOG_DEBUG("FILE_load failed, file not existed!!");

        /* If FILE_load failed, file has not been created yet, OTK is new
         *  - Clear up FPS,
         *  - Create file
         */
        if (OTK_RETURN_OK != FPS_eraseAll()) {
            OTK_LOG_ERROR("FPS_eraseAll failed!!");       
            return (OTK_RETURN_FAIL);        
        }

        /* Generate master node from seed. */
#ifdef FIX_SEED_INDEX
        OTK_LOG_DEBUG("Generating new master key from FIX seed...");
        CRYPTO_seed _seed;
        memcpy(_seed.octets, _fixSeed[FIX_SEED_INDEX], CRYPTO_SEED_SIZE_OCTET);
        ret = CRYPTO_deriveHdNode(NULL, &_keyObj.master, NULL, &_seed);
#else
        OTK_LOG_DEBUG("Generating new master key from RANDOM seed...");       
        ret = CRYPTO_deriveHdNode(NULL, &_keyObj.master, NULL, NULL);
#endif /* FIX_SEED_INDEX */      

        if (ret != OTK_RETURN_OK) {
            OTK_LOG_ERROR("Failed to generate master node!!");
            return (OTK_RETURN_FAIL);        
        }

        /* XXX Here we derive a non-harden child node automatically for now. It should be requested by APP. */
        CRYPTO_generateRandomPath(&_keyObj.path);

#ifdef FIX_SEED_INDEX
        /* Use index 0 for testing. */
        for (int i = 0; i < CRYPTO_DERIVATIVE_DEPTH; i++) {
            CRYPTO_setDerivativePath(&_keyObj.path, i, 0);
        }
#endif /* FIX_SEED_INDEX */

        ret = CRYPTO_deriveHdNode(&_keyObj.master, &_keyObj.derivative, &_keyObj.path, NULL);

        if (ret != OTK_RETURN_OK) {
            OTK_LOG_ERROR("Failed to generate derivative node!!");
            return (OTK_RETURN_FAIL);
        }

        _keyObj.pin = (KEY_DEFAULT_PIN);

        memset(_keyObj.keyNote, 0, KEY_NOTE_LENGTH + 1);

        ret = FILE_write(true, (uint8_t*)&_keyObj, sizeof(_keyObj));
        if (ret != OTK_RETURN_OK) {
            OTK_LOG_ERROR("Failed to write keys to file!!");
            return (OTK_RETURN_FAIL);
        }
    }

    NRF_LOG_INFO("Dupm Master Key:");
    key_dumpKey(_keyObj.master);
    NRF_LOG_INFO("Dupm Derivative Key:");
    key_dumpKey(_keyObj.derivative);
    NRF_LOG_INFO("Dupm Derivative Path: %s", KEY_getStrDerivativePath());
    NRF_LOG_INFO("\r\n");
   
    return (OTK_RETURN_OK);
}

OTK_Return KEY_sign(
    const uint8_t       *hash_ptr,
    size_t              hashLen,
    bool                usingMaster
    ) 
{
    CRYPTO_privateKey *_privKey = NULL;
    CRYPTO_publicKey  *_pubKey  = NULL;
    _keyObj.signature_ptr = NULL;        

    if (usingMaster) {
        _privKey = &_keyObj.master.privateKey;
        _pubKey  = &_keyObj.master.publicKey;
    }
    else {
        _privKey = &_keyObj.derivative.privateKey;
        _pubKey  = &_keyObj.derivative.publicKey;
    }

    if (OTK_RETURN_OK != CRYPTO_sign(_privKey, hash_ptr, hashLen, &_signature)) {
        OTK_LOG_ERROR("CRYPTO_sign failed!!");
        return (OTK_RETURN_FAIL);
    }


    if (OTK_RETURN_OK != CRYPTO_verify(_pubKey, hash_ptr, hashLen, &_signature)) {
        OTK_LOG_ERROR("Signature is not verified!!");
        return (OTK_RETURN_FAIL);
    }
    else {
        _keyObj.signature_ptr = &_signature;        
    }

    return (OTK_RETURN_OK);
}

char *KEY_getKeyNote() 
{
    return _keyObj.keyNote;
}

OTK_Return KEY_setKeyNote(char *note)
{
    if (note != NULL && strlen(note) <= KEY_NOTE_LENGTH) {
        memcpy(_keyObj.keyNote, note, strlen(note) + 1);
        return (key_updateFile());
    }

    return OTK_RETURN_FAIL;
}