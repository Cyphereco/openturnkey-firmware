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
#include "app_timer.h"
#include "app_util.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_serial.h"
#include "otk.h"
#include "uart.h"
#include "led.h"
#include "boards.h"

#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */

#if defined NRF_LOG_MODULE_NAME
#undef NRF_LOG_MODULE_NAME
#endif

#define NRF_LOG_MODULE_NAME otk_uart
NRF_LOG_MODULE_REGISTER();

static void sleep_handler(void)
{
    __WFE();
    __SEV();
    __WFE();
}

NRF_SERIAL_DRV_UART_CONFIG_DEF(m_uart0_drv_config,
                      OTK_PIN_FPS_UART_RX, OTK_PIN_FPS_UART_TX,
                      0, 0,
                      NRF_UART_HWFC_DISABLED, NRF_UART_PARITY_EXCLUDED,
                      NRF_UART_BAUDRATE_115200,
                      UART_DEFAULT_CONFIG_IRQ_PRIORITY);

#define SERIAL_FIFO_TX_SIZE 32
#define SERIAL_FIFO_RX_SIZE 32

NRF_SERIAL_QUEUES_DEF(serial_queues, SERIAL_FIFO_TX_SIZE, SERIAL_FIFO_RX_SIZE);


#define SERIAL_BUFF_TX_SIZE 1
#define SERIAL_BUFF_RX_SIZE 1

NRF_SERIAL_BUFFERS_DEF(serial_buffs, SERIAL_BUFF_TX_SIZE, SERIAL_BUFF_RX_SIZE);

NRF_SERIAL_CONFIG_DEF(serial_config, NRF_SERIAL_MODE_IRQ,
                      &serial_queues, &serial_buffs, NULL, sleep_handler);


NRF_SERIAL_UART_DEF(serial_uart, 0);

/*
 * ======== UART_init() ========
 * Initialize NRF's UART
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return UART_init(void)
{
    ret_code_t errCode;

    errCode = nrf_serial_init(&serial_uart, &m_uart0_drv_config, &serial_config);
    APP_ERROR_CHECK(errCode);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    /* need some time to get uart fully ready*/
    nrf_delay_ms(20);
    return (OTK_RETURN_OK);
}

/*
 * ======== UART_uninit() ========
 * Uninitialize NRF's UART
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return UART_uninit(void)
{
    ret_code_t errCode;

    errCode = nrf_serial_uninit(&serial_uart);
    APP_ERROR_CHECK(errCode);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    nrf_delay_ms(10);
    return (OTK_RETURN_OK);
}

/*
 * ======== UART_read() ========
 * Read data from UART
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return UART_read(
    uint8_t *buf_ptr,
    size_t   readSize)
{
    ret_code_t errCode;
    
    //NRF_LOG_INFO("%s %d", __FUNCTION__, __LINE__);
    errCode = nrf_serial_read(&serial_uart, buf_ptr, readSize, NULL, OTK_FPS_UART_TIMEOUT);

    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}

/*
 * ======== UART_write() ========
 * Write data to UART
 *
 * Parameters:
 *
 * Returns:
 */
OTK_Return UART_write(
    uint8_t *buf_ptr, size_t writeSize)
{
    ret_code_t errCode;
    
    errCode = nrf_serial_write(&serial_uart,
                           buf_ptr,
                           writeSize,
                           NULL,
                           OTK_FPS_UART_TIMEOUT);
    if (errCode != NRF_SUCCESS) {
        return (OTK_RETURN_FAIL);
    }

    return (OTK_RETURN_OK);
}
