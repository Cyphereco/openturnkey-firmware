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

#include <string.h>
#include <stdint.h>
#include "sdk_common.h"
#include "nrf_assert.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_crypto.h"
#include "nrf_crypto_ecc.h"
#include "nrf_crypto_error.h"
#include "nrf_crypto_ecdsa.h"
#include "mem_manager.h"
#include "nrf_crypto_ecc.h"
#include "mbedtls/ripemd160.h"

#include "otk.h"

static CRYPTO_seed seed1 = {
    .octets = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}
};

static uint8_t hash[] =
{
    // SHA256("Hello Bob!")
    0x42, 0xba, 0x83, 0x54, 0xdb, 0x26, 0x3a, 0x6a,
    0x5a, 0x9f, 0x74, 0xd6, 0xb7, 0xce, 0xb4, 0xc9,
    0x62, 0xa3, 0xd8, 0xfd, 0x58, 0xa4, 0x19, 0x69,
    0xe5, 0x21, 0xeb, 0x02, 0x22, 0x45, 0x54, 0x15,
};

/*
 * ======== hdTests() ========
 * BTC HD tests
 */
int hdTests(void)
{
    CRYPTO_HDNode hdNode = {0};
    CRYPTO_HDNode childHdNode = {0};
    int testNo = 0;

    /* Test 1, generate master node from a known seed1. */
    testNo++;
    if (CRYPTO_deriveHdNode(NULL, &hdNode, NULL, &seed1) != OTK_RETURN_OK) {
        OTK_LOG_ERROR("Test %d failed", testNo);
        return (1);
    }

    /* Test 2. */
    testNo++;
    if (CRYPTO_deriveHdNode(&hdNode, &childHdNode, NULL, NULL) != OTK_RETURN_OK) {
        NRF_LOG_ERROR("Test %d failed", testNo);
        return (2);
    }

    /* Test 3. Sign. */
    testNo++;
    CRYPTO_signature signature;
    if (CRYPTO_sign(&childHdNode.privateKey, hash, sizeof(hash), &signature) != OTK_RETURN_OK) {
        NRF_LOG_ERROR("Test %d failed", testNo);
        return (3);
    }

    /* Test 4. Verify. */
    testNo++;
    if (CRYPTO_verify(&childHdNode.publicKey, hash, sizeof(hash), &signature) != OTK_RETURN_OK) {
        NRF_LOG_ERROR("Test %d failed", testNo);
        return (4);
    }

    return (0);
}

/*
 *
 */
int unittestRun(void)
{
    return(hdTests());
}

