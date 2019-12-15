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
#include <stdlib.h>
#include <string.h>
#include "app_scheduler.h"
#include "app_timer.h"
#include "boards.h"
#include "nfc_t4t_lib.h"
#include "nfc_launchapp_msg.h"
#include "nfc_ndef_msg.h"
#include "nfc_launchapp_rec.h"
#include "nfc_ndef_record.h"
#include "nfc_ndef_msg_parser.h"
#include "nfc_text_rec.h"
#include "nfc_uri_msg.h"
#include "nrf_assert.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "mem_manager.h"
#include "libbtc/sha2.h"
#include "libbtc/base58.h"
#include "libbtc/utils.h"
#include "otk.h"
#include "nfc.h"
#include "led.h"
#include "crypto.h"
#include "key.h"

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif

#define NRF_LOG_MODULE_NAME otk_nfc
NRF_LOG_MODULE_REGISTER();

/* NFC tools Android application package name */
static const uint8_t m_android_package_name[] = {'c','o','m','.','c','y','p','h','e','r','e','c','o','.','o','p','e','n','t','u','r','n','k','e','y'};
static const uint8_t en_code[] = {'e', 'n'};

static bool m_nfc_started = false;                  /* (True), if NFC is running. */
static bool m_nfc_restart_flag = false;             /* (True), whether to restart NFC. */
static bool m_nfc_polling_started = false;          /* (True), NFC tag is polled */
static bool m_nfc_read_finished = false;            /* (True), if NFC data has been read.*/
static bool m_nfc_output_protect_data = false;      /* (True), if protected data has been read. */
static bool m_nfc_more_cmd = false;                 /* (True), if there is more command to come.*/
static bool m_nfc_security_shutdown = false;
static uint8_t m_nfc_auth_failure_counts = 0;

static NFC_REQUEST_COMMAND m_nfc_request_command = NFC_REQUEST_CMD_INVALID;                   /* Vairable to store passing in reqeuset command. */
static NFC_COMMAND_EXEC_STATE m_nfc_cmd_exec_state = NFC_CMD_EXEC_NA;
static NFC_COMMAND_EXEC_FAILURE_REASON m_nfc_cmd_failure_reason = NFC_REASON_INVALID;

/* Global buffer for NDEF read/write. */
static uint8_t m_ndef_msg_buf[NFC_NDEF_MSG_BUF_SZ];

static uint32_t m_nfc_session_id = 0;               /* UINT32 random number as identification for each session. */
static uint32_t m_nfc_request_id = 0;               /* UINT32 request ID submit via NFC request to be encoded in session data for validity check. */
static char m_nfc_request_data_buf[NFC_REQUEST_DATA_BUF_SZ];  /* Buffer to store passed in request data. */
static char m_nfc_request_opt_buf[NFC_REQUEST_OPT_BUF_SZ];    /* Buffer to store passed in request option. */

/**
 * @brief Typedef for safely calling external function which
 * has I/O access, i.e. UART, FPS, etc.
 */
typedef void (*deferredFunc_t)(void );
static deferredFunc_t deferredFunc = NULL;

void _schedShutdown_aft_unlock() {
    LED_all_off();
    LED_on(OTK_LED_GREEN);
    nrf_delay_ms(2400);
    OTK_shutdown(OTK_ERROR_NO_ERROR, false);    
}

/**
 * @brief Clear stored request command and data
 *
 */
static void nfc_clearRequest() 
{
    m_nfc_request_id = 0;
    m_nfc_request_command = NFC_REQUEST_CMD_INVALID;                   /* Vairable to store passing in reqeuset command. */

    memset(m_nfc_request_data_buf, 0, NFC_REQUEST_DATA_BUF_SZ);
    memset(m_nfc_request_opt_buf, 0, NFC_REQUEST_OPT_BUF_SZ);
}

/**
 * @brief Check if a sting is a valid hex string
 * @param[in]   str        Pointer to hex string
 *
 * @return      (true) if string is valid hex, (false) otherwise
 */
static bool nfc_isStrHex(char *str)
{

    if (str[strspn(str, "0123456789abcdefABCDEF")] == 0)
    {
        return (true);
    }
    return (false);
}

/**
 * @brief Convert string to UINT32 value
 * @param[in]   str            Pointer to hex string
 *
 * @return      UINT32 value for valid hex string, 0 for otherwise.
 */
