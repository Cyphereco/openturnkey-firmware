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

#ifndef _PWRMGMT_H_
#define _PWRMGMT_H_

#include "otk.h"

OTK_Return PWRMGMT_init(void);
void PWRMGMT_run(void);
void PWRMGMT_feed(void);
void PWRMGMT_reboot(void);
void PWRMGMT_shutdown(
	OTK_Error err);
#endif
