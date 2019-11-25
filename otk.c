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
#include <stdlib.h>
#include <math.h>
#include "app_scheduler.h"
#include "app_timer.h"
#include "boards.h"
#include "bsp.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_saadc.h"
#include "nrf_log.h"
#include "sdk_common.h"

#include "otk.h"
#include "fps.h"
#include "key.h"
#include "led.h"
#include "nfc.h"
#include "uart.h"
#include "pwrmgmt.h"

//#define DISABLE_FPS

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS  440  //!< Reference voltage (in milli volts) used by ADC while doing conversion.
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS 520  //!< Typical forward voltage drop of the diode (Part no: SD103ATW-7-F) that is connected in series with the voltage supply. This is the voltage drop when the forward current is 1mA. Source: Data sheet of 'SURFACE MOUNT SCHOTTKY BARRIER DIODE ARRAY' available at www.diodes.com.
#define ADC_RES_10BIT                  1024 //!< Maximum digital value for 10-bit ADC conversion.
#define ADC_PRE_SCALING_COMPENSATION   10   //!< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) \
    ((((ADC_VALUE) *ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION)

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif  /* NRF_LOG_MODULE_NAME */

#define NRF_LOG_MODULE_NAME otk_otk
NRF_LOG_MODULE_REGISTER();

static nrf_saadc_value_t adc_buf;          //!< Buffer used for storing ADC value.
static uint16_t          m_batt_lvl_in_milli_volts; //!< Current battery level.

static bool m_otk_isLocked = false;
static bool m_otk_isAuthorized = false;

void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        nrf_saadc_value_t adc_result;

        adc_result = p_event->data.done.p_buffer[0];

        m_batt_lvl_in_milli_volts =
            ADC_RESULT_IN_MILLI_VOLTS(adc_result) + DIODE_FWD_VOLT_DROP_MILLIVOLTS;
    }
}

void saadc_init(void)
{
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN2);

    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf, 1);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
}

/*
 * ======== OTK_init() ========
 * Initialize required modules.
 */
OTK_Error OTK_init(void)
{
    ret_code_t errCode;
    OTK_Error _init_error = OTK_ERROR_NO_ERROR;

    /* Enable NRF52 DCDC converter to save some power consumption. */
    NRF_POWER->DCDCEN = 1;

    /* Configure output GPIO pins */
    nrf_gpio_pin_dir_set(OTK_LED_BLUE, NRF_GPIO_PIN_DIR_OUTPUT);        /* LED blue control pin. */
    nrf_gpio_pin_dir_set(OTK_LED_GREEN, NRF_GPIO_PIN_DIR_OUTPUT);       /* LED green control pin. */
    nrf_gpio_pin_dir_set(OTK_LED_RED, NRF_GPIO_PIN_DIR_OUTPUT);         /* LED red control pin. */
    nrf_gpio_pin_dir_set(OTK_PIN_FPS_PWR_EN, NRF_GPIO_PIN_DIR_OUTPUT);  /* FPS power supply control pin. */
    nrf_gpio_pin_write(OTK_LED_BLUE, OTK_LED_OFF_STATE);                /* Set LED default to off. */
    nrf_gpio_pin_write(OTK_LED_GREEN, OTK_LED_OFF_STATE);               /* Set LED default to off. */
    nrf_gpio_pin_write(OTK_PIN_FPS_PWR_EN, NRF_GPIO_PIN_PULLDOWN);      /* Set FPS power switch default to off. */

    /* Configure input sensor pins */
    nrf_gpio_cfg_sense_input(OTK_PIN_FPS_TOUCHED, OTK_WAKE_UP_PIN_PULL, OTK_WAKE_UP_PIN_SENSE);
    nrf_gpio_cfg_input(OTK_PIN_FPS_TOUCHED, NRF_GPIO_PIN_PULLDOWN);

#if (LED_TEST)
    return (OTK_RETURN_OK);
#endif

#ifndef DISABLE_FPS
    /* Turn on FPS MCU power. */
    FPS_powerOn();
#endif
    /* Initialize SAADC for battery voltage detection. */
    saadc_init();

    OTK_LOG_DEBUG("SAADC sampling...");
    if (!nrf_drv_saadc_is_busy())
    {
        errCode = nrf_drv_saadc_buffer_convert(&adc_buf, 1);
        APP_ERROR_CHECK(errCode);

        errCode = nrf_drv_saadc_sample();
        APP_ERROR_CHECK(errCode);
    }
    OTK_LOG_DEBUG("Battery Voltage: %d", m_batt_lvl_in_milli_volts);

    /* Unitialize SAADC after voltage deteced. */
    nrf_drv_saadc_uninit();

#if 0
    if (m_batt_lvl_in_milli_volts < 3500) {
        LED_on(OTK_LED_RED);
        LED_on(OTK_LED_GREEN);
        nrf_delay_ms(2000);
        LED_all_off();
        nrf_delay_ms(500);
    }
#endif
    /* Initialize App Scheduler. */
    APP_SCHED_INIT(APP_SCHED_MAX_EVENT_SIZE, APP_SCHED_QUEUE_SIZE);

    /* Initialize timer module. */
    errCode = app_timer_init();
    APP_ERROR_CHECK(errCode);
    if (NRF_SUCCESS != errCode) {
        OTK_LOG_ERROR("app_timer_init failed!!");  
        _init_error |= OTK_ERROR_INIT_TIMER;     
    }

    /* Init UART. */
    if (UART_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("UART_init failed!!");       
        _init_error |= OTK_ERROR_INIT_UART;     
    }

     /* Init LED update module. */
    if (LED_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("LED_init failed!!");       
        _init_error |= OTK_ERROR_INIT_LED;     
    }

    /* Init power management. */
    if (PWRMGMT_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("PWRMGMT_init failed!!");       
        _init_error |= OTK_ERROR_INIT_PWRMGMT;     
    }

    /* Init CRYPTO. */  
    if (CRYPTO_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("CRYPTO_init failed!!");       
        _init_error |= OTK_ERROR_INIT_CRYPTO;     
    }

    /* Init KEY. */  
    if (KEY_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("KEY_init failed!!");       
        _init_error |= OTK_ERROR_INIT_KEY;     
    }

    /* Init NFC */
    if (NFC_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("NFC_init failed!!");       
        _init_error |= OTK_ERROR_INIT_NFC;     
    }
#ifndef DISABLE_FPS
    /* Init FPS */
     if (FPS_init() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("FPS_init failed!!");       
        _init_error |= OTK_ERROR_INIT_FPS;     
    }
    m_otk_isLocked = (FPS_getUserNum() > 0) ? true : false;
    m_otk_isAuthorized = false;
#endif

    return (_init_error);
}