static uint32_t nfc_strToUint32(char *str) {
    uint32_t ret;
    char     *ptr;

    ret = strtoul(str, &ptr, 10);
    if (strlen(ptr) > 0) {
        ret = 0;
    }

    return (ret);
}

/**
 * @brief NRF app_scheduler callback function for safely I/O context switch
 * @param[in]   data_ptr       Pointer of passed in data, not used.
 * @param[in]   dataSize       Pointer of passed in data length , not used.
 */
static void nfc_execDeferredFunc(
    void    *data_ptr,
    uint16_t dataSize)
{
    UNUSED_PARAMETER(data_ptr);
    UNUSED_PARAMETER(dataSize);

    deferredFunc();
}

/**
 * @brief Stop NFC interface safely in a scheduler task.
 * @param[in]   data_ptr       Pointer of passed in data, not used.
 * @param[in]   dataSize       Pointer of passed in data length , not used.
 */
static void nfc_safelyStop(
    void    *data_ptr,
    uint16_t dataSize)
{
    UNUSED_PARAMETER(data_ptr);
    UNUSED_PARAMETER(dataSize);

    /* Duplicated calling protection. */
    if (m_nfc_started) {
        /* Stop sensing NFC field */
        if (NRF_SUCCESS != nfc_t4t_emulation_stop()) {
            OTK_LOG_ERROR("Stop NFC failed!");
        }
        else {
            OTK_LOG_DEBUG("NFC stopped.");
            m_nfc_started = false;
        }

        if (m_nfc_restart_flag) {
            OTK_LOG_DEBUG("NFC restarting...");
            m_nfc_restart_flag = false;
            NFC_start();
        }
    }
}

/**
 * @brief NFC callback function, @ref nfc_t4t_callback_t for params detail.
 * Handling NFC events
 *  - Control LED incidation.
 *  - Processing request data.
 *  - Set tasks.
 */
