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
#include "nrf_assert.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_crypto.h"
#include "nrf_crypto_ecc.h"
#include "nrf_crypto_error.h"
#include "nrf_crypto_ecdsa.h"
#include "mem_manager.h"
#include "nrf_crypto_ecc.h"
#include "libbtc/sha2.h"
#include "libbtc/ripemd160.h"
#include "libbtc/base58.h"
#include "libbtc/utils.h"
#include "secp256k1/secp256k1.h"

#include "crypto.h"
#include "otk.h"

#define CRYPTO_DEBUG_INFO (0)

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif

#define NRF_LOG_MODULE_NAME otk_crypto
NRF_LOG_MODULE_REGISTER();

/* Module initialization check macro
 * If module is not initiated, log debug message and return fail.
 */
#define __INIT_CHECK__                                      \
do {                                                        \
    if (!_initiated) {                                      \
        OTK_LOG_ERROR("CRYPTO has not been initiated!!");   \
        return (OTK_RETURN_FAIL);                           \
    }                                                       \
} while (0);


/* === Start of local functions declaration === */
static void crypto_serialize32(
    uint32_t i,
    uint8_t *dest_ptr);

static OTK_Return crypto_derivePublicKeyFromPrivateKey(            
    CRYPTO_HDNode *hdNode_ptr);

static OTK_Return crypto_childKeyDerivation(
    CRYPTO_HDNode *masterhdNode_ptr,
    CRYPTO_HDNode *derivehdNode_ptr,
    uint32_t      index);

static void crypto_serializedExtPublicKey(
    CRYPTO_HDNode *hdNode_ptr);

static void crypto_serializedWIFPrivateKey(
    CRYPTO_HDNode *hdNode_ptr);

static int crypto_calcBtcAddr(
    CRYPTO_HDNode *hdNode_ptr);
/* === End of local functions declaration === */


/* === Start of local variables declaration === */
static bool _initiated = false;                             /* wether crypto module has been initiated */
static const char _hmac_sha512Key[] = "Bitcoin seed";
static const uint8_t empty[CRYPTO_DECOMP_PUBLIC_KEY_SZ] = {0}; /* For comparing if a key is empty. */
/* === Start of local variables declaration === */


/*
 * ======== crypto_serialize32() ========
 * Serialize a 32-bit unsigned integer i as a 4-byte sequence, most significant byte first.
 *
 * Parameters:
 *
 * Returns:
 */
static void crypto_serialize32(
    uint32_t i,
    uint8_t *dest_ptr)
{
    dest_ptr[0] = i >> 24;
    dest_ptr[1] = i >> 16;
    dest_ptr[2] = i >> 8;
    dest_ptr[3] = i;
}

static OTK_Return crypto_derivePublicKeyFromPrivateKey(
    CRYPTO_HDNode *hdNode_ptr)
{
    __INIT_CHECK__
    ret_code_t errCode;
    size_t length;

    nrf_crypto_ecc_private_key_t priKeyInt;
    nrf_crypto_ecc_public_key_t pubKeyInt;
    nrf_crypto_ecc_secp256k1_raw_public_key_t pubKeyRaw; 
    nrf_crypto_ecc_public_key_calculate_context_t   calcContext;

    if ((errCode = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256k1_curve_info,
              &priKeyInt, hdNode_ptr->privateKey.octets, CRYPTO_PRIVATE_KEY_SZ)) != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }

    /* Calculate public key. */
    if ((errCode = nrf_crypto_ecc_public_key_calculate(&calcContext, &priKeyInt, &pubKeyInt)) != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        nrf_crypto_ecc_private_key_free(&priKeyInt);
        return (OTK_RETURN_FAIL);
    }

    length = sizeof(pubKeyRaw);
    if ((errCode = nrf_crypto_ecc_public_key_to_raw(&pubKeyInt, pubKeyRaw, &length)) != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        nrf_crypto_ecc_private_key_free(&priKeyInt);
        nrf_crypto_ecc_public_key_free(&pubKeyInt);
        return (OTK_RETURN_FAIL);
    }

    nrf_crypto_ecc_private_key_free(&priKeyInt);
    nrf_crypto_ecc_public_key_free(&pubKeyInt);

    /* Convert to compressed format. */
    if (pubKeyRaw[length - 1] & 1) {
        /* Odd, prefix is 0x03. */
        hdNode_ptr->publicKey.octets[0] = 0x03;
    }
    else {
        /* Odd, prefix is 0x02. */
        hdNode_ptr->publicKey.octets[0] = 0x02;
    }
    /* Append first 32 bytes of raw public key(uncompressed) to pubic key. */
    memcpy(hdNode_ptr->publicKey.octets + 1, pubKeyRaw, 32);

    return (OTK_RETURN_OK);
}

