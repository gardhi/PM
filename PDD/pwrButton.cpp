
//------------------------------------------------------------------------------
//
//  Copyright (C) 2008-2009, Freescale Semiconductor, Inc. All Rights Reserved.
//  THIS SOURCE CODE, AND ITS USE AND DISTRIBUTION, IS SUBJECT TO THE TERMS
//  AND CONDITIONS OF THE APPLICABLE LICENSE AGREEMENT
//
//------------------------------------------------------------------------------
//
//  File:  pwrbutton.c
//
//  This file contains driver support for the power button.
//
//-----------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable: 4115 4201 4204 4214)
#include <windows.h>
#include <ceddk.h>
#include "pwstatemgr.h"
#include "pwstates.h"
#pragma warning(pop)
#include "C:\WINCE800\platform\rrm_ppc_windows\SRC\INC\bsp.h"


//-----------------------------------------------------------------------------
// External Functions
/*
extern BOOL DDKGpioReadDataPin(DDK_GPIO_PORT port, UINT32 pin, UINT32 *pData);
extern BOOL DDKGpioClearIntrPin(DDK_GPIO_PORT port, UINT32 pin);
extern BOOL DDKGpioSetConfig(DDK_GPIO_PORT port, UINT32 pin, DDK_GPIO_DIR dir,
                             DDK_GPIO_INTR intr);

*/
//-----------------------------------------------------------------------------
// External Variables


//-----------------------------------------------------------------------------
// Defines

#define BSP_PWRBTN_GPIO_PORT                    DDK_GPIO_PORT3 //edited to fit rrmppc bsp
#define BSP_PWRBTN_GPIO_PIN                     29
#define BSP_PWRBTN_GPIO_IRQ                     IRQ_GPIO3_PIN29


// BSP_PWRBTN_DEBOUNCE_SAMPLE_MSEC specifies the time between
// sampling of power button value.
#define BSP_PWRBTN_DEBOUNCE_SAMPLE_MSEC         10      // 10 msec
//  BSP_PWRBTN_DEBOUNCE_SUSPEND_MSEC specifies the minimum
// assertion time for detecting that the user wants to suspend the system.
#define BSP_PWRBTN_DEBOUNCE_SUSPEND_MSEC        100     // 100 msec
// BSP_PWRBTN_DEBOUNCE_OFF_MSEC specifies the minimum
// assertion time for detecting that the user wants to power off the system.
#define BSP_PWRBTN_DEBOUNCE_OFF_MSEC            2000    // 2 sec
// BSP_PWRBTN_DEBOUNCE_IGNORE_MSEC specifies the minimum
// time for ignoring the power button after processing the previous
// button press.
#define BSP_PWRBTN_DEBOUNCE_IGNORE_MSEC         500    // 500 msec

#define BSP_PWRBTN_THREAD_PRIORITY              152

//-----------------------------------------------------------------------------
// Types


//-----------------------------------------------------------------------------
// Global Variables


//-----------------------------------------------------------------------------
// Local Variables
static HANDLE g_hPwrBtnThread;
static HANDLE g_hPwrBtnEvent;
static DWORD g_dwPwrBtnSysIntr = (DWORD)SYSINTR_UNDEFINED;


//-----------------------------------------------------------------------------
// Local Functions
static DWORD WINAPI PwrBtnThread (LPVOID lpParam);


//-----------------------------------------------------------------------------
//
//  Function:  Init
//
//  This function initializes the power button driver.  Called by the Device Manager to
//  initialize a device.
//
//  Parameters:
//      None.
//
//  Returns:
//      Returns TRUE if successful, otherwise returns FALSE.
//
//-----------------------------------------------------------------------------

BOOL Init(void)
{
    BOOL rc = FALSE;
    DWORD dwIrq;

    // Configure GPIO signal for power button events
    DDKGpioSetConfig(BSP_PWRBTN_GPIO_PORT,
                     BSP_PWRBTN_GPIO_PIN,
                     DDK_GPIO_DIR_IN,
                     DDK_GPIO_INTR_HIGH_LEV);
    DDKGpioClearIntrPin(BSP_PWRBTN_GPIO_PORT, BSP_PWRBTN_GPIO_PIN);


    // Create event for IST signaling
    g_hPwrBtnEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(g_hPwrBtnEvent == NULL)
    {
        ERRORMSG(TRUE, (TEXT("%s(): failed to create IST event\r\n"),  __WFUNCTION__));
        goto cleanUp;
    }

    // Register the GPIO IRQ
    dwIrq = BSP_PWRBTN_GPIO_IRQ;
    if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwIrq, sizeof(dwIrq), &g_dwPwrBtnSysIntr, sizeof(g_dwPwrBtnSysIntr), NULL))
    {
        ERRORMSG(TRUE, (TEXT("%s(): failed to map irq into sys intr\r\n"),  __WFUNCTION__));
        goto cleanUp;
    }

    if (!InterruptInitialize(g_dwPwrBtnSysIntr, g_hPwrBtnEvent, NULL, 0)) {
        ERRORMSG(TRUE, (TEXT("%s(): failed to register sys intr\r\n"),  __WFUNCTION__));
        goto cleanUp;
    }

    // Configure power button as wake source
    if (!KernelIoControl(IOCTL_HAL_ENABLE_WAKE, &g_dwPwrBtnSysIntr, sizeof(g_dwPwrBtnSysIntr), NULL, 0, NULL))
    {
        ERRORMSG(TRUE, (TEXT("%s(): failed to register as wake source\r\n"),  __WFUNCTION__));
        goto cleanUp;
    }

    // Create IST for power button interrupts
    g_hPwrBtnThread = CreateThread(NULL, 0, PwrBtnThread, NULL, 0, NULL);
    if (!g_hPwrBtnThread)
    {
        ERRORMSG(TRUE, (_T("CreateThread failed for power button driver!\r\n")));
        goto cleanUp;
    }
    else
    {
       CeSetThreadPriority(g_hPwrBtnThread, BSP_PWRBTN_THREAD_PRIORITY);
    }
    rc = TRUE;

