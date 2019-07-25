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

#ifndef _OTK_COMMON_H
#define _OTK_COMMON_H

#include "nrf_log.h"
#include "nrf_delay.h"

/**@brief Log error message with NRF_LOG_ERROR and appending funciton name and line number automatically. */
#define OTK_LOG_ERROR(ERR_MSG, ...)  NRF_LOG_ERROR("%s %d : " ERR_MSG, __FUNCTION__, __LINE__, ##__VA_ARGS__)
/**@brief Log debug message with NRF_LOG_DEBUG and appending funciton name and line number automatically. */
#define OTK_LOG_DEBUG(DBG_MSG, ...)  NRF_LOG_DEBUG("%s %d : " DBG_MSG, __FUNCTION__, __LINE__, ##__VA_ARGS__)
/**@brief Dump hex data and delay 5 ms for completion. */
#define OTK_LOG_HEXDUMP(hex, len)  {NRF_LOG_RAW_HEXDUMP_INFO(hex, len);nrf_delay_ms(len/8);}
/**@brief Log info and delay 5 ms for completion. */
#define OTK_LOG_RAW_INFO(...)  {NRF_LOG_RAW_INFO(__VA_ARGS__);nrf_delay_ms(5);}

#endif /* _OTK_COMMON_H */