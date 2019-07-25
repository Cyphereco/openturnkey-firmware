
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

#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "otk_const.h"

#define CRYPTO_DERIVE_HARDENED_FORBIDDEN    /* Undefine to allow derivative of hardened key */

/* Constant definitions. */
#define CRYPTO_SEED_SIZE_OCTET          (512 / 8) 
#define CRYPTO_CHAIN_CODE_SZ            (32)
#define CRYPTO_PRIVATE_KEY_SZ           (32)
#define CRYPTO_PUBLIC_KEY_SZ            (33)    /* Compressed. */
#define CRYPTO_DECOMP_PUBLIC_KEY_SZ     (64)    /* Uncompressed. */
#define CRYPTO_SIGNATURE_SZ             (64)
#define CRYPTO_SIGNATURE_HEXSTR_SZ      (CRYPTO_SIGNATURE_SZ * 2 + 1)
#define CRYPTO_BITCOIN_ADDR_SZ          (35)    /* Compressed. */
#define CRYPTO_HEX_PUBLIC_KEY_SZ        (CRYPTO_PUBLIC_KEY_SZ * 2 + 1)
#define CRYPTO_EXT_PUBLIC_KEY_MAX_SZ    (160)   /* Maximum space reserved for extended public key string */
#define CRYPTO_DERIVATIVE_DEPTH         (5)
#define CRYPTO_DERIVATIVE_PATH_STR_SZ   (CRYPTO_DERIVATIVE_DEPTH * 11)

/* Seed struct. */
typedef struct {
    uint8_t octets[CRYPTO_SEED_SIZE_OCTET];
} CRYPTO_seed;

/* Derivative path struct. */
typedef struct {
    uint32_t  derivativeIndex[CRYPTO_DERIVATIVE_DEPTH];
} CRYPTO_derivativePath;

/* Raw public key struct. */
typedef struct {
    uint8_t octets[CRYPTO_PUBLIC_KEY_SZ];
} CRYPTO_publicKey;

/* Raw private key struct. */
typedef struct {
    uint8_t octets[CRYPTO_PRIVATE_KEY_SZ];
} CRYPTO_privateKey;

/* Decompressed public key struct. */
typedef struct {
    uint8_t octets[CRYPTO_DECOMP_PUBLIC_KEY_SZ];
} CRYPTO_decompPublicKey;

/* Chain codey struct. */
typedef struct {
    uint8_t octets[CRYPTO_CHAIN_CODE_SZ];
} CRYPTO_chainCode;

/* Signature struct. */
typedef struct {
    uint8_t octets[CRYPTO_SIGNATURE_SZ];
} CRYPTO_signature;

/* Extended public key string struct. */
typedef struct {
    char  str_ptr[CRYPTO_EXT_PUBLIC_KEY_MAX_SZ];
} CRYPTO_extPublicKey;

/* Hex public key string struct. */
typedef struct {
    char  str_ptr[CRYPTO_HEX_PUBLIC_KEY_SZ];
} CRYPTO_hexPublicKey;

/* Bitcoin (BTC) address string struct. */
typedef struct {
    char  str_ptr[CRYPTO_BITCOIN_ADDR_SZ];
} CRYPTO_btcAddr;

/* HD Node object struct. */
typedef struct {
    uint8_t depth;
    uint32_t fingerprint;
    uint32_t childNum;
    CRYPTO_chainCode     chainCode;
    CRYPTO_privateKey    privateKey;
    CRYPTO_publicKey     publicKey;
    CRYPTO_extPublicKey  extPublickey;
    CRYPTO_hexPublicKey  hexPublickey; 
    CRYPTO_btcAddr       btcAddr; 
} CRYPTO_HDNode;

uint32_t CRYPTO_rng32(void);

void CRYPTO_dumpHDNode(
    CRYPTO_HDNode *node_ptr);

OTK_Return CRYPTO_generateRandomSeed(
    CRYPTO_seed *seed);

OTK_Return CRYPTO_generateRandomPath(
    CRYPTO_derivativePath *path_ptr);

OTK_Return CRYPTO_setDerivativePath(
    CRYPTO_derivativePath *path_ptr,
    uint8_t depth,
    uint32_t value);

bool CRYPTO_isHDNodeValid(
    CRYPTO_HDNode *node_ptr);

OTK_Return CRYPTO_deriveHdNode(
    CRYPTO_HDNode           *masterHd_ptr,
    CRYPTO_HDNode           *deriveHd_ptr,
    CRYPTO_derivativePath   *path_ptr,
    CRYPTO_seed             *seed_ptr);

OTK_Return CRYPTO_sign(
    CRYPTO_privateKey  *privateKey_ptr,
    const uint8_t *hash_ptr,
    size_t         hashLen,
    CRYPTO_signature      *signature_ptr);

OTK_Return CRYPTO_verify(
    CRYPTO_publicKey   *publicKey_ptr,
    const uint8_t *hash_ptr,
    size_t         hashLen,
    CRYPTO_signature      *signature_ptr);

OTK_Return CRYPTO_init(void);

#endif

