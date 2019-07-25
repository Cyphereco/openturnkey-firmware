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

#ifndef _LED_H_
#define _LED_H_

#include "otk_const.h"

/* 20181230 by QL, change LED update interval param for power saving */
#define LED_UPDATE_INTERVAL_MS   (200)
#define LED_UPDATE_FRAME_NUM     (4000 / LED_UPDATE_INTERVAL_MS)
#define LED_UPDATE_INTERVAL_TICK (APP_TIMER_TICKS(LED_UPDATE_INTERVAL_MS))

/* LED state enumeration. */
typedef enum {
	LED_CAD_IDLE_UNLOCKED,
	LED_CAD_IDLE_LOCKED,
	LED_CAD_NFC_POLLING,
	LED_CAD_FPS_CAPTURING,
	LED_CAD_PRE_AUTHORIZED,
	LED_CAD_RESULT_READY,
	LED_CAD_ERROR,
	LED_CAD_LAST
} LED_CadenceType;

typedef struct {
    uint8_t cad[LED_UPDATE_FRAME_NUM];
} LED_Cadence;

OTK_Return LED_init(void);

void LED_setCadenceType(LED_CadenceType cadType);

void LED_cadence_start(void);

void LED_cadence_stop(void);

void LED_on(int pin);

void LED_off(int pin);

void LED_all_off();

void LED_blink(int color, int blinkTimes);
#endif