static void nfc_callback(
    void           *context,
    nfc_t4t_event_t event,
    const uint8_t * data_ptr,
    size_t          dataLength,
    uint32_t        flags)
{
    (void)context;

    uint8_t _record_idx = 0;

    switch (event)
    {
        /* External Reader polling detected. */
        case NFC_T4T_EVENT_FIELD_ON:
            if (!m_nfc_polling_started) {
                OTK_LOG_DEBUG("NFC reader polling started...");

                m_nfc_polling_started = true;
                OTK_pause();
                LED_setCadenceType(LED_CAD_NFC_POLLING);        
                LED_cadence_start();
                OTK_extend();
            }
            
            if (true == m_nfc_read_finished && m_nfc_output_protect_data) {
                /* NFC reader had read proteded data, do nothing here 
                 * and prepare to shutdown when NFC polling ended. */
                return;
            }

            /* Reading NFC data requires several polling detections before 
             * reading data. Take necessary actions only after data is read,
             * or received a request command, and ingore the rest.
             */            
            if (true == m_nfc_read_finished && m_nfc_request_command != NFC_REQUEST_CMD_INVALID) {
                OTK_LOG_DEBUG("NFC reader updated with request command (%d)", m_nfc_request_command);;

                static NFC_COMMAND_EXEC_STATE _execState = NFC_CMD_EXEC_SUCCESS;
                m_nfc_read_finished = false;

                OTK_LOG_DEBUG("Request Received:");
                OTK_LOG_DEBUG("Request ID: (%lu)", m_nfc_request_id);
                OTK_LOG_DEBUG("Request Command: (%d)", m_nfc_request_command);
                OTK_LOG_DEBUG("Request Data: (%d) %s", strlen(m_nfc_request_data_buf), m_nfc_request_data_buf);
                OTK_LOG_DEBUG("Request Option: (%d) %s", strlen(m_nfc_request_opt_buf), m_nfc_request_opt_buf);

                if (NFC_REQUEST_CMD_CANCEL == m_nfc_request_command) {
                    m_nfc_cmd_exec_state = NFC_CMD_EXEC_NA;
                    m_nfc_cmd_failure_reason = NFC_REASON_INVALID;                    
                    nfc_clearRequest();
                    NFC_stop(true);
                    OTK_standby();
                    break;
                }

                OTK_Error err = OTK_ERROR_NO_ERROR;

                if (!OTK_isAuthorized() && 
                    strstr(m_nfc_request_opt_buf, "pin=") != NULL &&
                    m_nfc_request_command != NFC_REQUEST_CMD_EXPORT_WIF_KEY) {
                    /*
                     * For security concerns, export WIF private key must be authorized by 
                     * Fingerprint, so it will not accept PIN code authoriztion. 
                     */
                    /* Parse Option Params and check if PIN code is submitted and matched
                     * Request Options are order irrelavant params separated by comma
                     * as in the following foramt:
                     * key=0,pin=99999999
                     */
                    err = OTK_pinValidate(m_nfc_request_opt_buf);  
                    OTK_LOG_DEBUG("PIN validation (%d), Error: (%d)", OTK_isAuthorized(), err);
                }

                if (OTK_isAuthorized() || NFC_REQUEST_CMD_RESET == m_nfc_request_command) {
                    OTK_extend();

                    /* Handling NFC request and set correspondent tasks. */
                    switch (m_nfc_request_command) {
                        case NFC_REQUEST_CMD_UNLOCK:
                            if (OTK_isLocked() == false) {
                                _execState = NFC_CMD_EXEC_FAIL;
                                m_nfc_cmd_failure_reason = NFC_REASON_UNLOCKED_ALREADY;
                            }
                            else {
                                deferredFunc = OTK_unlock;
                            }
                            break;
                        case NFC_REQUEST_CMD_SET_KEY:
                            err = OTK_setKey(m_nfc_request_data_buf);
                            break;
                        case NFC_REQUEST_CMD_SET_PIN:
                            err = OTK_setPin(m_nfc_request_data_buf);
                            break;
                        case NFC_REQUEST_CMD_SET_NOTE:
                            err = OTK_setNote(m_nfc_request_data_buf);
                            break;
                        case NFC_REQUEST_CMD_RESET:
                            NFC_stop(false);
                            deferredFunc = OTK_reset;
                            break;
                        case NFC_REQUEST_CMD_LOCK:
                        case NFC_REQUEST_CMD_SHOW_KEY:
                        case NFC_REQUEST_CMD_SIGN:  
                        case NFC_REQUEST_CMD_CANCEL:
                        case NFC_REQUEST_CMD_EXPORT_WIF_KEY:
                                _execState = NFC_CMD_EXEC_SUCCESS;
                            break;
                        case NFC_REQUEST_CMD_INVALID:
                        case NFC_REQUEST_CMD_LAST:         
                                _execState = NFC_CMD_EXEC_FAIL;
                                m_nfc_cmd_failure_reason = NFC_REASON_CMD_INVALID;
                            break;                   
                    }

                    /* Schedule command execution and clear request command but keep the data. */
                    if (NULL != deferredFunc) {
                        if (app_sched_queue_space_get() > 0) {
                            APP_ERROR_CHECK(app_sched_event_put(NULL, 0, nfc_execDeferredFunc));
                        }
                        else {
                            OTK_LOG_ERROR("Schedule NFC command failed, scheduler full!!");
                            OTK_shutdown(OTK_ERROR_SCHED_ERROR,false);
                        }
                    }            

                    if (err != OTK_ERROR_NO_ERROR) {
                        _execState = NFC_CMD_EXEC_FAIL;

                        if (OTK_ERROR_NOTE_TOO_LONG == err) {
                            m_nfc_cmd_failure_reason = NFC_REASON_INVALID_NOTE_TOO_LONG;
                        }
                        else if (OTK_ERROR_INVALID_PIN == err) {
                            m_nfc_cmd_failure_reason = NFC_REASON_INVALID_PIN;
                        }
                        else if (OTK_ERROR_INVALID_KEYPATH == err) {
                            m_nfc_cmd_failure_reason = NFC_REASON_INVALID_KEYPATH;
                        }
                    }

                    m_nfc_cmd_exec_state = _execState;
                }
                else {
                    m_nfc_cmd_exec_state = NFC_CMD_EXEC_FAIL;
                    if (OTK_ERROR_PIN_UNSET == err) {
                        m_nfc_cmd_failure_reason = NFC_REASON_PIN_UNSET;
                    }
                    else {
                        m_nfc_cmd_failure_reason = NFC_REASON_AUTH_FAILED;                        
                    }
                    m_nfc_auth_failure_counts++;

                    if (m_nfc_auth_failure_counts > 2) {
                        OTK_LOG_ERROR("Authentication failed three times, prepare OTK shutdown!");                       
                        m_nfc_security_shutdown = true;
                    }
                }

                NFC_stop(true);
                OTK_standby();
            }           
            break;

        /* External Reader polling ended. */
        case NFC_T4T_EVENT_FIELD_OFF:
            if (m_nfc_polling_started) {
                /* Reading NFC data requires several polling detections before 
                 * reading data. Take necessary actions only after data is read,
                 * or received a request command, and ingore the rest.
                 */            
                OTK_LOG_DEBUG("NFC reader polling ended!");
                m_nfc_polling_started = false;

                if (m_nfc_read_finished) {
                    m_nfc_read_finished = false;

                    if (m_nfc_output_protect_data && !m_nfc_more_cmd) {
                        OTK_LOG_DEBUG("Protected data is read, prepare shutdown.");
                        OTK_shutdown(OTK_ERROR_NO_ERROR, false);
                        return;
                    }
                    NFC_stop(true);
                }
                OTK_standby();
            }
            break;

        /* External Reader has read static NDEF-Data from Emulation. */            
        case NFC_T4T_EVENT_NDEF_READ:
            OTK_LOG_DEBUG("NFC reader read completed!");
            /* Occurs only once per NFC reading. */
            m_nfc_read_finished = true;
            m_nfc_cmd_exec_state = NFC_CMD_EXEC_NA;
            m_nfc_request_command = NFC_REQUEST_CMD_INVALID;
            m_nfc_cmd_failure_reason = NFC_REASON_INVALID;

            if (m_nfc_security_shutdown) {
                OTK_LOG_ERROR("Invalid request or Authentication Failed, Shutting down OTK!");
                OTK_shutdown(OTK_ERROR_NFC_INVALID_REQUEST, false);                                        
            }
            break;

        /* External Reader has written to length information of NDEF-Data from Emulation. */
        case NFC_T4T_EVENT_NDEF_UPDATED:

            /* Occur twice per writing,
             * First, with data length 0 to indicate start of writing.
             * Second, with complete data length of written data.
             *
             * Set data size upper boundary to avoid overflow attacks.
             *
             * If data is written, decode, parse, and validate whether data is valid request.
             * If so, store request to be processing at polling end.
             *
             * NOTE! DO NOT add too many log or delay in the callback which will cause a 
             * timeout on the client device resulted a "Written Error".
             *
             */
            if (dataLength > 0 && dataLength < NFC_MAX_RECORD_SZ) {
                _record_idx = 0;
                OTK_LOG_DEBUG("NFC reader write data (%d) bytes.", dataLength);

                /* Parse it. */                
                uint8_t *ndef_record_ptr = m_ndef_msg_buf + NLEN_FIELD_SIZE;
                bool _invalidRequest = false;

                do {
                    ret_code_t errCode;

                    nfc_ndef_bin_payload_desc_t payloadDescription;
                    nfc_ndef_record_desc_t recordDesc;
                    nfc_ndef_record_location_t recordLocation;
                    uint32_t _descLen = dataLength;

                    uint8_t _recordPayload[NFC_MAX_RECORD_SZ] = {0};
                    uint32_t _payloadLen = NFC_MAX_RECORD_SZ;
                    uint32_t _u32val = 0;

                    errCode = ndef_record_parser(&payloadDescription, 
                                                    &recordDesc, 
                                                    &recordLocation, 
                                                    ndef_record_ptr, 
                                                    &_descLen);
                    APP_ERROR_CHECK(errCode);


                    recordDesc.payload_constructor(recordDesc.p_payload_descriptor, 
                                                    _recordPayload, 
                                                    &_payloadLen);

                    /* Takes data for process, escape leading 'en' encode string. */
                    char *_recordPayload_data = (char*)_recordPayload + sizeof(en_code) + 1;

                    /* check record one by one and break out whenever invalid data is found.*/
                    switch (_record_idx) {
                        case NFC_REQUEST_DEF_SESSION_ID:
                            _u32val = nfc_strToUint32(_recordPayload_data);
#ifndef DEBUG                          
                            if (_u32val != m_nfc_session_id) {
                                OTK_LOG_ERROR("Invalid request session ID!! (%s)", _recordPayload_data);
                                _invalidRequest = true;                                
                            }
#endif                            
                            break;    
                        case NFC_REQUEST_DEF_COMMAND_ID:
                            _u32val = nfc_strToUint32(_recordPayload_data);                        
                            if (_u32val == 0) {
                                OTK_LOG_ERROR("Invalid request command ID!! (%s)", _recordPayload_data);
                                _invalidRequest = true;                                
                            }
                            else {
                                m_nfc_request_id = _u32val;
                            }
                            break;   
                        case NFC_REQUEST_DEF_COMMAD:
                            _u32val = nfc_strToUint32(_recordPayload_data);
                            if (_u32val >= NFC_REQUEST_CMD_LOCK && _u32val < NFC_REQUEST_CMD_LAST) {
                                m_nfc_request_command = _u32val;
                            }
                            else {
                                OTK_LOG_ERROR("Invalid request command!! (%s)", _recordPayload_data);
                                _invalidRequest = true;                                
                            }
                            break;
                        case NFC_REQUEST_DEF_DATA:
                            if (m_nfc_request_command >= NFC_REQUEST_CMD_SIGN &&
                                m_nfc_request_command < NFC_REQUEST_CMD_CANCEL &&
                                strlen(_recordPayload_data) > 0 &&
                                strlen(_recordPayload_data) < NFC_REQUEST_DATA_BUF_SZ) {                              
                                sprintf(m_nfc_request_data_buf, "%s", _recordPayload_data);
                            }
                            else {
                                OTK_LOG_DEBUG("Request data is not used, ignore it!");
                            }
                            break;
                        case NFC_REQUEST_DEF_OPTION:
                            m_nfc_more_cmd = false;
                            if (strlen(_recordPayload_data) > 0 && strlen(_recordPayload_data) < NFC_REQUEST_OPT_BUF_SZ) {
                                sprintf(m_nfc_request_opt_buf, "%s", _recordPayload_data);   

                                char *strPos = strstr(m_nfc_request_opt_buf, "more=");
                                if (strPos != NULL) {
                                    char *ptrTail;                           
                                    strPos += strlen("more=");
                                    m_nfc_more_cmd = (1 == strtoul(strPos, &ptrTail, 10));
                                    if (m_nfc_more_cmd) {
                                        OTK_extend();
                                    }
                                }
                            }
                            break;
                        default:
                            OTK_LOG_ERROR("Too many records!!");
                            _invalidRequest = true;
                            break;
                    }

                    /* if request is not valid, reset local variable. */
                    if (_invalidRequest) {
                        OTK_LOG_ERROR("Invalid request, prepare OTK shutdown!");
                        m_nfc_security_shutdown = true;
                    }
                    ndef_record_ptr += _descLen;
                    dataLength -= _descLen;
                    _record_idx++;
                    OTK_extend();
                } while (dataLength > 0);
            }                
            break;
        default:
            OTK_LOG_ERROR("Unhandled NFC Event - (%0xX)", event);
            break;
    }
}

