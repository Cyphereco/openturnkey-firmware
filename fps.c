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

#include "app_scheduler.h"
#include "app_timer.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "led.h"
#include "otk.h"
#include "uart.h"
#include "fps.h"


/* Right now only suppot XTB0811 FPS. */
#define FPS_XTB0811

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif 

#define NRF_LOG_MODULE_NAME otk_fps
NRF_LOG_MODULE_REGISTER();

#if defined(FPS_XTB0811)
static XTB0811_CmdData _cmdData[FPS_CMD_TYPE_LAST] = {
    [FPS_CMD_TYPE_CAPTURE].data             = {0x6C, 0x63, 0x62, 0x51, 0x00, 0x00, 0x00},
    [FPS_CMD_TYPE_ENROLL].data              = {0x6C, 0x63, 0x62, 0x7F, 0x00, 0x00, 0x00},
    [FPS_CMD_TYPE_ERASE_ONE].data           = {0x6C, 0x63, 0x62, 0x73, 0x00, 0x00, 0x00},
    [FPS_CMD_TYPE_ERASE_ALL].data           = {0x6C, 0x63, 0x62, 0x54, 0x00, 0x00, 0x19},
    [FPS_CMD_TYPE_MATCH_1_N].data           = {0x6C, 0x63, 0x62, 0x71, 0x00, 0x00, 0x36},
    [FPS_CMD_TYPE_GET_USER_NUM].data        = {0x6C, 0x63, 0x62, 0x55, 0x00, 0x00, 0x1A},
    [FPS_CMD_TYPE_SET_SECURITY_LEVEL].data  = {0x6C, 0x63, 0x62, 0x57, 0x06, 0x00, 0xF8},
    [FPS_CMD_TYPE_RESET_SENSOR].data        = {0x6C, 0x63, 0x62, 0x33, 0x00, 0x00, 0xF8},
};

/*
 * ======== _checkSum() ========
 * Calculate XTB0111 request's checksum
 */
static void _checkSum(
    XTB0811_CmdData *cmd_ptr)
{
    int i;

    cmd_ptr->data[6] = 0;
    for (i = 1; i < 6; i++) {
        cmd_ptr->data[6] = (cmd_ptr->data[6] + cmd_ptr->data[i]) & 0xFF;
    }
}
#endif

static bool _touched  = false;
static bool _ltdRunning = false;
static bool _killLTD = false;
static uint32_t _touchPeriod = 0;
static bool _confirm_reset = false;

/**
 * @brief Typedef for safely calling external function which
 * has I/O access, i.e. UART, FPS, etc.
 */
typedef void (*ltdCommadFunc_t)(void );

/* === Start of local function declarations === */
void fps_longTouchDetector(
    void    *data_ptr,
    uint16_t dataSize);

static OTK_Return fps_sendCommand(
    FPS_CmdData *cmd_ptr);

static OTK_Return fps_recvResponse(
    FPS_RespData *resp_ptr);

static OTK_Return fps_capture(
    uint8_t idx);

static uint8_t fps_match(void);

static OTK_Return fps_resp_processor(
    FPS_RespData *resp_ptr);
/* === End of local function declarations === */

/*
 * ======== FPS_init() ========
 * Configure FPS touch sensor pin.
 */
OTK_Return FPS_init() {
    /* Set FPS security level to 5 -- 6 is too restrict and match failed easity */
    if (FPS_setSecurityLevel(5) != OTK_RETURN_OK) {
        OTK_LOG_ERROR("FPS_setSecurityLevel failed!!");       
        return (OTK_RETURN_FAIL);
    }

    if (FPS_resetSensor() != OTK_RETURN_OK) {
        OTK_LOG_ERROR("FPS_resetSensor failed!!");       
       return (OTK_RETURN_FAIL);        
    }

    return (OTK_RETURN_OK);
} 

void FPS_powerOn() {
    nrf_gpio_pin_write(OTK_PIN_FPS_PWR_EN, NRF_GPIO_PIN_PULLUP);
    nrf_delay_ms(20);
}

void FPS_powerOff() {
    nrf_gpio_pin_write(OTK_PIN_FPS_PWR_EN, NRF_GPIO_PIN_PULLDOWN);
}

/*
 * ======== FPS_longTouchDetectorStart() ========
 * Start touch handler to detect if FPS is touched.
 */