/*
 * ======== CRYPTOcrypto_childKeyDerivation() ========
 * Derive HD child keys from HD keys.
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return crypto_childKeyDerivation(
    CRYPTO_HDNode *masterhdNode_ptr,
    CRYPTO_HDNode *derivehdNode_ptr,
    uint32_t      index)
{
    __INIT_CHECK__
    int ret;

    uint8_t I[CRYPTO_PRIVATE_KEY_SZ + CRYPTO_CHAIN_CODE_SZ] = {0};
    uint8_t data[1 + 32 + 4];
    uint8_t fingerprint[32];

    /* Check if to derivate hardened child key, index > 2^31 - 1 */
    if (index & 0x80000000) {
#if defined CRYPTO_DERIVE_HARDENED_FORBIDDEN
        NRF_LOG_DEBUG("Derive hardened key is forbidden!!");
        return (OTK_RETURN_FAIL);
#endif /* defined CRYPTO_DERIVE_HARDENED_FORBIDDEN */

        /*
         * Key is hardened.
         * I = HMAC-SHA512(Key = cpar, Data = 0x00 || ser256(kpar) || ser32(i)).
         * (Note: The 0x00 pads the private key to make it 33 bytes long.)
         */
        data[0] = 0;
        memcpy(data + 1, masterhdNode_ptr->privateKey.octets, CRYPTO_PRIVATE_KEY_SZ);
        /* Append ser32(i) */
        crypto_serialize32(index, data + CRYPTO_PUBLIC_KEY_SZ + 1);
    } else {
        /*
         * Key is non-hardened
         * If not (normal child): let I = HMAC-SHA512(Key = cpar, Data = serP(point(kpar)) || ser32(i)).
         */
        memcpy(data, masterhdNode_ptr->publicKey.octets, CRYPTO_PUBLIC_KEY_SZ);
        /* Append ser32(i) */
        crypto_serialize32(index, data + CRYPTO_PUBLIC_KEY_SZ);
    }


    /* SHA256 */
    sha256_Raw(masterhdNode_ptr->publicKey.octets, CRYPTO_PUBLIC_KEY_SZ, fingerprint);

    ripemd160(fingerprint, SHA256_DIGEST_LENGTH, fingerprint);

    derivehdNode_ptr->fingerprint = (fingerprint[0] << 24) + (fingerprint[1] << 16) +
             (fingerprint[2] << 8) + fingerprint[3];

    hmac_sha512(masterhdNode_ptr->chainCode.octets, CRYPTO_CHAIN_CODE_SZ, data, sizeof(data), I);

    /* Copy first 32 bytes to private key. */
    memcpy(derivehdNode_ptr->privateKey.octets, I, CRYPTO_PRIVATE_KEY_SZ);
    if ((ret = secp256k1_ec_privkey_tweak_add(derivehdNode_ptr->privateKey.octets, masterhdNode_ptr->privateKey.octets)) != 1) {
         return (OTK_RETURN_FAIL);
    }

    memcpy(derivehdNode_ptr->chainCode.octets, I + CRYPTO_PRIVATE_KEY_SZ, CRYPTO_CHAIN_CODE_SZ);
    derivehdNode_ptr->depth = masterhdNode_ptr->depth + 1;
    derivehdNode_ptr->childNum = index;

    /* Generate public key. */
    if (crypto_derivePublicKeyFromPrivateKey(derivehdNode_ptr) != OTK_RETURN_OK) {
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}

/*
 * ======== crypto_serializedHexPublicKey() ========
 * Serialize HD extended key.
 *
 * 4 byte: version bytes (mainnet: 0x0488B21E public, 0x0488ADE4 private; testnet: 0x043587CF public,
 *   0x04358394 private)
 * 1 byte: depth: 0x00 for master nodes, 0x01 for level-1 derived keys, ....
 * 4 bytes: the fingerprint of the parent's key (0x00000000 if master key)
 * 4 bytes: child number. This is ser32(i) for i in xi = xpar/i, with xi the key being serialized.
 *   (0x00000000 if master key)
 * 32 bytes: the chain code
 * 33 bytes: the public key or private key data (serP(K) for public keys, 0x00 || ser256(k) for private keys)
 */