/*
 * ======== OTK_isLocked() ========
 * Return OTK lock state.
 */
bool OTK_isLocked() {
    return (m_otk_isLocked);
}

bool OTK_isAuthorized(void) {
    return (m_otk_isAuthorized);
}

void OTK_standby()
{
    if (OTK_isAuthorized()) {
        LED_setCadenceType(LED_CAD_PRE_AUTHORIZED);
    }
    else {
        LED_setCadenceType(LED_CAD_IDLE_STANDBY);        
    }

    /* Start LED cadence */
    LED_cadence_start();

    /* Start sensing NFC field */
    NFC_start();
#ifndef DISABLE_FPS
    if (FPS_longTouchDetectorStart() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("FPS_longTouchDetectorStart failed!!");   
        OTK_shutdown(OTK_ERROR_INIT_FPS, false);
    }        
#endif
    OTK_extend();
}

void OTK_pause()
{
#ifndef DISABLE_FPS    
    FPS_longTouchDetectorStop();  
#endif      
    LED_all_off();
}

void OTK_extend()
{
    PWRMGMT_feed();    
}

void OTK_clearAuth() {
    m_otk_isAuthorized = false;
}

void OTK_cease(OTK_Error err) {
    OTK_pause();
    NFC_forceStop();
#ifndef DISABLE_FPS    
    FPS_powerOff();
#endif
    /* Uninitializations. */
    UART_uninit();

    nrf_delay_ms(500);
    if (err > OTK_ERROR_NO_ERROR) {
        LED_blink(OTK_LED_RED, err);
    }
    else {
        LED_blink(OTK_LED_BLUE, 2);
    }
    nrf_delay_ms(500);

    APP_ERROR_CHECK(app_timer_stop_all());

    /* Reset all GPIO pin positions. */
    nrf_gpio_cfg_default(OTK_LED_BLUE);
    nrf_gpio_cfg_default(OTK_LED_GREEN);
    nrf_gpio_cfg_default(OTK_LED_RED);
    nrf_gpio_cfg_default(OTK_PIN_FPS_TOUCHED);
    nrf_gpio_cfg_default(OTK_PIN_FPS_PWR_EN);
    nrf_gpio_cfg_default(OTK_PIN_FPS_UART_RX);
    nrf_gpio_cfg_default(OTK_PIN_FPS_UART_TX);

    OTK_LOG_RAW_INFO("\r\n\r\n>>> GOOD BYE! OTK SHUTTING DOWN! <<<\r\n\r\n");
}