OTK_Return FPS_longTouchDetectorStart(void)
{
    ret_code_t errCode;

    if (!_ltdRunning) {
        if (app_sched_queue_space_get() > 0) {
            _killLTD = false;
            /* Initialize _touchStateHandler task */
            errCode = app_sched_event_put(NULL, 0, fps_longTouchDetector);
            APP_ERROR_CHECK(errCode);
            if (NRF_SUCCESS != errCode) {
                OTK_LOG_ERROR("Start long touch detector failed, scheduler error!!");
                OTK_shutdown(OTK_ERROR_SCHED_ERROR, false);
                return (OTK_RETURN_FAIL);        
            }
            _ltdRunning = true;
        }
        else {
            OTK_LOG_ERROR("Start touch handler failed, scheduler event buffer full!!");
            OTK_shutdown(OTK_ERROR_SCHED_ERROR, false);
            return (OTK_RETURN_FAIL);                
        }
    }

    return (OTK_RETURN_OK);
}

/*
 * ======== FPS_longTouchDetectorStop() ========
 * Stop touch handler to yield scheduler for other task.
 */
void FPS_longTouchDetectorStop(void)
{
    _killLTD = true;
}

/*
 * ======== FPS_setSecurityLevel() ========
 * Configure FP module security level, 0x06 as the highest for banking
 */
OTK_Return FPS_setSecurityLevel(
    uint8_t level)
{
#if defined(FPS_XTB0811)
    OTK_Return ret;
    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_SET_SECURITY_LEVEL];

    /* Set security level */
    cmd.data[4] = level;
    /* Calculate checksum. */
    _checkSum(&cmd);
    fps_sendCommand((FPS_CmdData *)&cmd);
    ret = fps_recvResponse(&resp);

    if (OTK_RETURN_OK == ret) {
        NRF_LOG_INFO("Set FP security level to : (%d)", level);
    }
    else {
        OTK_LOG_ERROR("Set FP security level failed!!");
    }

    return (ret);
#else 
    return (OTK_RETURN_FAIL);
#endif
}

/*
 * ======== FPS_resetSensor() ========
 * Erase all finger prints.
 *
 * Parameters:
 *
 * Returns:
 *  None
 */
OTK_Return FPS_resetSensor(void)
{
#if defined(OTK_V1_2)
#if defined(FPS_XTB0811)
    OTK_Return ret = OTK_RETURN_FAIL;
    int retries = 3;

    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_RESET_SENSOR];

    fps_sendCommand((FPS_CmdData *)&cmd);

    while (retries > 0 && OTK_RETURN_OK != fps_recvResponse(&resp)) {
        nrf_delay_ms(10);
        fps_sendCommand((FPS_CmdData *)&cmd);
        retries--;     
    }

    if (retries > 0) {
        ret = OTK_RETURN_OK;
    }
    else {
        OTK_LOG_ERROR("Reset FPS sensor failed!!");      
    }

    return ret;
#else
    return (OTK_RETURN_FAIL);
#endif
#endif
    return (OTK_RETURN_OK);
}