/**
 * @brief Set NFC records based on valid conditions.
 * All NFC records is constructed here.
 * For protected data (full key info) and calculation result (sign data signature),
 * only be constructed when conditions are met. 
 *
 * @return   OTK_Return     OTK_RETURN_OK if no error, OTK_RETURN_FAIL otherwise.
 */
static OTK_Return nfc_setRecords()
{

    ret_code_t errCode;

    uint32_t _ndef_msg_len =  sizeof(m_ndef_msg_buf);
    uint32_t _record_length;
    uint8_t _dblhash[SHA256_DIGEST_LENGTH] = {0};
    char *_sigHex_ptr = NULL;

    m_nfc_output_protect_data = false;

    NFC_NDEF_MSG_DEF(nfc_msg, NFC_MAX_RECORD_COUNT);
    nfc_ndef_msg_desc_t *_ndef_msg_desc_ptr = &NFC_NDEF_MSG(nfc_msg);

    OTK_LOG_DEBUG("\r\n== NFC Records Start ==");

    /* 1. Application Package URI*/
    NFC_NDEF_RECORD_BIN_DATA_DEF(nfc_record_app_uri, TNF_EXTERNAL_TYPE, NULL, 0,
            m_android_package_name, sizeof(m_android_package_name), m_android_package_name, sizeof(m_android_package_name));
    errCode = nfc_ndef_record_encode( &NFC_NDEF_RECORD_BIN_DATA(nfc_record_app_uri),
                                     NDEF_MIDDLE_RECORD,
                                     NULL,
                                     &_record_length);
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_RECORD_BIN_DATA(nfc_record_app_uri));
    OTK_LOG_RAW_INFO(OTK_LABEL_APP_URI "\r\n%s\r\n\r\n", m_android_package_name);

    /* 2. OTK mint information. */
    char _mintInfo[256];
    char *ptr_mstAddr = KEY_getBtcAddr(true);
    char _serial[11] = {0};
    memcpy(_serial, ptr_mstAddr + strlen(ptr_mstAddr) - 10, 10);
    sprintf(_mintInfo, "%s\r\nSerial No.: %s", OTK_MINT_INFO, _serial);