void OTK_shutdown(
    OTK_Error err,
    bool reboot)
{
    if (reboot) {
        PWRMGMT_reboot();
    }
    else {
        PWRMGMT_shutdown(err);
    }
}


void OTK_lock()
{
    OTK_Return ret;
    OTK_LOG_DEBUG("Executing OTK_lock");

    if (OTK_isLocked()) {
        OTK_LOG_DEBUG("OTK is locked already!");
        return;
    }
    LED_setCadenceType(LED_CAD_FPS_CAPTURING);
    LED_cadence_start();
    ret = FPS_captureAndEnroll();
    LED_all_off();
    nrf_delay_ms(1000);

    if (OTK_RETURN_OK == ret) {
        LED_on(OTK_LED_GREEN);
    }
    else {
        LED_on(OTK_LED_RED);
    }
    nrf_delay_ms(1000);
    FPS_resetSensor();

    OTK_shutdown(OTK_ERROR_NO_ERROR, false);       
}


void OTK_unlock() 
{
    OTK_LOG_DEBUG("Executing OTK_unlock");
    char *empty_str = "";

    if (!OTK_isLocked()) {
        OTK_LOG_DEBUG("OTK is unlocked already!");
        return;
    }

    if (OTK_isAuthorized()) {
        OTK_pause();
#ifndef DISABLE_FPS        
        OTK_Return ret = FPS_eraseAll();
        if (OTK_RETURN_OK == ret) {
            ret = KEY_setPin(KEY_DEFAULT_PIN);
            ret &= KEY_setNote(empty_str);
        }

        m_otk_isLocked = (FPS_getUserNum() > 0) ? true : false;        
#endif        
    }
    OTK_clearAuth();
    OTK_standby();
}

void OTK_reset() 
{
    OTK_LOG_DEBUG("Executing OTK_reset command");
    OTK_LOG_DEBUG("User must press the FP sensor and hold 8 seconds to confirm the reset.");

    NFC_stop(false);
    LED_setCadenceType(LED_CAD_FPS_CAPTURING);
    LED_cadence_start();
    FPS_confirmReset();
}

void OTK_resetConfirmed() 
{
    OTK_LOG_DEBUG("OTK_reset command is confirmed.");
    OTK_LOG_DEBUG("Fingerprints, Notes, PIN will be cleared and a new random derivative key will be chosen.");

    m_otk_isAuthorized = true;
    OTK_unlock();

    int idx;
    CRYPTO_derivativePath  newPath;

    /* choose new random derivative key. */
    for (idx = 0; idx < CRYPTO_DERIVATIVE_DEPTH; idx++) {
        newPath.derivativeIndex[idx] = (CRYPTO_rng32() % 0x80000000);
    }

    KEY_setNewDerivativePath(&newPath);
    KEY_recalcDerivative();

    nrf_delay_ms(1000);
    FPS_resetSensor();
    OTK_shutdown(OTK_ERROR_NO_ERROR, false);       
}

void OTK_fpValidate() 
{
    OTK_LOG_DEBUG("Executing OTK_fpValidate");
    if (!OTK_isAuthorized()) {
        if (!OTK_isLocked()) {
            OTK_LOG_ERROR("OTK_fpValidate requires OTK_lock first!");
            return;
        }
        LED_setCadenceType(LED_CAD_FPS_CAPTURING);
        LED_cadence_start();
#ifndef DISABLE_FPS        
        if (FPS_captureAndMatch(1) > 0) {
            m_otk_isAuthorized = true;
            NFC_stop(true);
        }
        else {
            /* Match FP failed, shutdown to protect OTK. */
            OTK_shutdown(OTK_ERROR_AUTH_FAILED, false);
        }
#endif        
    }
    OTK_standby();
}