cleanUp:

    return rc;

}


//-----------------------------------------------------------------------------
//
//  Function:  Deinit
//
//  This function deinitializes the power button driver.  Called by the Device Manager to
//  deinitialize a device.
//
//  Parameters:
//      None.
//
//  Returns:
//      Returns TRUE.
//
//-----------------------------------------------------------------------------
BOOL Deinit(void)
{
    // Power button driver is never unloaded.  This entry point is only defined
    // to meet minum requirements of the stream interface.
    return TRUE;
}


//-----------------------------------------------------------------------------
//
//  Function:  PwrBtnThread
//
//  This is the interrupt service thread for the power button driver.
//
//  Parameters:
//      lpParam
//          [in] Thread data passed to the function using the
//          lpParameter parameter of the CreateThread function. Not used.
//
//  Returns:
//      Returns thread exit code.
//
//-----------------------------------------------------------------------------
static DWORD WINAPI PwrBtnThread (LPVOID lpParam)
{
    UINT32 msec, start;
    UINT32 pinVal;
    WCHAR szState[MAX_PATH];
    DWORD dwStateFlags = 0;

    // Remove-W4: Warning C4100 workaround
    UNREFERENCED_PARAMETER(lpParam);

    // IST loop for servicing power button interrupts
    while (WaitForSingleObject(g_hPwrBtnEvent, INFINITE) != WAIT_FAILED)
    {
        // Capture start time of button press
        start = GetTickCount();

        // Query current system power state to determine if we are resuming
        // from suspend
        GetSystemPowerState(szState, MAX_PATH, &dwStateFlags);

        // Avoid requesting power state transition if the system is resuming from
        // suspend.  In such cases the POWER_STATE_SUSPEND flag will be set.
        if (!(POWER_STATE(dwStateFlags) & POWER_STATE_SUSPEND))
        {
            // Keep track of how long button is pressed
            msec = 0;

            // Loop to sample power button pin
            do
            {
                // Sleep between samples
                Sleep(BSP_PWRBTN_DEBOUNCE_SAMPLE_MSEC);

                // Read current power button pin
                DDKGpioReadDataPin(BSP_PWRBTN_GPIO_PORT, BSP_PWRBTN_GPIO_PIN, &pinVal);

                // Check if button is still pressed
                if (pinVal)
                {
                    // Update how long button is pressed
                    msec = GetTickCount() - start;
                }

            // Loop terminates if button is released or time to power off
            // system is reached
            } while (pinVal && (msec < BSP_PWRBTN_DEBOUNCE_OFF_MSEC));

             /// Edits by Gard----------------------------
             /// I am note sure if this is the right way to pass the strings.
            // Check if button press indicates user wants to power off the
            // system           
            if (msec >= BSP_PWRBTN_DEBOUNCE_OFF_MSEC)
            {
                PmSetSystemPowerState_I(STRING_OFF,0,0,TRUE);
            }


            // Check if button press indicates user wants to suspend the
            // system
            if (msec >= BSP_PWRBTN_DEBOUNCE_SUSPEND_MSEC)
            {
                PmSetSystemPowerState_I(STRING_SUSPEND,0,0,TRUE);
            }
            /// end edits by Gard------------------------
        }


        // After handling the button press, delay before servicing new button
        // interrupts to prevent from bouncing into suspend again
        Sleep(BSP_PWRBTN_DEBOUNCE_IGNORE_MSEC);

        // Clear and reenable button interrupts
        DDKGpioClearIntrPin(BSP_PWRBTN_GPIO_PORT, BSP_PWRBTN_GPIO_PIN);
        InterruptDone(g_dwPwrBtnSysIntr);
    }


    return 0;
}