/*
 * ======== FPS_captureAndEnroll() ========
 * Capture and enroll finger print.
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return FPS_captureAndEnroll(void)
{
#if defined(FPS_XTB0811)
    int idx = 0;
    int fpId = 0;
    int failure_count = 0;

    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_CAPTURE];
    OTK_Return ret = OTK_RETURN_OK;

    if (FPS_getUserNum() >= OTK_MAX_FINGER_PRINT_NUM) {
        OTK_LOG_ERROR("Reached maximum fringerprints!!");
        return (OTK_RETURN_FAIL);
    }

    uint32_t _startTime = app_timer_cnt_get();

    LED_all_off();
    LED_setCadenceType(LED_CAD_FPS_CAPTURING);
    LED_cadence_start();

    /* Exit loop when captured FP meet maximum limit or timeout */
    while(idx < OTK_FINGER_PRINT_MAX_CAPTURE_NUM && failure_count < 3 &&
        (app_timer_cnt_get() - _startTime) < APP_TIMER_TICKS(5000)) {

        /* Start capture only at FPS is touched */
        if (FPS_isTouched()) {
            OTK_LOG_DEBUG("FP touched +")
            if (OTK_RETURN_OK == fps_capture(idx)) {
                idx++;
                LED_all_off();
                LED_on(OTK_LED_GREEN);
            }
            else {
                LED_all_off();
                LED_on(OTK_LED_RED);
                failure_count++;
            }

            uint8_t _untouchCounter = 0;
            while (_untouchCounter < FPS_TOUCH_DEBOUNCE) {
                if (FPS_isTouched()) {
                    _untouchCounter = 0;
                }
                else {
                    _untouchCounter++;
                }
                FPS_resetSensor();
                nrf_delay_ms(180);
            }
            OTK_LOG_DEBUG("FP released - ")

            FPS_resetSensor();
            LED_all_off();
            _startTime = app_timer_cnt_get();

            LED_setCadenceType(LED_CAD_FPS_CAPTURING);
            LED_cadence_start();
        }
        __WFE();
    }

    LED_all_off();


    if (idx < OTK_FINGER_PRINT_MIN_CAPTURE_NUM) {
        /* Failed to capture. */
        LED_on(OTK_LED_RED);
        FPS_resetSensor();
        return (OTK_RETURN_FAIL);
    }

    NRF_LOG_INFO("Captured #%d FPs.", idx);
    /* Captured, then enroll. */
    cmd = _cmdData[FPS_CMD_TYPE_ENROLL];
    fpId = FPS_getUserNum() + 1;
    cmd.data[4] = fpId;
    _checkSum(&cmd);
    fps_sendCommand((FPS_CmdData *)&cmd);
    ret = fps_recvResponse(&resp);

    /* Do not rely on fps_recvResponse() return value
     * which can failed at UART_read() -- Enrollment seems takes longer to response
     * check FP fingerprints number is more reliable 
     */
    if (FPS_getUserNum() == fpId) {
        NRF_LOG_INFO("FP enrolled!");
        /* Run capture and match to confirm the enrolled fingerprint can be matched,
         * if not, the enrollment is bad, erase all fingerprint and return fail for enrollment.
         */
        if (FPS_captureAndMatch(3) == 0) {
            OTK_LOG_ERROR("Bad FP, FP enrolled but cannot match, erase all and return fail!!");
            FPS_eraseAll();
            ret = OTK_RETURN_FAIL;
        }
    }
    else {
        OTK_LOG_ERROR("FP enrollment failed!!");
        ret = OTK_RETURN_FAIL;
    }

    LED_on(ret == OTK_RETURN_FAIL ? OTK_LED_RED : OTK_LED_GREEN);
    FPS_resetSensor();

    return (ret);
#else 
    return (OTK_RETURN_FAIL);
#endif
}


/*
 * ======== FPS_eraseOne() ========
 * Erase a finger print.
 *
 * Parameters:
 *
 * Returns:
 *  None
 */
OTK_Return FPS_eraseOne(
    uint8_t idx)
{
#if defined(FPS_XTB0811)
    OTK_Return ret;

    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_ERASE_ONE];

    /* Set idx to erase. */
    cmd.data[4] = idx;
    /* Calculate checksum. */
    _checkSum(&cmd);
    fps_sendCommand((FPS_CmdData *)&cmd);
    ret = fps_recvResponse(&resp);

    if (OTK_RETURN_OK != ret) {
        OTK_LOG_ERROR("Erase one fingerprint failed!!");
    }
 
    return (ret);
#else 
    return (OTK_RETURN_FAIL);
#endif
}

/*
 * ======== FPS_eraseAll() ========
 * Erase all finger prints.
 *
 * Parameters:
 *
 * Returns:
 *  None
 */
OTK_Return FPS_eraseAll(void)
{
#if defined(FPS_XTB0811)
    OTK_Return ret;

    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_ERASE_ALL];

    fps_sendCommand((FPS_CmdData *)&cmd);
    ret = fps_recvResponse(&resp);
    if (OTK_RETURN_OK == ret) {
        NRF_LOG_INFO("All fingerpints erased!");
    }

    return (ret);
#else 
    return (OTK_RETURN_FAIL);
#endif
}

/*
 * ======== FPS_captureAndMatch() ========
 * Capture and match finger print.
 *
 * Parameters:
 *
 * Returns:
 *  0 - Not matched.
 *  Otherwise - Matched index.
 */