#ifdef DEBUG    
    sprintf(_mintInfo, "%s (DEBUG ONLY)", OTK_MINT_INFO);
#endif    
    sprintf(_mintInfo, "%s\r\nBattery Level: %s / %d mV", _mintInfo, OTK_battLevel(), OTK_battVoltage());
    sprintf(_mintInfo, "%s\r\nNote: \r\n%s", _mintInfo, KEY_getNote());

    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_record_mint_info, UTF_8, en_code, sizeof(en_code),
            (const uint8_t *)_mintInfo, strlen(_mintInfo));
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_TEXT_RECORD_DESC(nfc_record_mint_info));
    OTK_LOG_RAW_INFO(OTK_LABEL_MINT_INFO "\r\n%s\r\n\r\n", _mintInfo);

    /* 3. OTK State */
    /* 4 bytes int to represent 4 states, 
     * BYTE 1 (Highest) - Lock State: 0 （Unlocked) / 1 (Locked) / 2 (Authorized)
     * BYTE 2 - Execution State: 0 (No Command) / 1 (Success) / 2 (Failed)
     * BYTE 3 - Request Comannd
     * BYTE 4 - Failure Reason
     */
    unsigned int _otkState = 0;
    _otkState |= ((OTK_isAuthorized() ? 2 : OTK_isLocked() ? 1 : 0) << 24);
    _otkState |= (m_nfc_cmd_exec_state << 16);
    _otkState |= (m_nfc_request_command << 8);
    _otkState |= (m_nfc_cmd_failure_reason);
    char _strOtkState[8];
    sprintf(_strOtkState, "%08X", _otkState);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_record_lock_state, UTF_8, en_code, sizeof(en_code),
            (const uint8_t *)_strOtkState, strlen(_strOtkState));
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_TEXT_RECORD_DESC(nfc_record_lock_state));
    OTK_LOG_RAW_INFO(OTK_LABEL_OTK_STATE "\r\n%s\r\n\r\n", _strOtkState);

    /* 4. Public Key, HEX string */
    char *_pubKey = KEY_getHexPublicKey(KEY_DERIVATIVE);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_record_public_key, UTF_8, en_code, sizeof(en_code),
            (const uint8_t *)_pubKey, strlen(_pubKey));
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_TEXT_RECORD_DESC(nfc_record_public_key));
    OTK_LOG_RAW_INFO(OTK_LABEL_PUBLIC_KEY "\r\n%s\r\n\r\n", _pubKey);
    //OTK_LOG_RAW_INFO("Master Private Key: \r\n%s\r\n\r\n", KEY_getWIFPrivateKey(KEY_MASTER));
    //OTK_LOG_RAW_INFO("Derivative Private Key: \r\n%s\r\n\r\n", KEY_getWIFPrivateKey(KEY_DERIVATIVE));

    /* 5. Session Data */
    int _sessDataLen = 0;
    char _sessData[NFC_REQUEST_DATA_BUF_SZ] = {0};

    _sessDataLen = sprintf(_sessData, "<%s>\r\n%lu\r\n", OTK_LABEL_SESSION_ID, m_nfc_session_id);

    _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_BITCOIN_ADDR, KEY_getBtcAddr(KEY_DERIVATIVE));

    if (m_nfc_cmd_exec_state == NFC_CMD_EXEC_SUCCESS &&
        (m_nfc_request_command == NFC_REQUEST_CMD_SHOW_KEY ||
        m_nfc_request_command == NFC_REQUEST_CMD_SIGN ||
        m_nfc_request_command == NFC_REQUEST_CMD_EXPORT_WIF_KEY)) {
        _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%lu\r\n", _sessData, OTK_LABEL_REQUEST_ID, m_nfc_request_id);

        /* Below are protected data which require OTK user's authorization to be accessed. */
        if (NFC_REQUEST_CMD_SIGN == m_nfc_request_command) {
            char *_strHash;
            char delim[] = "\n";

            /* Check request option, 1 - using master key, 0 - using derivative (default, if not presented) */
            bool    _useMaster = false;
            char    *strPos = strstr(m_nfc_request_opt_buf, "key=");
            if (strPos != NULL) {
                char    *ptrTail;
                strPos += strlen("key=");
                _useMaster = (1 == strtoul(strPos, &ptrTail, 10));
            }

            char *_sigPubKey = KEY_getHexPublicKey(_useMaster);
            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_PUBLIC_KEY, _sigPubKey);

            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n", _sessData, OTK_LABEL_REQUEST_SIG);

            _strHash = strtok(m_nfc_request_data_buf, delim);
            if (!nfc_isStrHex(_strHash)) {
                OTK_LOG_ERROR("Request data hash is not valid!! Shutting down OTK to protect attack!");
                OTK_shutdown(OTK_ERROR_NFC_INVALID_SIGN_DATA, false);                        
            }
            while(_strHash != NULL)
            {
                int _hashLen = 0;

                if ((strlen(_sessData) + CRYPTO_SIGNATURE_SZ * 2 + 1) > NFC_REQUEST_DATA_BUF_SZ) {
                    OTK_LOG_ERROR("Too many signatures. Shutting down OTK to protect attack!");
                    OTK_shutdown(OTK_ERROR_NFC_TOO_MANY_SIGNATURES, false);                        
                }

                KEY_eraseSignature();
                utils_hex_to_bin(_strHash, _dblhash, strlen(_strHash), &_hashLen);
                if (_hashLen == 0 || _hashLen > SHA256_DIGEST_LENGTH) {
                    break;
                }
                else if (OTK_RETURN_OK != KEY_sign(_dblhash, SHA256_DIGEST_LENGTH, _useMaster)) {
                    OTK_LOG_ERROR("Sign failed.");
                    OTK_shutdown(OTK_ERROR_NFC_SIGN_FAIL, false);                        
                }

                /* Signing calulation may take some time, wait until it's done. */
                while (!KEY_getSignature()) {
                    __WFE();
                }
                _sessDataLen = sprintf(_sessData, "%s%s", _sessData, KEY_getHexSignature());
                /* Signature is copied, erase it to avoid misuse. */
                /* Clear hash buffer for reuse. */
                memset(_dblhash, 0, sizeof(_dblhash));
                _strHash = strtok(NULL, delim);
                if (_strHash != NULL && nfc_isStrHex(_strHash)) {
                    _sessDataLen = sprintf(_sessData, "%s%s", _sessData, delim);                    
                }
            }
            _sessDataLen = sprintf(_sessData, "%s\r\n", _sessData);

            /* Stop OTK tasks and indicate calculated data available. */
            OTK_pause();
            LED_setCadenceType(LED_CAD_RESULT_READY);
            LED_cadence_start();

            m_nfc_output_protect_data = true;
        }
        else if (NFC_REQUEST_CMD_SHOW_KEY == m_nfc_request_command) {
            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_MASTER_EXT_KEY, KEY_getExtPublicKey(KEY_MASTER));
            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_DERIVATIVE_EXT_KEY, KEY_getExtPublicKey(KEY_DERIVATIVE));
            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_DERIVATIVE_PATH, KEY_getStrDerivativePath());

            /* Stop OTK tasks and indicate protected data available. */
            OTK_pause();
            LED_setCadenceType(LED_CAD_RESULT_READY);
            LED_cadence_start();
            m_nfc_output_protect_data = true;
        }
        else if (NFC_REQUEST_CMD_EXPORT_WIF_KEY == m_nfc_request_command) {
            _sessDataLen = sprintf(_sessData, "%s<%s>\r\n%s\r\n", _sessData, OTK_LABEL_WIF_KEY, KEY_getWIFPrivateKey(KEY_DERIVATIVE));

            /* Stop OTK tasks and indicate protected data available. */
            OTK_pause();
            LED_setCadenceType(LED_CAD_RESULT_READY);
            LED_cadence_start();
            m_nfc_output_protect_data = true;            
        }
    }

    /* Append session data record. */
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_record_session_data, UTF_8, en_code, sizeof(en_code),
            (const uint8_t *)_sessData, _sessDataLen);
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_TEXT_RECORD_DESC(nfc_record_session_data));
    OTK_LOG_RAW_INFO(OTK_LABEL_SESSION_DATA "\r\n%s\r\n", _sessData);

    /* 6. Session signature */
    /* Clear hash buffer for reuse. */
    memset(_dblhash, 0, sizeof(_dblhash));
    sha256_Raw((uint8_t*)_sessData, _sessDataLen, _dblhash);
    sha256_Raw(_dblhash, SHA256_DIGEST_LENGTH, _dblhash);

    KEY_eraseSignature();

    if (OTK_RETURN_OK != KEY_sign(_dblhash, SHA256_DIGEST_LENGTH, false)) {
        OTK_LOG_ERROR("Sign failed.");
        OTK_shutdown(OTK_ERROR_NFC_SIGN_FAIL, false);        
    }

    while (!KEY_getSignature()) {
        __WFE();
    }

    _sigHex_ptr = KEY_getHexSignature();
    NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_record_session_sig, UTF_8, en_code, sizeof(en_code),
            (const uint8_t *)_sigHex_ptr, strlen(_sigHex_ptr));
    /* Signature copied, erase it to avoid misuse. */
    errCode = nfc_ndef_msg_record_add(_ndef_msg_desc_ptr, &NFC_NDEF_TEXT_RECORD_DESC(nfc_record_session_sig));
    OTK_LOG_RAW_INFO(OTK_LABEL_SESSION_SIG "\r\n%s", _sigHex_ptr);

    /* End of NFC record creation */
    nrf_delay_ms(10);   /* Delay for long data print completion. */
    OTK_LOG_RAW_INFO("\r\n==  NFC Records End  ==\r\n");

    errCode = nfc_ndef_msg_encode(_ndef_msg_desc_ptr, m_ndef_msg_buf, &_ndef_msg_len);

    APP_ERROR_CHECK(errCode);
    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }
    /* Run Read-Write mode for Type 4 Tag platform */
    errCode = nfc_t4t_ndef_rwpayload_set(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));

    APP_ERROR_CHECK(errCode);
    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    if (NFC_REQUEST_CMD_UNLOCK == m_nfc_request_command &&
        NFC_CMD_EXEC_SUCCESS == m_nfc_cmd_exec_state) {
        deferredFunc = _schedShutdown_aft_unlock;
        if (app_sched_queue_space_get() > 0) {
            APP_ERROR_CHECK(app_sched_event_put(NULL, 0, nfc_execDeferredFunc));
        }
        else {
            OTK_LOG_ERROR("Schedule NFC command failed, scheduler full!!");
            OTK_shutdown(OTK_ERROR_SCHED_ERROR,false);
        }
    }            

    nfc_clearRequest();

    return (OTK_RETURN_OK);
}

