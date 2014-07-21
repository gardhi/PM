//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef __PMSQM_H
#define __PMSQM_H

#include <pmimpl.h>
#include <sqmdatapoints.h>

#ifdef __cplusplus
extern "C" {
#endif

// these marker ids correspond precisely to the marker ids on the server side
// please see the sqm site too look up each value.

#define PMSQM_DATAID_POWER_BKL_TOTAL                        DATAID_POWER_BKL_TOTAL 
        // total ms backlight on in a standard session

#define PMSQM_DATAID_POWER_BKL_ON                           DATAID_POWER_BKL_ON  // (Stream) 
        // individual backlight session lengths in ms in a standard session

#define PMSQM_DATAID_POWER_BKL_OFF                          DATAID_POWER_BKL_OFF  // (Stream) 
        // individual backlight off session lengths in ms in a standard session

#define PMSQM_DATAID_POWER_BAT_CHARGE                       DATAID_POWER_BAT_CHARGE 
        // total time battery charged in a week-long session

#define PMSQM_DATAID_POWER_BAT_DRAIN                        DATAID_POWER_BAT_DRAIN  // (Stream) 
        // individual battery drain session lengths (no charging) in a week-long session

#define PMSQM_DATAID_POWER_USER_SHUTDOWNS                   DATAID_POWER_USER_SHUTDOWNS 
        // total number of user-initiated shutdowns in a week-long session

#define PMSQM_DATAID_POWER_BAT_START_CHARGE_LEVEL           DATAID_POWER_BAT_START_CHARGE_LEVEL  // (Stream) 
        // individual percentages of battery charge level when charging session started in a week-long session

BOOL PMSQM_Start(void);
void PMSQM_Set(DWORD markerId, DWORD dwValue);

#ifdef __cplusplus
}
#endif

#endif