uint8_t FPS_captureAndMatch(uint8_t min_matches)
{
#if defined(FPS_XTB0811)
    uint8_t fpId = 0;
    uint8_t _match = 0;
    uint32_t _startTime = app_timer_cnt_get();

    LED_all_off();
    LED_setCadenceType(LED_CAD_FPS_CAPTURING);
    LED_cadence_start();

    /* Exit loop when captured FP matched or timeout */
    while(_match < min_matches && (app_timer_cnt_get() - _startTime) < APP_TIMER_TICKS(10000)) {
        /* Start capture only at FPS is touched */
        if (FPS_isTouched()) {
            OTK_LOG_DEBUG("FP touched + ")
            if (OTK_RETURN_OK == fps_capture(0)) {
                fpId = fps_match();
            }
            if (fpId > 0 ) {
                _match++;
                LED_all_off();
                LED_on(OTK_LED_GREEN);
            }
            else {
                LED_all_off();
                LED_on(OTK_LED_RED);
            }

            uint8_t _untouchCounter = 0;
            while (_untouchCounter < FPS_TOUCH_DEBOUNCE) {
                if (FPS_isTouched()) {
                    _untouchCounter = 0;
                }
                else {
                    _untouchCounter++;
                }
                FPS_resetSensor();
                nrf_delay_ms(180);
            }
            OTK_LOG_DEBUG("FP relased - ")

            FPS_resetSensor();
            LED_all_off();
            LED_setCadenceType(LED_CAD_FPS_CAPTURING);
            LED_cadence_start();
        }
        __WFE();
    }

    LED_all_off();

    return (fpId);
#else 
    return (0);
#endif
}

/*
 * ======== FPS_getUserNum() ========
 * Get enrolled fingerprint number.
 */
uint8_t FPS_getUserNum(void)
{
#if defined(FPS_XTB0811)
    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_GET_USER_NUM];

    fps_sendCommand((FPS_CmdData *)&cmd);
    if (OTK_RETURN_OK != fps_recvResponse(&resp)) {
        return (0);
    }

    XTB0811_RespData *data_ptr;
    data_ptr = &resp.u.xtb0811;

    return (data_ptr->data[5]);
#else 
    return (0);
#endif
}


bool FPS_isTouched() {
    if (nrf_gpio_pin_read(OTK_PIN_FPS_TOUCHED)) {
        OTK_extend();
        return (true);
    }

    return (false);
} 

/* ============== LOCAL FUNCTIONS =============== */

/*
 * ======== fps_longTouchDetector() ========
 * GPIO handler for FPS touch.
 */
void fps_longTouchDetector(
    void    *data_ptr,
    uint16_t dataSize)
{
    UNUSED_PARAMETER(data_ptr);
    UNUSED_PARAMETER(dataSize);

    bool _tpTouchState = false;
    bool _cacheTouchState = false;
    uint8_t _untouchCounter = 0;
    uint32_t _touchStartTime = 0; 
    ltdCommadFunc_t ltdCmdFunc = NULL;

    OTK_LOG_DEBUG("Starting Long-Touch-Detector!!");

    while (!_killLTD) {
        _tpTouchState = FPS_isTouched();

        if (_tpTouchState == true) {
            _cacheTouchState = true;
            _untouchCounter = 0;
            FPS_resetSensor();
        }
        else if (_touched) {
            _untouchCounter++;

             if (_untouchCounter > FPS_TOUCH_DEBOUNCE) {
                _cacheTouchState = _tpTouchState;
                _untouchCounter = 0;            
            }
        }

        if (_touchStartTime > 0) {
            _touchPeriod = app_timer_cnt_get() - _touchStartTime;
        }

        if (_touchPeriod > APP_TIMER_TICKS(10000)) {
            if (_confirm_reset) {
                ltdCmdFunc = OTK_resetConfirmed;
            }
            else if (OTK_isAuthorized()) {
                ltdCmdFunc = OTK_unlock;
            }
            else {
                OTK_shutdown(OTK_ERROR_NO_ERROR, false);
            }
            break;
        }
        else if (_touchPeriod > APP_TIMER_TICKS(2400) &&
            !_confirm_reset && !OTK_isAuthorized()) {
            if (OTK_isLocked()) {
                ltdCmdFunc = OTK_fpValidate;
            }
            else {
                ltdCmdFunc = OTK_lock;
            }
            break;
        }

        if (_cacheTouchState != _touched) {
            OTK_LOG_DEBUG("FPS State: %s, %d ms", (_tpTouchState == true ? "touched" : "untouched"), _touchPeriod / APP_TIMER_TICKS(1));
            _touched = _cacheTouchState;

             if (_cacheTouchState) {
                _touchStartTime = app_timer_cnt_get();
            }
            else {
                _touchStartTime = 0;
                _touchPeriod = 0;
            }
        }            

        __WFE();
    }

    _touched = false;
    _touchStartTime = 0;
    _touchPeriod = 0;
    _ltdRunning = false;

    OTK_LOG_DEBUG("FPS State: %s, %d ms", (_tpTouchState == true ? "touched" : "untouched"), _touchPeriod / APP_TIMER_TICKS(1));
    OTK_LOG_DEBUG("End of Long-Touch-Detector!!");

    if (ltdCmdFunc != NULL) {
        ltdCmdFunc();
    }
}


