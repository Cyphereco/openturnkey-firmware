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

#include "app_error.h"
#include "app_scheduler.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "otk.h"
#include "led.h"
#include "nfc.h"
#include "key.h"
#include "pwrmgmt.h"
#include "unittest.h"

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif 

#define NRF_LOG_MODULE_NAME otk_main
NRF_LOG_MODULE_REGISTER();


/*
 * ======== main() ========
 */
int main(void)
{
    /* Init log. */
    ret_code_t errCode = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(errCode);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("\r\n\r\n\r\n>>> GOOD DAY ! OTK BOOT UP! <<<\r\n");

    /* Init OTK. */
    OTK_Error err = OTK_init();
    if (OTK_ERROR_NO_ERROR == err) {
        NRF_LOG_INFO("OTK Initialized Successfully!");
#ifdef DEBUG
        NRF_LOG_INFO("Running DEBUG version!!");
        NRF_LOG_INFO("PIN=%i", KEY_getPin());
        nrf_delay_ms(1000);
#endif        
    }
    else {
        OTK_LOG_ERROR("OTK Initialized Failed, system will shutdown in 1 second!!");
        nrf_delay_ms(5000);
        OTK_shutdown(err, false);
    }

#if (LED_TEST)
    int m = 0;

    OTK_LOG_ERROR("Running LED test, B > G > B+G > R > B+R > G+R > B+G+R > OFF...");
    while (1) {
        if (m & OTK_LED_BLUE_MASK) {
            LED_on(OTK_LED_BLUE);
        }
        else {
            LED_off(OTK_LED_BLUE);
        }
        if (m & OTK_LED_GREEN_MASK) {
            LED_on(OTK_LED_GREEN);
        }
        else {
            LED_off(OTK_LED_GREEN);
        }
        if (m & OTK_LED_RED_MASK) {
            LED_on(OTK_LED_RED);
        }
        else {
            LED_off(OTK_LED_RED);
        }
        nrf_delay_ms(1000);
        m = (m < OTK_LED_RED_MASK + OTK_LED_GREEN_MASK + OTK_LED_BLUE_MASK) ? m + 1 : 0;
    }
#endif

#if (UNITTEST)
    int x = unittestRun();
    if (0 == x) {
        LED_on(OTK_LED_RED);
        LED_on(OTK_LED_GREEN);
        LED_on(OTK_LED_BLUE);
        OTK_LOG_DEBUG("Unittest passed!");
    }
    else {
        LED_blink(OTK_LED_RED, x);
        OTK_LOG_ERROR("Unittest failed on (%d).", x);
    }
    while (1)
    {
        __WFE();
    }
#endif

    /* Turn on LED for lock state indicatoin */
    int _led = OTK_isLocked() ? OTK_LED_RED : OTK_LED_GREEN;
    int _pwrLvl = OTK_battVoltage();

    if (_pwrLvl < 3580) {
        int i;
        for (i = 0; i < 4; i++) {
            LED_on(_led);
            nrf_delay_ms(200);
            LED_all_off();
            nrf_delay_ms(50);
        }
    }
    else {
        LED_on(_led);
        nrf_delay_ms(1000);
        LED_all_off();
    }

    NFC_start();
    OTK_standby();

    while (1)
    {
        app_sched_execute();
        if (NRF_LOG_PROCESS() == false)
        {
            PWRMGMT_run();
        }
        __WFE();
    }
}

/** @}
 */
