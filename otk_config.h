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

#ifndef _OTK_CONFIG_H
#define _OTK_CONFIG_H

/* Define this for testnet. Will keep it as default value for current developing stage. */
//#define TESTNET

/**
 * @brief Defined for development test purpose.
 */
#define LED_TEST (0)
#define UNITTEST (0)

/* Define OTK_V1_2 for OTK version 1.1 */
#define OTK_V1_2

#define OTK_LED_BLUE_MASK                       (1 << 0)
#define OTK_LED_GREEN_MASK                      (1 << 1)
#define OTK_LED_RED_MASK                        (1 << 2)

#ifdef OTK_V1_2 
	/* Pin definition. */
	#define OTK_PIN_FPS_TOUCHED                     (17)
	#define OTK_PIN_FPS_PWR_EN                      (28)
	#define OTK_PIN_FPS_UART_RX                     (8)
	#define OTK_PIN_FPS_UART_TX                     (6)
	/* LED definition. */
	#define OTK_LED_OFF_STATE                       (0)
	#define OTK_LED_ON_STATE                        (1)
	#define OTK_LED_BLUE                            (25)
	#define OTK_LED_GREEN                           (26)
	#define OTK_LED_RED                             (27)
#else
	/* NRF52382_DEV_KIT */
	/* Define this to enable wake up button. */
	//#define OTK_WAKE_UP_BUTTON_ENABLE
	//#define OTK_WAKE_UP_BUTTON                      (0) /* Button 1. */

	/* Pin definition. */
	#define OTK_PIN_FPS_TOUCHED                     (23)
	#define OTK_PIN_FPS_UART_RX                     (25)
	#define OTK_PIN_FPS_UART_TX                     (24)
	/* LED definition. */
	#define OTK_LED_OFF_STATE                       (NRF_GPIO_PIN_PULLUP)
	#define OTK_LED_ON_STATE                        (NRF_GPIO_PIN_PULLDOWN)
	#define OTK_LED_BLUE             		        (17)
	#define OTK_LED_GREEN                    		(18)
	#define OTK_LED_RED               			    (19)
#endif

/* Wake up pin pull and sense definition. */
#define OTK_WAKE_UP_PIN_PULL                    (NRF_GPIO_PIN_PULLDOWN) 
#define OTK_WAKE_UP_PIN_SENSE                   (NRF_GPIO_PIN_SENSE_HIGH) 

/* FPS UART config. */
#define OTK_FPS_UART_FLOW_CONTROL               (APP_UART_FLOW_CONTROL_DISABLED)
#define OTK_FPS_UART_BAUDRATE                   (NRF_UART_BAUDRATE_115200)
#define OTK_FPS_UART_TIMEOUT                    (1500)   /* ms, don't set less than 1000, FPS enrollment takes longer to respond */

/* OTK standby timeout in second. */
#define OTK_PWRMGMT_STANDBY_TIMEOUT             (15)     /* used in sdk_config.h */

/* Fingerprint capture times = 3 ~ 5 */
#define OTK_FINGER_PRINT_MIN_CAPTURE_NUM        (3)	  /* Suggested minimum number is 3 */
#define OTK_FINGER_PRINT_MAX_CAPTURE_NUM        (5)   /* If larger than 5, capture will report error */

/* Max num of finger print can be added. */
#define OTK_MAX_FINGER_PRINT_NUM                (1)

#define OTK_FP_CAPTURE_TIMEOUT_MS               (5000) /* 5 seconds. */

#define OTK_FP_MATCH_TIMEOUT_MS                 (5000) /* 5 seconds. */
#define OTK_FP_MATCH_TRY_INTERVAL_MS            (100)
#define OTK_FP_MATCH_TRY_COUNT                  (OTK_FP_MATCH_TIMEOUT_MS / OTK_FP_MATCH_TRY_INTERVAL_MS)
#endif