/*
 * ======== fps_sendCommand() ========
 */
static OTK_Return fps_sendCommand(
    FPS_CmdData *cmd_ptr)
{
    OTK_Return ret;

#if defined(FPS_XTB0811)
    ret = UART_write(cmd_ptr->u.xtb0811.data, sizeof(cmd_ptr->u.xtb0811.data));

    if (OTK_RETURN_OK != ret) {
        OTK_LOG_ERROR("Send FPS command failed!!");
        OTK_LOG_HEXDUMP(cmd_ptr->u.xtb0811.data, sizeof(cmd_ptr->u.xtb0811.data));       
    }

    return (ret);
#else
    return (OTK_RETURN_FAIL);
#endif
}

/*
 * ======== fps_recvResponse() ========
 */
static OTK_Return fps_recvResponse(
    FPS_RespData *resp_ptr)
{
    OTK_Return ret;

#if defined(FPS_XTB0811)
    XTB0811_RespData *data_ptr;
    data_ptr = &resp_ptr->u.xtb0811;

    ret = UART_read(data_ptr->data, sizeof(data_ptr->data));

    if (OTK_RETURN_OK != ret) {
        OTK_LOG_ERROR("UART_read failed!!");
        return (OTK_RETURN_FAIL);
    }

    ret = fps_resp_processor(resp_ptr);

    return (ret);

#else /* defined(FPS_XTB0811) */
    return (OTK_RETURN_FAIL);
#endif /* defined(FPS_XTB0811) */
}

/*
 * ======== fps_capture() ========
 * Capture finger print to given index(idx)
 */
static OTK_Return fps_capture(
    uint8_t idx)
{
#if defined(FPS_XTB0811)
    OTK_Return ret;
    FPS_RespData resp = {0};
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_CAPTURE];

    /* Capture idx. */
    /* Set capture num */
    cmd.data[4] = idx;
    /* Calculate checksum. */
    _checkSum(&cmd);
    fps_sendCommand((FPS_CmdData *)&cmd);
    ret = fps_recvResponse(&resp);

    if (OTK_RETURN_OK == ret) {
        NRF_LOG_INFO("FP Captured!");
    }
    
    return (ret);
#else 
    return (OTK_RETURN_FAIL);
#endif
}

/*
 * ======== fps_match() ========
 * Match finger print.
 *
 * Returns:
 *  0: Not match.
 *  Otherwise: Index of matched finger print.
 */
static uint8_t fps_match(void)
{
#if defined(FPS_XTB0811)
    FPS_RespData resp = {0};
    XTB0811_RespData *data_ptr;
    XTB0811_CmdData cmd = _cmdData[FPS_CMD_TYPE_MATCH_1_N];

    fps_sendCommand((FPS_CmdData *)&cmd);
    fps_recvResponse(&resp);

    data_ptr = &resp.u.xtb0811;
    /* Byte 5: 0 for success. Byte 6 the matched indedx. */
    if (0 == data_ptr->data[5]) {
        NRF_LOG_INFO("FP NO Match!");
    }
    else {
        NRF_LOG_INFO("FP Matched : (%d)", data_ptr->data[5]);
    }
    return (data_ptr->data[5]);
#else 
    return (0);
#endif
}

/*
 * ======== fps_resp_processor() ========
 * Process FPS response 
 */