void crypto_serializedExtPublicKey(
    CRYPTO_HDNode *hdNode_ptr)
{
    memset(hdNode_ptr->extPublickey.str_ptr, 0, CRYPTO_EXT_PUBLIC_KEY_MAX_SZ);

    char    *dest_ptr = hdNode_ptr->extPublickey.str_ptr;
    size_t   destLen  = CRYPTO_EXT_PUBLIC_KEY_MAX_SZ;
    uint32_t version = 0x0488B21E;
    uint8_t depth = hdNode_ptr->depth;
    uint32_t fingerprint = hdNode_ptr->fingerprint;
    uint32_t childNum = hdNode_ptr->childNum;
    uint8_t *chainCode_ptr = hdNode_ptr->chainCode.octets;
    uint8_t *key_ptr = hdNode_ptr->publicKey.octets;
    bool     isPublicKey = true;
    uint8_t nodeData[78];
    uint8_t *cur_ptr = nodeData;

#if defined(TESTNET)
    version = 0x043587CF;
#endif
    crypto_serialize32(version, cur_ptr);
    cur_ptr += 4;
    *cur_ptr = depth;
    cur_ptr += 1;
    crypto_serialize32(fingerprint, cur_ptr);
    cur_ptr += 4;
    crypto_serialize32(childNum, cur_ptr);
    cur_ptr += 4;
    memcpy(cur_ptr, chainCode_ptr, CRYPTO_CHAIN_CODE_SZ);
    cur_ptr += CRYPTO_CHAIN_CODE_SZ;
    if (isPublicKey) {
        memcpy(cur_ptr, key_ptr, CRYPTO_PUBLIC_KEY_SZ);
    } else {
        *cur_ptr = 0;
        cur_ptr += 1;
        memcpy(cur_ptr, key_ptr, CRYPTO_PRIVATE_KEY_SZ);
    }
   
    base58_encode_check(nodeData, 78, dest_ptr, destLen);
}

/*
 * ======== crypto_serializedWIFPrivateKey() ========
 * Serialize HD node's private key to WIF formate string.
 *
 */
void crypto_serializedWIFPrivateKey(
    CRYPTO_HDNode *hdNode_ptr)
{
    memset(hdNode_ptr->WIFPrivatekey.str_ptr, 0, CRYPTO_WIF_PRIVATE_KEY_SZ);

    char    *dest_ptr = hdNode_ptr->WIFPrivatekey.str_ptr;
    uint8_t extKey[CRYPTO_PRIVATE_KEY_SZ + 2];

    extKey[0] = 0x80;
    memcpy(extKey + 1, hdNode_ptr->privateKey.octets, CRYPTO_PRIVATE_KEY_SZ);
    extKey[CRYPTO_PRIVATE_KEY_SZ + 1] = 0x01;
 
    base58_encode_check(extKey, CRYPTO_PRIVATE_KEY_SZ + 2, dest_ptr, CRYPTO_WIF_PRIVATE_KEY_SZ);
}

/*
 * ======== crypto_calcBtcAddr() ========
 * Convert compressed public key to Bitcoin address
 *
 * Parameters: com
 *
 * Returns:
 */
static int crypto_calcBtcAddr(
    CRYPTO_HDNode *hdNode_ptr)
{
    __INIT_CHECK__
    uint8_t _sha256Result[SHA256_DIGEST_LENGTH];
    sha256_Raw(hdNode_ptr->publicKey.octets, CRYPTO_PUBLIC_KEY_SZ, _sha256Result);

    uint8_t _addressBase[21];
    memset(_addressBase, 0, sizeof(_addressBase));
#if defined(TESTNET)
    _addressBase[0] = 0x6f;
#endif
    ripemd160(_sha256Result, SHA256_DIGEST_LENGTH, _addressBase + 1);

    return base58_encode_check(_addressBase, 21, hdNode_ptr->btcAddr.str_ptr, CRYPTO_BITCOIN_ADDR_SZ);
}

uint32_t CRYPTO_rng32(void)
{
    __INIT_CHECK__
    ret_code_t errCode;
    uint32_t rnd32 = 0;

    errCode = nrf_crypto_rng_init(NULL, NULL);
    APP_ERROR_CHECK(errCode);
    errCode = nrf_crypto_rng_vector_generate((uint8_t *)&rnd32, sizeof(rnd32));
    APP_ERROR_CHECK(errCode);

    return (rnd32);
}

