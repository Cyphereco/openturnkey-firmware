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

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "app_timer.h"
#include "otk.h"
#include "led.h"

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif 

#define NRF_LOG_MODULE_NAME otk_led
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

APP_TIMER_DEF(_ledUpdateTimerId);

#define B OTK_LED_BLUE_MASK
#define G OTK_LED_GREEN_MASK
#define R OTK_LED_RED_MASK
#define W (B+G+R)
#define C (B+G)
#define M (B+R)
#define Y (G+R)
#define K (0)

static LED_Cadence _stateCad[LED_CAD_LAST] = {
    /* The first two cadences all set to 0 to avoid led mix when cadence changed */
    [LED_CAD_IDLE_UNUSED].cad   =  {K, K, K, B, B, K, K, K, B, B, K, K, K, B, B, K, K, K, B, B}, /* Short blink blue*/
    [LED_CAD_IDLE_STANDBY].cad  =  {K, K, K, K, K, K, K, K, B, B, K, K, K, K, K, K, K, K, B, B}, /* Long blink blue*/
    [LED_CAD_NFC_POLLING].cad   =  {K, K, K, M, M, K, K, K, M, M, K, K, K, M, M, K, K, K, M, M}, /* Fast blinking pink. */
    [LED_CAD_FPS_CAPTURING].cad =  {R, R, G, G, R, R, G, G, R, R, G, G, R, R, G, G, R, R, G, G}, /* Red/green inter-blinking. */
    [LED_CAD_PRE_AUTHORIZED].cad = {G, G, G, K, G, G, G, K, G, G, G, K, G, G, G, K, G, G, G, K}, /* Fast blinking yellow (R+G). */
    [LED_CAD_RESULT_READY].cad  =  {K, K, K, G, G, K, K, K, G, G, K, K, K, G, G, K, K, K, G, G}, /* Fast blinking green. */
    [LED_CAD_ERROR].cad         =  {K, K, R, K, R, K, R, K, R, K, R, K, R, K, R, K, R, K, R, K}, /* Fast blinking red. */
};

static bool _ledUpdateTimerRunning = false;
static uint8_t _cadCounter = 0;
static LED_CadenceType _cadType = LED_CAD_IDLE_UNUSED;

/*
 * ======== _ledUpdate() ========
 * Update all LEDs based on cadence set for each LEDs.
 * This function is run every 200ms.
 */
static void _ledUpdate(
    void *context_ptr)
{
    UNUSED_PARAMETER(context_ptr);

    uint8_t x = _stateCad[_cadType].cad[_cadCounter];

    /* Update for state LED. */
    nrf_gpio_pin_write(OTK_LED_RED, (x & OTK_LED_RED_MASK));
    nrf_gpio_pin_write(OTK_LED_GREEN, (x & OTK_LED_GREEN_MASK));
    nrf_gpio_pin_write(OTK_LED_BLUE, (x & OTK_LED_BLUE_MASK));
 
    /* Increment counter. */
    if (++_cadCounter > LED_UPDATE_FRAME_NUM) {
        _cadCounter = 0;
    }
}

/*
 * ======== LED_init() =========
 * Initialize LED update module.
 */
OTK_Return LED_init(void)
{
    ret_code_t errCode;

    // Create timers.
    errCode = app_timer_create(&_ledUpdateTimerId, APP_TIMER_MODE_REPEATED, _ledUpdate);
    APP_ERROR_CHECK(errCode);
    
    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    /* Turn off all LEDs. */
    LED_all_off();

    return (OTK_RETURN_OK);    
}

void LED_setCadenceType(LED_CadenceType cadType)
{
    _cadCounter = 0;
    _cadType = cadType;
}

/*
 * ======== LED_cadence_start() =========
 * Start LED update module.
 */
void LED_cadence_start(void)
{
    ret_code_t errCode;

    if (!_ledUpdateTimerRunning) {
         // Start application timers.
        errCode = app_timer_start(_ledUpdateTimerId, LED_UPDATE_INTERVAL_TICK, NULL);
        APP_ERROR_CHECK(errCode);
        _ledUpdateTimerRunning = true;
    }
}

/*
 * ======== LED_cadence_stop() =========
 * Stop LED update module.
 */
void LED_cadence_stop(void)
{
    ret_code_t errCode;

    if (_ledUpdateTimerRunning) {
        // Stop application timers.
        errCode = app_timer_stop(_ledUpdateTimerId);
        APP_ERROR_CHECK(errCode);
        _ledUpdateTimerRunning = false;
    }
}

/*
 * ======== LED_on() ========
 * Turn on a LED.
 */
void LED_on(int pin) {
    nrf_gpio_pin_write(pin, OTK_LED_ON_STATE);
}

/*
 * ======== LED_off() ========
 * Turn off a LED.
 */
void LED_off(int pin) {
    nrf_gpio_pin_write(pin, OTK_LED_OFF_STATE);
}

/*
 * ======== LED_off() ========
 * Turn off a LED.
 */
void LED_all_off() {
    LED_cadence_stop();
    LED_off(OTK_LED_RED);
    LED_off(OTK_LED_GREEN);
    LED_off(OTK_LED_BLUE);
}

/*
 * ======== LED_blink() ========
 * blink specific color of led twice 0.25s on / 0.25s off.
 * !!! This is a block function !!!
 */
void LED_blink(int color, int blinkTimes) {
    int i;

    LED_all_off();
    nrf_delay_ms(1000);

    for (i = 0; i < blinkTimes; i++) {
        LED_off(color);
        nrf_delay_ms(250);
        LED_on(color);
        nrf_delay_ms(250);
    }

    LED_off(color);
}