OTK_Return NFC_init(void)
{
    ret_code_t errCode;
                      
    m_nfc_session_id = CRYPTO_rng32();

    OTK_LOG_DEBUG("NFC Session ID: <%lu>", m_nfc_session_id);

    errCode = nfc_t4t_setup(nfc_callback, NULL);
    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}

OTK_Return NFC_start(void)
{
    ret_code_t errCode;

    /* Duplicated calling protection. */
    if (!m_nfc_started) {
        if (nfc_setRecords() != OTK_RETURN_OK) {
            return (OTK_RETURN_FAIL);
        }

        /* Start sensing NFC field */
        errCode = nfc_t4t_emulation_start();
        APP_ERROR_CHECK(errCode);
        if (errCode != NRF_SUCCESS) {
            OTK_LOG_ERROR("nfc_t4t_emulation_start FAILED!");
            return (OTK_RETURN_FAIL);
        }
        OTK_LOG_DEBUG("NFC started.");
        m_nfc_started = true;
    }

    return (OTK_RETURN_OK);
}

OTK_Return NFC_stop(bool restart)
{
    ret_code_t errCode;

    m_nfc_restart_flag = restart;
    errCode = app_sched_event_put(NULL, 0, nfc_safelyStop);

    /* Duplicated calling protection. */
    if (NRF_SUCCESS != errCode) {
        OTK_LOG_DEBUG("Cannot schedule NFC_stop task!");
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}

OTK_Return NFC_forceStop()
{
    if (m_nfc_started) {
        if (NRF_SUCCESS != nfc_t4t_emulation_stop()) {
            OTK_LOG_ERROR("Stop NFC failed!");
            return (OTK_RETURN_FAIL);
        }
        else {
            OTK_LOG_DEBUG("NFC stopped.");
            m_nfc_started = false;
        }      
    }

    return (OTK_RETURN_OK);
}