void CRYPTO_dumpHDNode(
    CRYPTO_HDNode *node_ptr)
{
    if (NULL == node_ptr) {
        OTK_LOG_ERROR("Node is NULL");
        return;
    }
    NRF_LOG_INFO("HD Node:");
    NRF_LOG_INFO("Fingerprint: %x", node_ptr->fingerprint);
    NRF_LOG_INFO("Depth: %d", node_ptr->depth);
    NRF_LOG_INFO("ChildNum: %x", node_ptr->childNum);
    NRF_LOG_INFO("Private key:");
    OTK_LOG_HEXDUMP(node_ptr->privateKey.octets, sizeof(node_ptr->privateKey));
    NRF_LOG_INFO("Chain code:");
    OTK_LOG_HEXDUMP(node_ptr->chainCode.octets, sizeof(node_ptr->chainCode));
    NRF_LOG_INFO("Public key:");
    OTK_LOG_HEXDUMP(node_ptr->publicKey.octets, sizeof(node_ptr->publicKey));
}

OTK_Return CRYPTO_generateRandomSeed(
    CRYPTO_seed *seed)
{
    __INIT_CHECK__

    ret_code_t errCode;

    /* Generate random number. */
    if ((errCode = nrf_crypto_rng_vector_generate(seed->octets, CRYPTO_SEED_SIZE_OCTET)) != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}


OTK_Return CRYPTO_generateRandomPath(
    CRYPTO_derivativePath *path_ptr)
{
    __INIT_CHECK__
    ret_code_t errCode;

    int i;
    uint32_t index;

    for (i = 0; i < CRYPTO_DERIVATIVE_DEPTH; i++) {
        /* Generate random number. */
        if ((errCode = nrf_crypto_rng_vector_generate((uint8_t *)&index, sizeof(index))) != NRF_SUCCESS) {
            OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
            return (OTK_RETURN_FAIL);
        }
        path_ptr->derivativeIndex[i] = (index % 0x80000000);        
    }

    return (OTK_RETURN_OK);
}

OTK_Return CRYPTO_setDerivativePath(
    CRYPTO_derivativePath *path_ptr,
    uint8_t depth,
    uint32_t value)
{
    if (NULL == path_ptr || depth >= CRYPTO_DERIVATIVE_DEPTH) {
        return (OTK_RETURN_FAIL);
    }
    path_ptr->derivativeIndex[depth] = (value % 0x80000000);        

    return (OTK_RETURN_OK);
}

/*
 * ======== CRYPTO_isHDNodeValid() ========
 * Check if the given HD node is valid.
 *
 * Parameters:
 *
 * Returns:
 */
bool 
CRYPTO_isHDNodeValid(
    CRYPTO_HDNode *node_ptr)
{
    if (NULL == node_ptr) {
        return (false);
    }
    /* Only check if private key, public key and chain code are not empty. */ 
    if ((0 == memcmp(node_ptr->chainCode.octets, empty, CRYPTO_CHAIN_CODE_SZ)) ||
            (0 ==  memcmp(node_ptr->privateKey.octets, empty, CRYPTO_PRIVATE_KEY_SZ)) ||
            (0 == memcmp(node_ptr->publicKey.octets, empty, CRYPTO_PUBLIC_KEY_SZ))) {
        return (false);
    }
    return (true);
}

OTK_Return CRYPTO_deriveHdNode(
    CRYPTO_HDNode           *masterhdNode_ptr,
    CRYPTO_HDNode           *derivehdNode_ptr,
    CRYPTO_derivativePath   *path_ptr,
    CRYPTO_seed             *seed_ptr)
{
    __INIT_CHECK__

    uint8_t I[CRYPTO_PRIVATE_KEY_SZ + CRYPTO_CHAIN_CODE_SZ] = {0};
    CRYPTO_derivativePath _randomPath;
    CRYPTO_derivativePath *_prtPath;
    CRYPTO_seed _randomSeed;
    CRYPTO_seed  *_ptrSeed;


    if (NULL == masterhdNode_ptr ) {
        /* No master node presented, the derivative is a master node */
        if (NULL == derivehdNode_ptr) {
            /* Derivative node is invalid */
            OTK_LOG_ERROR("Generate master node failed, derivehdNode_ptr is NULL!!");
            return (OTK_RETURN_FAIL);
        }
        else {
            memset(derivehdNode_ptr, 0, sizeof(CRYPTO_HDNode));
            derivehdNode_ptr->depth = 0;
            derivehdNode_ptr->fingerprint = 0x00000000;
            derivehdNode_ptr->childNum = 0;

            if (NULL == seed_ptr) {
                /* Seed is not presented, generate a random seed */
                if (OTK_RETURN_OK != CRYPTO_generateRandomSeed(&_randomSeed)) {
                    OTK_LOG_ERROR("Failed to generate random seed.");
                    return (OTK_RETURN_FAIL);
                }
                _ptrSeed = &_randomSeed;
            }
            else {
                /* Seed is presented, using given seed */                
                _ptrSeed = seed_ptr;
            }

#if CRYPTO_DEBUG_INFO & 0 
            NRF_LOG_INFO("Random Seed Vector:");
            OTK_LOG_HEXDUMP(&_randomSeed, sizeof(_randomSeed));
#endif /* CRYPTO_DEBUG_INFO */

            /* Seed is not presented, generate a master node with given seed */
            hmac_sha512((const uint8_t *)_hmac_sha512Key, strlen(_hmac_sha512Key), _ptrSeed->octets, CRYPTO_SEED_SIZE_OCTET, I);

            memcpy(derivehdNode_ptr->privateKey.octets, I, CRYPTO_PRIVATE_KEY_SZ);
            memcpy(derivehdNode_ptr->chainCode.octets, I + CRYPTO_PRIVATE_KEY_SZ, CRYPTO_CHAIN_CODE_SZ);

            /* Generate public key. */
            if (OTK_RETURN_OK != 
                crypto_derivePublicKeyFromPrivateKey(derivehdNode_ptr)) {
                return (OTK_RETURN_FAIL);
            }
        }
    }
    else {
        if (NULL == derivehdNode_ptr) {
            /* Derivative node is invalid */
            OTK_LOG_ERROR("Generate derivative node failed, derivehdNode_ptr is NULL!!");
            return (OTK_RETURN_FAIL);
        }
        else {
            /* Derivative node is valid, try to derive a child node */
            if (!CRYPTO_isHDNodeValid(masterhdNode_ptr)) {
                /* Master node is not valid */
                OTK_LOG_ERROR("Generate derivative node failed, masterhdNode_ptr is invalid!!");
                return (OTK_RETURN_FAIL);
            }
            else {
                if (NULL == path_ptr) {
                    /* Derivative path is not presented, generate a random path */
                    if (OTK_RETURN_OK != CRYPTO_generateRandomPath(&_randomPath)) {
                        OTK_LOG_ERROR("Failed to generate random seed.");
                        return (OTK_RETURN_FAIL);
                    }
                    _prtPath = &_randomPath;
                }
                else {
                    /* Derivative path is presented, using given path */
                    _prtPath = path_ptr;
                }

                CRYPTO_HDNode _masterNode;
                memcpy(&_masterNode, masterhdNode_ptr, sizeof(CRYPTO_HDNode));
                for (int i = 0; i < CRYPTO_DERIVATIVE_DEPTH; i++) {
                    crypto_childKeyDerivation(&_masterNode, derivehdNode_ptr, _prtPath->derivativeIndex[i]);
                    memcpy(&_masterNode, derivehdNode_ptr, sizeof(CRYPTO_HDNode));
                }
            }
        }
    }

    utils_bin_to_hex(derivehdNode_ptr->publicKey.octets, CRYPTO_PUBLIC_KEY_SZ, derivehdNode_ptr->hexPublickey.str_ptr);
    crypto_serializedExtPublicKey(derivehdNode_ptr);
    crypto_serializedWIFPrivateKey(derivehdNode_ptr);
    crypto_calcBtcAddr(derivehdNode_ptr);

#if CRYPTO_DEBUG_INFO
    CRYPTO_dumpHDNode(derivehdNode_ptr);
#endif
    return (OTK_RETURN_OK);
}

/*
 * ======== CRYPTO_sign() ========
 * Sign a hash using given private key.
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return CRYPTO_sign(
    CRYPTO_privateKey  *privateKey_ptr,
    const uint8_t *hash_ptr,
    size_t         hashLen,
    CRYPTO_signature      *signature_ptr)
{
    __INIT_CHECK__
    nrf_crypto_ecdsa_secp256k1_signature_t signature;
    nrf_crypto_ecc_private_key_t priKeyInt;
    ret_code_t errCode = NRF_SUCCESS;

#if CRYPTO_DEBUG_INFO
    NRF_LOG_INFO("HASH:");
    OTK_LOG_HEXDUMP(hash_ptr, hashLen);
#endif /* CRYPTO_DEBUG_INFO */    
    /* Converts raw private key to internal representation */
    errCode = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256k1_curve_info,
           &priKeyInt, privateKey_ptr->octets, CRYPTO_PRIVATE_KEY_SZ);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }

    size_t _sigLen = CRYPTO_SIGNATURE_SZ;
    nrf_crypto_ecdsa_sign_context_t sign_context;
    /* Generates signature using ECDSA and SHA-256. */
    errCode = nrf_crypto_ecdsa_sign(&sign_context, &priKeyInt, hash_ptr, hashLen, signature, &_sigLen);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }

    char sighex[CRYPTO_SIGNATURE_SZ * 2 + 1];
    utils_bin_to_hex(signature, CRYPTO_SIGNATURE_SZ, sighex);

