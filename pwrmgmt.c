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

#include "app_timer.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "otk.h"

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif  /* NRF_LOG_MODULE_NAME */

#define NRF_LOG_MODULE_NAME otk_pwrmgmt
NRF_LOG_MODULE_REGISTER();

static bool _shutdownHandler(nrf_pwr_mgmt_evt_t event);

/**@brief Register application shutdown handler with priority 0. */
NRF_PWR_MGMT_HANDLER_REGISTER(_shutdownHandler, 0);

OTK_Error m_exit_error = OTK_ERROR_NO_ERROR;

/*
 * ======== PWRMGMT_init() ========
 * Init power management module.
 */
OTK_Return PWRMGMT_init(void)
{
    ret_code_t errCode;
    /* Init low frequency clock. */
    errCode = nrf_drv_clock_init();
    APP_ERROR_CHECK(errCode);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    nrf_drv_clock_lfclk_request(NULL);

    /* Init timer. */
    errCode = app_timer_init();
    APP_ERROR_CHECK(errCode);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    /* Init power management. */
    uint32_t ret_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(ret_code);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK); 
}

/*
 * ======== PWRMGMT_run() ========
 * Run PWR MGMT
 */
void PWRMGMT_run(void)
{
    nrf_pwr_mgmt_run();
}

/*
 * ======== PWRMGMT_feed() ========
 * Feed PWR MGMT for indicating the device is still run.
 */
void PWRMGMT_feed(void)
{
    nrf_pwr_mgmt_feed();
}


/*
 * ======== PWRMGMT_feed() ========
 * Request OTK reboot.
 */
void PWRMGMT_reboot(void)
{
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
}

/*
 * ======== PWRMGMT_shutdown() ========
 * Request OTK shutdown imediately.
 */
void PWRMGMT_shutdown(OTK_Error err)
{
    m_exit_error = err;
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_EVT_PREPARE_WAKEUP);
}

/*
 * ======== _shutdownHandler() ========
 */
bool _shutdownHandler(
    nrf_pwr_mgmt_evt_t event)
{
    switch (event)
    {
        case NRF_PWR_MGMT_EVT_PREPARE_SYSOFF:
            NRF_LOG_INFO("NRF_PWR_MGMT_EVT_PREPARE_SYSOFF");
            break;
        case NRF_PWR_MGMT_EVT_PREPARE_WAKEUP:
            NRF_LOG_INFO("NRF_PWR_MGMT_EVT_PREPARE_WAKEUP");
            break;
        case NRF_PWR_MGMT_EVT_PREPARE_RESET:
            NRF_LOG_INFO("NRF_PWR_MGMT_EVT_PREPARE_RESET");
            break;
        default:
            OTK_LOG_ERROR("Unhandled event:%d", event);
            return (false);
    }

    OTK_cease(m_exit_error);
    /* configure wakeup pin before shutting down. */
    nrf_gpio_cfg_sense_input(OTK_PIN_FPS_TOUCHED, OTK_WAKE_UP_PIN_PULL, OTK_WAKE_UP_PIN_SENSE);

    return (true);
}