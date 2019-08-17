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

#ifndef _OTK_H_
#define _OTK_H_

#include "otk_const.h"
#include "otk_config.h"
#include "otk_common.h"
#include "crypto.h"

bool OTK_isLocked(void);

bool OTK_isAuthorized(void);

/* OTK_init
 * initiate i/o interfaces, timers, load keys, 
 */
OTK_Error OTK_init(void);

/* OTK_standby
 * enable NFC, start touch detection, set led cadence, set timer
 */
void OTK_standby(void);

/* OTK_pause
 * disable NFC, stop touch detection, turn off led
 */
void OTK_pause(void);

/* OTK_pause
 * disable NFC, stop touch detection, turn off led
 */
void OTK_extend(void);

/* OTK_cease
 * prepare for OTK shutdown
 */
void OTK_cease(
	OTK_Error err);

/* OTK_shutdown
 * indicate shutdown alert led, turn off all interfaces, shutdown or reboot otk.
 */
void OTK_shutdown(
	OTK_Error err,
    bool reboot);

/* OTK_lock
 * enroll fingerprint
 */
void OTK_lock(void);

/* OTK_lock
 * remove fingerprint, reset OTK PIN, erase Key Note
 */
void OTK_unlock(void);

/* OTK_authorization
 * start fingerprint authorization
 */
void OTK_fpValidate(void);

/* OTK_setKey
 * configure the key path with the input index sequence separated by comma (,)
 * i.e. 1,2,3,4,5
 * if a child index is 0, it means to get a random child index
 */
void OTK_setKey(char *strIn);

/* OTK_setPin
 * set the OTK PIN as user's input number
 */
void OTK_setPin(char *strIn);

/* OTK_pinValidate
 * start fps capture, set led cadence, set timer
 */
void OTK_pinValidate(char *strIn);

/* OTK_setNote
 * set Key Note with the customized input string
 */
void OTK_setNote(char *strIn);

/* OTK_battLevel
 * Return OTK battery level
 *
 * @return char* battery level: 10%~100% step 10%
 */
char* OTK_battLevel(void);

/* OTK_battVoltage
 * Return OTK battery voltage
 *
 * @return uint16_t, battery voltage in milli-volt.
 */
uint16_t OTK_battVoltage(void);

#endif

