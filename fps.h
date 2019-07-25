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

#ifndef _FPS_H_
#define _FPS_H_

#define FPS_TOUCH_DEBOUNCE    (3)     /* Consecutive untouched times */

/* Command enum. */
typedef enum {
    FPS_CMD_TYPE_CAPTURE = 0,
    FPS_CMD_TYPE_ENROLL,
    FPS_CMD_TYPE_ERASE_ONE,
    FPS_CMD_TYPE_ERASE_ALL,
    FPS_CMD_TYPE_MATCH_1_N,
    FPS_CMD_TYPE_GET_USER_NUM,
    FPS_CMD_TYPE_SET_SECURITY_LEVEL,
    FPS_CMD_TYPE_RESET_SENSOR,
    FPS_CMD_TYPE_LAST,
} FPS_CmdType;

typedef struct {
    uint8_t data[7];
} XTB0811_CmdData;

typedef struct {
    union {
        XTB0811_CmdData xtb0811;
    } u;
} FPS_CmdData;

typedef struct {
    uint8_t data[8];
} XTB0811_RespData;

typedef struct {
    union {
        XTB0811_RespData xtb0811;
    } u;
} FPS_RespData;

OTK_Return  FPS_init(void);

void        FPS_powerOn();

void        FPS_powerOff();

OTK_Return  FPS_longTouchDetectorStart(void);

void        FPS_longTouchDetectorStop(void);

OTK_Return  FPS_setSecurityLevel(
    uint8_t level);

OTK_Return  FPS_resetSensor(void);

OTK_Return  FPS_captureAndEnroll(void);

uint8_t     FPS_captureAndMatch(
    uint8_t min_matches);

OTK_Return  FPS_eraseOne(
    uint8_t idx);

OTK_Return  FPS_eraseAll(void);

uint8_t     FPS_getUserNum(void);

bool        FPS_isTouched(void);

#endif