OTK_Error OTK_setKey(char *strIn)
{
    OTK_LOG_DEBUG("Executing OTK_setKey");

    int idx = 0;
    char *ptr;
    char *str;
    char delim[] = ",";
    CRYPTO_derivativePath  newPath;

    if (OTK_isAuthorized()) {
        if (strlen(strIn) > 0) {
            idx = 0;
            str = strtok(strIn, delim);
            while(str != NULL)
            {
                uint32_t x = pow(10, strlen(str) - 1);
                uint32_t ret = strtoul(str, &ptr, 10);
                uint32_t chk = strtoul(str+1, NULL, 10);
                if (strlen(ptr) == 0 && 
                    (ret % x) == chk &&
                    ret >= 0 && ret < (0x80000000)) {
                    newPath.derivativeIndex[idx] = ret;
                } 
                else {
                    OTK_clearAuth();
                    return OTK_ERROR_INVALID_KEYPATH;
                }
                str = strtok(NULL, delim);
                idx++;
            }
            if (idx < 5) {
                OTK_clearAuth();
                return OTK_ERROR_INVALID_KEYPATH;                
            }
        }

        KEY_setNewDerivativePath(&newPath);
        KEY_recalcDerivative();

        OTK_clearAuth();

        return OTK_ERROR_NO_ERROR;
    }

    OTK_clearAuth();

    return OTK_ERROR_AUTH_FAILED;
}

OTK_Error OTK_setPin(char *strIn)
{
    OTK_LOG_DEBUG("Executing OTK_setPin");
    uint32_t ret;
    uint32_t chk;
    char     *ptr;

    if (OTK_isAuthorized()) {
        if (strlen(strIn) > 0 && strlen(strIn) <= 10) {
            uint32_t x = pow(10, strlen(strIn) - 1);
            ret = strtoul(strIn, &ptr, 10);
            chk = strtoul(strIn + 1, NULL, 10);
            OTK_LOG_DEBUG("SET_PIN: %i / %i", (ret % x), chk);
            if (strlen(ptr) == 0 && (ret % x) == chk) {
                KEY_setPin(ret);

                OTK_clearAuth();
                return OTK_ERROR_NO_ERROR;
            }
        }
        OTK_clearAuth();
        return OTK_ERROR_INVALID_PIN;
    }

    OTK_clearAuth();
    return OTK_ERROR_AUTH_FAILED;
}

OTK_Error OTK_pinValidate(char *strIn)
{
    OTK_LOG_DEBUG("Executing OTK_pinValidate");
    uint32_t ret = 0;
    char     *ptrTail;
    char     *strPos = strstr(strIn, "pin=");

    OTK_pause();

    if (KEY_DEFAULT_PIN == KEY_getPin()) {
        return OTK_ERROR_PIN_UNSET;
    }

    if (strPos !=NULL) {
        strPos += strlen("pin=");

        if (strlen(strPos) > 0) {
            ret = strtoul(strPos, &ptrTail, 10);
            if (KEY_getPin() == ret) {
                m_otk_isAuthorized = true;
                OTK_LOG_DEBUG("PIN Valid!");
                return OTK_ERROR_NO_ERROR;
            }
        }        
    }

    OTK_LOG_ERROR("PIN Invalid: %i", ret);   
    OTK_clearAuth();

    return OTK_ERROR_PIN_NOT_MATCH;
}

OTK_Error OTK_setNote(char *str)
{
    OTK_Return ret;
    OTK_LOG_DEBUG("Executing OTK_setNote");

    OTK_pause();
    ret = KEY_setNote(str);

    if (ret != OTK_RETURN_OK) {
        OTK_clearAuth();
        return OTK_ERROR_NOTE_TOO_LONG;
    }

    OTK_clearAuth();
    return OTK_ERROR_NO_ERROR;
}

char* OTK_battLevel()
{
    return (m_batt_lvl_in_milli_volts > 4000 ? "100%" :
        m_batt_lvl_in_milli_volts > 3920 ? "90%" :
        m_batt_lvl_in_milli_volts > 3860 ? "80%" :
        m_batt_lvl_in_milli_volts > 3800 ? "70%" :
        m_batt_lvl_in_milli_volts > 3725 ? "60%" :
        m_batt_lvl_in_milli_volts > 3650 ? "50%" :
        m_batt_lvl_in_milli_volts > 3635 ? "40%" :
        m_batt_lvl_in_milli_volts > 3620 ? "30%" :
        m_batt_lvl_in_milli_volts > 3550 ? "20%" : "10%");
}

uint16_t OTK_battVoltage()
{
    return (m_batt_lvl_in_milli_volts);
}