static OTK_Return fps_resp_processor(FPS_RespData *resp_ptr) {
#if defined(FPS_XTB0811)
    OTK_Return ret = OTK_RETURN_OK;
    XTB0811_RespData *data_ptr;
    data_ptr = &resp_ptr->u.xtb0811;

    if (0x6C == data_ptr->data[0] && 0x62 == data_ptr->data[1] &&
        0x63 == data_ptr->data[2]) {
        switch (data_ptr->data[3]) {
            case 0x51:  /* FPS_CMD_TYPE_CAPTURE */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;

                    switch (data_ptr->data[4]) {        
                        case 0xB1:
                            OTK_LOG_ERROR("Fingerprint too small"); 
                            break;
                        case 0xB2:
                            OTK_LOG_ERROR("No fingerprint");
                            break;
                        case 0xB7:
                            OTK_LOG_ERROR("Wet finger");
                            break;
                        default:
                            OTK_LOG_ERROR("Unknown response (0x%2X)", data_ptr->data[4]);
                    }
                }
                break;
            case 0x7F: /* FPS_CMD_TYPE_ENROLL */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;

                    switch (data_ptr->data[4]) {        
                        case 0x83:
                            OTK_LOG_ERROR("Incorrec ID number"); 
                            break;
                        case 0x91:
                            OTK_LOG_ERROR("Memory full");
                            break;
                        case 0x93:
                            OTK_LOG_ERROR("ID in use");
                            break;
                        case 0x94:
                            OTK_LOG_ERROR("Extract fingerprint less than 3");
                            break;
                        default:
                            OTK_LOG_ERROR("Unknown response (0x%2X)", data_ptr->data[4]);
                    }
                }
                break;
            case 0x73: /* FPS_CMD_TYPE_ERASE_ONE */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;

                    switch (data_ptr->data[4]) {        
                        case 0x83:
                            OTK_LOG_ERROR("Incorrect parameter"); 
                            break;
                        case 0x90:
                            OTK_LOG_ERROR("ID not found");
                            break;
                        case 0xFF:
                            OTK_LOG_ERROR("Write ROM failed");
                            break;
                        default:
                            OTK_LOG_ERROR("Unknown response (0x%2X)", data_ptr->data[4]);
                    }
                }
                break;
            case 0x54: /* FPS_CMD_TYPE_ERASE_ALL */
                /* 0x90 = No fingerprint to be erased */
                if (0 != data_ptr->data[4] && 0x90 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;
                    OTK_LOG_ERROR("Erase all failed"); 
                }
                break;
            case 0x71: /* FPS_CMD_TYPE_MATCH_1_N */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;
                    OTK_LOG_ERROR("Match fingerprint failed"); 
                }
                break;
            case 0x55: /* FPS_CMD_TYPE_GET_USER_NUM */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;
                    OTK_LOG_ERROR("Get enrolled user number failed"); 
                }
                break;
            case 0x57: /* PS_CMD_TYPE_SET_SECURITY_LEVEL */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;

                    switch (data_ptr->data[4]) {        
                        case 0x83:
                            OTK_LOG_ERROR("Incorrect parameter"); 
                            break;
                        case 0x81:
                            OTK_LOG_ERROR("Communication error");
                            break;
                        default:
                            OTK_LOG_ERROR("Unknown response (0x%2X)", data_ptr->data[4]);
                    }
                }
                break;
            case 0x33: /* FPS_CMD_TYPE_RESET_SENSOR */
                if (0 != data_ptr->data[4])
                {
                    ret = OTK_RETURN_FAIL;
                    OTK_LOG_ERROR("Unknown error"); 
                }
                break;
            default:
                OTK_LOG_ERROR("Command not supported!!");
                ret = OTK_RETURN_FAIL;                
        }      
    }
    else {
        OTK_LOG_ERROR("Invalid FPS response!!");            
        ret = OTK_RETURN_FAIL;        
    }

    

    if (OTK_RETURN_OK != ret) {
        OTK_LOG_ERROR("FPS response:");
        OTK_LOG_HEXDUMP(data_ptr->data, sizeof(data_ptr->data));       
    }

    return (ret);

#else /* defined(FPS_XTB0811) */
    return (OTK_RETURN_FAIL);
#endif /* defined(FPS_XTB0811) */
}

void FPS_confirmReset() {
    _confirm_reset = true;
}

