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

#include "nrf_log.h"
#include "file.h"
#include "fds.h"

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif

#define NRF_LOG_MODULE_NAME otk_file
NRF_LOG_MODULE_REGISTER();

#define FILE_ID 0x1111 /* File ID. */
#define REC_KEY 0x2222 /* Record KEY. */

static volatile bool _isFdsReady      = false; /* Flag used to indicate that FDS initialization is finished. */
static volatile bool _GcCompleted     = false;  /* Flag used to preserve write request during Garbage Collector activity. */

static fds_record_desc_t  _recordDesc; /* Record descriptor. */

/*
 * ======== _eventHandler() ========
 * This function is used to handle various FDS events like end of initialization,
 * write, update and Garbage Collection activity. It is used to track FDS actions
 * and perform pending writes after the Garbage Collecion activity.
 *
 */
static void _eventHandler(fds_evt_t const * const p_fds_evt)
{
    OTK_LOG_DEBUG("FDS event %u with result %u.", p_fds_evt->id, p_fds_evt->result);

    switch (p_fds_evt->id)
    {
        case FDS_EVT_INIT:
            APP_ERROR_CHECK(p_fds_evt->result);
            _isFdsReady = true;
            break;

        case FDS_EVT_UPDATE:
            APP_ERROR_CHECK(p_fds_evt->result);
            NRF_LOG_INFO("FDS update success.");
            break;

        case FDS_EVT_WRITE:
            APP_ERROR_CHECK(p_fds_evt->result);
            NRF_LOG_INFO("FDS write success.");
            break;

        case FDS_EVT_GC:
            APP_ERROR_CHECK(p_fds_evt->result);
            NRF_LOG_INFO("Garbage Collector activity finished.");
            _GcCompleted = true;
            break;

        default:
            break;
    }
}


/*
 * ======== FILE_init() ========
 * Setup file module.
 *
 * Parameters:
 *   None.
 * 
 * Returns:
 *   OTK_RETURN_OK - Setup done.
 *   OTK_RETURN_FAIL - Setup failed.
 */
OTK_Return FILE_init(void)
{
    ret_code_t errCode;

    // Register FDS event handler to the FDS module.
    errCode = fds_register(_eventHandler);
    APP_ERROR_CHECK(errCode);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("FDS_REGISTER failed!");
        return (OTK_RETURN_FAIL);
    }

    // Initialize FDS.
    errCode = fds_init();
    APP_ERROR_CHECK(errCode);
    if (errCode != NRF_SUCCESS) {
        OTK_LOG_ERROR("FDS_INIT failed!");
        return (OTK_RETURN_FAIL);
    }

    // Wait until FDS is initialized.
    while (!_isFdsReady) {
        __WFE();
    }

    return (OTK_RETURN_OK);
}

/*
 * ======== FILE_write() ========
 * Write files. It could be file creation or file update depends on isCreate.
 *
 * Parameters:
 *   isCreate - To indicate if it's to create new files.
 *   file_ptr - Point to data buffer for creating or updating the files.
 * 
 * Returns:
 *   OTK_RETURN_OK - Write done.
 *   OTK_RETURN_FAIL - Write failed.
 */
OTK_Return FILE_write(
    bool isCreate, const uint8_t *buff_ptr, int size)
{
    if (!_isFdsReady) {
        return (OTK_RETURN_FAIL);
    }

    ret_code_t errCode;
    fds_record_t _record = {
        .file_id           = FILE_ID,
        .key               = REC_KEY,
        .data.p_data       = buff_ptr,
        .data.length_words = BYTES_TO_WORDS(size) // Align data length to 4 bytes.
    }; 

    // Create FLASH file with NDEF message.
    if (isCreate) {
        errCode = fds_record_write(&_recordDesc, &_record);
    }
    else {
        errCode = fds_record_update(&_recordDesc, &_record);
    }

    if (errCode == NRF_SUCCESS) {
        return (OTK_RETURN_OK);
    }
    else {
        if (errCode == FDS_ERR_NO_SPACE_IN_FLASH)
        {
            NRF_LOG_INFO("FDS has no free space left, Garbage Collector triggered!");
            // If there is no space, preserve write request and call Garbage Collector.
            errCode = fds_gc();

            while (!_GcCompleted) {
                __WFE();
            }

            _GcCompleted = false;
            
            return (FILE_write(isCreate, buff_ptr, size));
        }
        return (OTK_RETURN_FAIL);
    }
}

/*
 * ======== FILE_load() ========
 * Load file from storage.
 *
 * Parameters:
 *   None.
 * 
 * Returns:
 *   OTK_RETURN_OK - Setup done.
 *   OTK_RETURN_FAIL - Setup failed.
 */
OTK_Return FILE_load(
    uint8_t *buff_ptr, int *buff_len)
{
    if (!_isFdsReady) {
        return (OTK_RETURN_FAIL);
    }

    ret_code_t         errCode;
    fds_find_token_t   ftok;
    fds_flash_record_t flash_record;

    // Always clear token before running new file/record search.
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    // Search for NDEF message in FLASH.
    errCode = fds_record_find(FILE_ID, REC_KEY, &_recordDesc, &ftok);

    // If there is no record with given key and file ID,
    // create default message and store in FLASH.
    if (errCode == FDS_SUCCESS)
    {
        NRF_LOG_INFO("Found file record.");

        // Open record for read.
        errCode = fds_record_open(&_recordDesc, &flash_record);
        APP_ERROR_CHECK(errCode);
        if (errCode != NRF_SUCCESS) {
            return (OTK_RETURN_FAIL);
        }

        *buff_len = flash_record.p_header->length_words * sizeof(uint32_t);

        // Access the record through the flash_record structure.
        memcpy(buff_ptr, flash_record.p_data, *buff_len);

        // Print file length and raw message data.
        OTK_LOG_DEBUG("Load file data length: %d bytes.",*buff_len);

        //NRF_LOG_HEXDUMP_DEBUG(buff_ptr, flash_record.p_header->length_words * sizeof(uint32_t));

        // Close the record when done.
        errCode = fds_record_close(&_recordDesc);
    }
    else if (errCode == FDS_ERR_NOT_FOUND)
    {
        OTK_LOG_ERROR("Load file failed!! (FDS_ERR_NOT_FOUND)");
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}