#if CRYPTO_DEBUG_INFO    
    NRF_LOG_INFO("Signature: %s", sighex);
    OTK_LOG_HEXDUMP(signature, sizeof(signature));
#endif /* CRYPTO_DEBUG_INFO */    

    /* Free key. */
    errCode = nrf_crypto_ecc_private_key_free(&priKeyInt);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }
    /* Copy signature to destination. */
    memcpy(signature_ptr->octets, signature, CRYPTO_SIGNATURE_SZ);

    return (OTK_RETURN_OK);
}

/*
 * ======== CRYPTO_verify() ========
 * Verify given signature which is signed by the correspoding private key with the public key
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return CRYPTO_verify(
    CRYPTO_publicKey   *publicKey_ptr,
    const uint8_t *hash_ptr,
    size_t         hashLen,
    CRYPTO_signature      *signature_ptr)    
{
    __INIT_CHECK__
    nrf_crypto_ecc_public_key_t pubKeyInt;
    ret_code_t errCode = NRF_SUCCESS;
    OTK_Return ret = OTK_RETURN_FAIL;
    CRYPTO_decompPublicKey  decompressedPubKey;

    /* Decompress public key. */
    secp256k1_context *ctx_ptr = (secp256k1_context *)1; /* Give a non-null value. */
    if (1 != secp256k1_ec_pubkey_parse(
            ctx_ptr, 
            (secp256k1_pubkey *)decompressedPubKey.octets, 
            publicKey_ptr->octets, 
            CRYPTO_PUBLIC_KEY_SZ)) {
        OTK_LOG_ERROR("Decompress public key failed!!");
        return (OTK_RETURN_FAIL);
    }

    /* Converts raw private key to internal representation */
    errCode = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_secp256k1_curve_info,
           &pubKeyInt, decompressedPubKey.octets, CRYPTO_DECOMP_PUBLIC_KEY_SZ);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
        return (OTK_RETURN_FAIL);
    }

    /* Verify the signature using ECDSA and SHA-256. */
    errCode = nrf_crypto_ecdsa_verify(NULL, &pubKeyInt, hash_ptr, hashLen, signature_ptr->octets, CRYPTO_SIGNATURE_SZ);
    if (errCode == NRF_SUCCESS) {
        ret = OTK_RETURN_OK;
    }

    /* Free key. */
    errCode = nrf_crypto_ecc_public_key_free(&pubKeyInt);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("Error 0x%04X: %s", errCode, nrf_crypto_error_string_get(errCode));
    }
    return (ret);
}

OTK_Return CRYPTO_init() 
{
    ret_code_t errCode;

    if (_initiated) {
        return (OTK_RETURN_OK);
    }

    /* Init mem */
    errCode = nrf_mem_init();
    APP_ERROR_CHECK(errCode);
    if (NRF_SUCCESS != errCode) {
        OTK_LOG_ERROR("nrf_mem_init failed!!");        
        return (OTK_RETURN_FAIL);        
    }

    /* Init crypto */
    errCode = nrf_crypto_init();
    APP_ERROR_CHECK(errCode);
    if (NRF_SUCCESS != errCode) {
        OTK_LOG_ERROR("nrf_crypto_init failed!!");                
        return (OTK_RETURN_FAIL);        
    }

    /* Init rng. */
    errCode = nrf_crypto_rng_init(NULL, NULL);
    APP_ERROR_CHECK(errCode);
    if (NRF_SUCCESS != errCode) {
        OTK_LOG_ERROR("nrf_crypto_rng_init failed!!");       
        return (OTK_RETURN_FAIL);        
    }

    _initiated = true;

    return (OTK_RETURN_OK);
}
