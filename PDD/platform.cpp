//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Use of this sample source code is subject to the terms of the Microsoft license
// agreement under which you licensed this sample source code. If you did not accept
// the terms of the license agreement, you are not authorized to use this sample
// source code. For the terms of the license, please see the license agreement
// between you and Microsoft or, if applicable, see the LICENSE.RTF on your install
// media or the root of your tools installation.  THE SAMPLE SOURCE CODE IS PROVIDED
// "AS IS", WITH NO WARRANTIES.
//
// Microsoft Public License (MS-PL)
//
// This license governs use of the accompanying software. If you use the software,
// you accept this license. If you do not accept the license, do not use the software.
//
// 1. Definitions The terms "reproduce," "reproduction," "derivative works," and
// "distribution" have the same meaning here as under U.S. copyright law.  
// A "contribution" is the original software, or any additions or changes to the software.
// A "contributor" is any person that distributes its contribution under this license.  
// "Licensed patents" are a contributor's patent claims that read directly on its contribution.
//
// 2. Grant of Rights 
// (A) Copyright Grant- Subject to the terms of this license, including the
// license conditions and limitations in section 3, each contributor grants you
// a non-exclusive, worldwide, royalty-free copyright license to reproduce its
// contribution, prepare derivative works of its contribution, and distribute its
// contribution or any derivative works that you create.
// (B) Patent Grant- Subject to the terms of this license, including the license
// conditions and limitations in section 3, each contributor grants you a
// non-exclusive, worldwide, royalty-free license under its licensed patents to
// make, have made, use, sell, offer for sale, import, and/or otherwise dispose of
// its contribution in the software or derivative works of the contribution in the
// software.
//
// 3. Conditions and Limitations 
// (A) No Trademark License- This license does not grant you rights to use any
// contributors' name, logo, or trademarks.
// (B) If you bring a patent claim against any contributor over patents that you
// claim are infringed by the software, your patent license from such contributor to
// the software ends automatically.
// (C) If you distribute any portion of the software, you must retain all copyright,
// patent, trademark, and attribution notices that are present in the software.
// (D) If you distribute any portion of the software in source code form, you may do
// so only under this license by including a complete copy of this license with your
// distribution. If you distribute any portion of the software in compiled or object
// code form, you may only do so under a license that complies with this license.
// (E) The software is licensed "as-is." You bear the risk of using it. The
// contributors give no express warranties, guarantees or conditions. You may have
// additional consumer rights under your local laws which this license cannot
// change. To the extent permitted under your local laws, the contributors exclude
// the implied warranties of merchantability, fitness for a particular purpose and
// non-infringement.

#include <pmimpl.h>
#include <nkintr.h>
#include <extfile.h>
#include <pmpolicy.h>
#include <PmSysReg.h>

#include "pwstates.h"
#include "pwstatemgr.h"

typedef BOOL (WINAPI * PFN_GwesPowerDown)(void);
typedef void (WINAPI * PFN_GwesPowerUp)(BOOL);
typedef BOOL (WINAPI * PFN_ShowStartupWindow)( void );

#define MAXACTIVITYTIMEOUT              (0xFFFFFFFF / 1000)  // in seconds

#ifndef PMCLASS_TOUCH
#define PMCLASS_TOUCH  TEXT("{7119776D-9F23-4E4E-9D9B-9AE962733770}")
#endif

// GWES suspend/resume functions:
PFN_GwesPowerDown gpfnGwesPowerDown = NULL;
PFN_GwesPowerUp gpfnGwesPowerUp = NULL;

// This variable is protected by the system power state critical section:
BOOL gfSystemSuspended = FALSE;
BOOL gfFileSystemsAvailable = TRUE;
GUID idBlockDevices = {0x8DD679CE, 0x8AB4, 0x43c8, { 0xA1, 0x4A, 0xEA, 0x49, 0x63, 0xFA, 0xA7, 0x15 } };

// Activity timer variables:
HANDLE ghevReloadActivityTimeouts;
HANDLE ghevRestartTimers;
BOOL gfGwesReady;
DWORD gdwACSuspendTimeout;                   // AC power, from any state other than "Resuming"
DWORD gdwACResumingSuspendTimeout;           // AC power, from "Resuming"
DWORD gdwBattSuspendTimeout;                 // Battery, from any state other than "Resuming"
DWORD gdwBattResumingSuspendTimeout;         // Battery, from "Resuming"
DWORD gdwStateTimeLeft = INFINITE;
INT giPreSuspendPriority;
INT giSuspendPriority;
DWORD gdwUnattendedModeRequests;

BOOL gfSupportPowerButtonRelease = FALSE;
BOOL gfPageOutAllModules = FALSE;

PowerStateManager *g_pPowerStateManager = NULL;

// Requires "C" linkage for compatibility with C language PDD implementations
extern "C" {
 POWER_BROADCAST_POWER_INFO gSystemPowerStatus;
};

///////////////////////////////////////////////////////////////////////////////
//
// PlatformValidatePMRegistry
//
// Checks the consistency of the system's power management registry
// settings.  It is called during during power manager initialization.
// If no registry settings are found, OEMs can use this routine to set
// them up.
//
// Returns:
//
//   ERROR_SUCCESS    The registry is OK, or it can be initialized/repaired.
// 
//   Error code       An error occurred while reading the registry.
//
//   FALSE            A fatal error is discovered and the registry is unusable.
//                    This error will halt PM initialization.  
// 
///////////////////////////////////////////////////////////////////////////////

EXTERN_C DWORD WINAPI
PlatformValidatePMRegistry (VOID)
{
	HKEY hkPM = NULL, hkSubkey;
	LPTSTR pszSubKey;
	DWORD dwStatus = ERROR_GEN_FAILURE;
	DWORD dwDisposition;

	SETFNAME (_T ("PlatformValidatePMRegistry"));

	PMLOGMSG (ZONE_INIT, (_T ("+%s\r\n"), pszFname));

	// Open the Power Manager registry key:

	dwStatus = RegCreateKeyEx (HKEY_LOCAL_MACHINE, PWRMGR_REG_KEY, 0, NULL, 0, 0, NULL,
							   &hkPM, &dwDisposition);
	if (dwStatus != ERROR_SUCCESS)
	{
		PMLOGMSG (ZONE_ERROR, (_T ("%s: can't open '%s', error is %d\r\n"), pszFname,
							   PWRMGR_REG_KEY, dwStatus));
	}

    // The registry key exists. Examine registry values and set global variables according
    // to registry settings:

	if (dwStatus == ERROR_SUCCESS && dwDisposition != REG_CREATED_NEW_KEY)
	{   // Exit Key.
		DWORD dwValue = 0;
		DWORD dwSize = sizeof (DWORD);

		if (RegQueryTypedValue (hkPM, PM_SUPPORT_PB_RELEASE, &dwValue, &dwSize, REG_DWORD) == ERROR_SUCCESS)
		{
			gfSupportPowerButtonRelease = (dwValue != 0);
		}
		dwSize = sizeof (dwSize);
		if (RegQueryTypedValue (hkPM, L"PageOutAllModules", &dwValue, &dwSize, REG_DWORD) == ERROR_SUCCESS && dwValue != 0)
			gfPageOutAllModules = TRUE;
		else
			gfPageOutAllModules = FALSE;
	}

	// Verify interface GUIDs:

	if (dwStatus == ERROR_SUCCESS)
	{
		pszSubKey = _T ("Interfaces");
		dwStatus = RegCreateKeyEx (hkPM, pszSubKey, 0, NULL, 0, 0, NULL, &hkSubkey, &dwDisposition);
		if (dwStatus == ERROR_SUCCESS)
		{
			if (dwDisposition == REG_CREATED_NEW_KEY)
			{
                // GUIDs for these interface classes are defined in public\COMMONk\oak\files\common.reg
                // and in public\COMMON\sdk\inc\pm.h

				LPTSTR pszName = PMCLASS_GENERIC_DEVICE;
				LPTSTR pszValue = _T ("Generic power-manageable devices");

				dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
										  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				if (dwStatus == ERROR_SUCCESS)
				{
					pszName = PMCLASS_BLOCK_DEVICE;
					pszValue = _T ("Power-manageable block devices");
					dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
											  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				}
				if (dwStatus == ERROR_SUCCESS)
				{
					pszName = PMCLASS_DISPLAY;
					pszValue = _T ("Power-manageable display drivers");
					dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
											  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				}
				if (dwStatus == ERROR_SUCCESS)
				{
					pszName = PMCLASS_NDIS_MINIPORT;
					pszValue = _T ("Power-manageable NDIS (network) miniport devices");
					dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
											  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				}
				if (dwStatus == ERROR_SUCCESS)
				{
					pszName = PMCLASS_BACKLIGHT;
					pszValue = _T ("Power-manageable backlight");
					dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
											  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				}
				if (dwStatus == ERROR_SUCCESS)
				{
					pszName = PMCLASS_TOUCH;
					pszValue = _T ("Power-manageable touch devices");
					dwStatus = RegSetValueEx (hkSubkey, pszName, 0, REG_SZ, (LPBYTE) pszValue,
											  (_tcslen (pszValue) + 1) * sizeof (*pszValue));
				}
                // Other power-manageable device classes can be added here.
			}
			RegCloseKey (hkSubkey);
		}

		PMLOGMSG (dwStatus != ERROR_SUCCESS && ZONE_ERROR,
				  (_T ("%s: error %d while creating or writing values in '%s\\%s'\r\n"), pszFname,
				   dwStatus, PWRMGR_REG_KEY, pszSubKey));
	}

	// Release resources

	if (hkPM != NULL)
		RegCloseKey (hkPM);

	PMLOGMSG (ZONE_INIT, (_T ("-%s: returning %d\r\n"), pszFname, dwStatus));

	return dwStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformDeviceListInit 
//
// Performs platform-specific initialization on a device list.
// Primarily, this involves determining what routines should be used to
// communicate with the class.
//
// Arguments:
//
//   pdl              Pointer to a device list (PDEVICE_LIST).  The device list is a 
//                    structure that describes a set of power-manageable devices.
//
// Returns:
//
//   TRUE             Device initialization completed: pdl->pInterface is set to
//                    the device interface used.
// 
//   FALSE            Device initialization failed.
// 
///////////////////////////////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI
PlatformDeviceListInit (PDEVICE_LIST pdl)
{
	BOOL fOk = FALSE;
	PDEVICE_INTERFACE pInterface;

	SETFNAME (_T ("PlatformDeviceListInit"));

	PREFAST_DEBUGCHK (pdl != NULL);
	DEBUGCHK (pdl->pGuid != NULL);

	if (*pdl->pGuid == idPMDisplayDeviceClass)
	{
		PMLOGMSG (ZONE_INIT || ZONE_PLATFORM,
				  (_T
				   ("%s: using display interface to access class %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\r\n"),
				   pszFname, pdl->pGuid->Data1, pdl->pGuid->Data2, pdl->pGuid->Data3,
				   (pdl->pGuid->Data4[0] << 8) + pdl->pGuid->Data4[1], pdl->pGuid->Data4[2],
				   pdl->pGuid->Data4[3], pdl->pGuid->Data4[4], pdl->pGuid->Data4[5],
				   pdl->pGuid->Data4[6], pdl->pGuid->Data4[7]));

		// Use the display driver interface to get to the device.  To remove
		// display code from the link, edit or conditionally compile this code out
		// of the Power Manager.

		extern DEVICE_INTERFACE gDisplayInterface;	// defined in the MDD

		pInterface = &gDisplayInterface;
	}
	else
	{
		// Use the standard stream interface to get to the device:
		PMLOGMSG (ZONE_INIT || ZONE_PLATFORM,
				  (_T
				   ("%s: using stream interface to access class %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\r\n"),
				   pszFname, pdl->pGuid->Data1, pdl->pGuid->Data2, pdl->pGuid->Data3,
				   (pdl->pGuid->Data4[0] << 8) + pdl->pGuid->Data4[1], pdl->pGuid->Data4[2],
				   pdl->pGuid->Data4[3], pdl->pGuid->Data4[4], pdl->pGuid->Data4[5],
				   pdl->pGuid->Data4[6], pdl->pGuid->Data4[7]));
		extern DEVICE_INTERFACE gStreamInterface;	// defined in the MDD

		pInterface = &gStreamInterface;
	}

	// Try to initialize the interface:
	if (pInterface != NULL)
	{
		if (pInterface->pfnInitInterface () == FALSE)
		{
			PMLOGMSG (ZONE_WARN,
					  (_T ("%s: warning: pfnInitInterface() failed for interface\r\n"), pszFname));
		}
		else
		{
			// Pass back the pointer
			pdl->pInterface = pInterface;
			fOk = TRUE;
		}
	}
	return fOk;
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformMapPowerStateHint
//
// Maps system power state hint values to known system power states that
// we maintain in the registry.
//
// Arguments:
//
//   dwHint                         The power state hint value (such as 
//                                  POWER_STATE_ON, POWER_STATE_SUSPEND) to map.
//
//   pszBuf                         A caller-supplied buffer to receive the name
//                                  string of the mapped power state.
//
//   dwBufChars                     The size of the caller-supplied buffer, 
//                                  in bytes.
//
// Returns:
//
//   ERROR_SUCCESS                  The power state hint value was mapped to a 
//                                  known system power state.  The mapped state
//                                  name was copied into pszBuf.
//
//   ERROR_FILE_NOT_FOUND           Unable to map the power state hint value to
//                                  known power state.
//
//   ERROR_INSUFFICIENT_BUFFER      The size of pszBuf is not large
//                                  enough to hold the mapped state name.
//
///////////////////////////////////////////////////////////////////////////////


EXTERN_C DWORD WINAPI
PlatformMapPowerStateHint (DWORD dwHint, LPTSTR pszBuf, DWORD dwBufChars)
{
	DWORD dwStatus = ERROR_SUCCESS;
	LPTSTR pszMappedStateName = NULL;

	SETFNAME (_T ("PlatformMapPowerStateHint"));

	// Mask off unused bits:
	dwHint &=
		(POWER_STATE_ON | POWER_STATE_USERIDLE | POWER_STATE_IDLE | POWER_STATE_SUSPEND |
		 POWER_STATE_OFF | POWER_STATE_RESET | POWER_STATE_CRITICAL);

	// Try to map the hint value.  Note that only one bit at a time should be set.
	switch (dwHint)
	{
	  case POWER_STATE_ON:
		  pszMappedStateName = _T ("on");
		  break;

	  case POWER_STATE_IDLE:
		  pszMappedStateName = _T ("screenoff");
		  break;

	  case POWER_STATE_SUSPEND:
		  pszMappedStateName = _T ("suspend");
		  break;

	  case POWER_STATE_OFF:	        // Power off, cold boot on resume.
          pszMappedStateName = _T ("off");  // Gard
          break;

	  case POWER_STATE_CRITICAL:	// Catastrophic power loss, shut down immediately.
		  pszMappedStateName = _T ("suspend");
		  break;

	  case POWER_STATE_RESET:	    // Flush files and reboot.
		  pszMappedStateName = _T ("reboot");
		  break;
	  case POWER_STATE_USERIDLE:
		  pszMappedStateName = _T ("useridle");
		  break;
	  default:
		  PMLOGMSG (ZONE_PLATFORM | ZONE_WARN,
					(_T ("%s: bad hint value 0x%x\r\n"), pszFname, dwHint));
		  dwStatus = ERROR_FILE_NOT_FOUND;
		  break;
	}

	DEBUGCHK (dwStatus == ERROR_SUCCESS);

	// If we were able to map the hint to a state name, copy it back to the caller:

	if (pszMappedStateName != NULL)
	{
		if (dwBufChars < (_tcslen (pszMappedStateName) + 1))
		{
			dwStatus = ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			_tcscpy_s (pszBuf, dwBufChars, pszMappedStateName);
		}
	}

	PMLOGMSG (dwStatus != ERROR_SUCCESS && ZONE_WARN,
			  (_T ("%s: returning %d\r\n"), pszFname, dwStatus));
	return dwStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformSetSystemPowerState
//
// Reads and verifies system power state information from the registry,
// then updates all devices appropriately.  The caller of this routine
// should hold the system power state critical section.
//
// Arguments:
//
//   pszName          The name of the new system power state (as defined 
//                    in the registry).
//
//   fForce           Set to TRUE to change power state quickly without 
//                    performing driver notifications. Set to FALSE to 
//                    change power state normally.
//
//   fInternal        Set to TRUE to indicate that the call originated from
//                    within the Power Manager, otherwise FALSE.
//
// Returns:
//
//   ERROR_SUCCESS    The system transitioned to the new system power state.
// 
//   Error code       An error occurred while trying to read the power state
//                    information from the registry.
//
///////////////////////////////////////////////////////////////////////////////

EXTERN_C DWORD WINAPI
PlatformSetSystemPowerState (LPCTSTR pszName, BOOL fForce, BOOL fInternal)
{
	DWORD dwStatus = ERROR_SUCCESS;
	PSYSTEM_POWER_STATE pNewSystemPowerState = NULL;
	PDEVICE_POWER_RESTRICTION pNewCeilingDx = NULL;
	BOOL fDoTransition = FALSE;
	INT iPreSuspendPriority = 0;
	static BOOL fFirstCall = TRUE;

	SETFNAME (_T ("PlatformSetSystemPowerState"));

	// Read system power state variables and construct new lists:

	if (gfFileSystemsAvailable)
		PmUpdateSystemPowerStatesIfChanged ();
	dwStatus = RegReadSystemPowerState (pszName, &pNewSystemPowerState, &pNewCeilingDx);

	// Did we get registry information about the new power state?

	if (dwStatus == ERROR_SUCCESS)
	{
		BOOL fSuspendSystem = FALSE;
		static BOOL fWantStartupScreen = FALSE;
		DWORD dwNewStateFlags = pNewSystemPowerState->dwFlags;

		// Assume we will update the system power state:
		fDoTransition = TRUE;

		// Are we going to suspend the system as a whole?
		if ((dwNewStateFlags &
			 (POWER_STATE_SUSPEND | POWER_STATE_OFF | POWER_STATE_CRITICAL | POWER_STATE_RESET)) != 0)
		{
			fSuspendSystem = TRUE;
		}

		// A "critical" suspend might mean we have totally lost battery power and need
		// to suspend really quickly.  Depending on the platform, OEMs may be able
		// to bypass driver notification entirely and rely on xxx_PowerDown() notifications
		// to suspend gracefully.  Or they may be able to implement a critical suspend
		// kernel ioctl.  This sample implementation is very generic and simply sets the
		// POWER_FORCE flag, which is not used.

		if (dwNewStateFlags & (POWER_STATE_CRITICAL | POWER_STATE_OFF | POWER_STATE_RESET))
		{
			fForce = TRUE;
		}

		// If everything seems OK, do the set operation:

		if (fDoTransition)
		{
			POWER_BROADCAST_BUFFER pbb;
			PDEVICE_LIST pdl;
			BOOL fResumeSystem = FALSE;

			// Send out system power state change notifications:
			pbb.Message = PBT_TRANSITION;
			pbb.Flags = pNewSystemPowerState->dwFlags;
			pbb.Length = _tcslen (pNewSystemPowerState->pszName) + 1;	// Char count not byte count for now
			if (pbb.Length > MAX_PATH)
			{
				// Truncate the system power state name -- note, we actually have MAX_PATH + 1
				// characters available.
				pbb.Length = MAX_PATH;
			}
			_tcsncpy_s (pbb.SystemPowerState, _countof (pbb.SystemPowerState),
						pNewSystemPowerState->pszName, pbb.Length);
			pbb.Length *= sizeof (pbb.SystemPowerState[0]);	           // Convert to byte count
			GenerateNotifications ((PPOWER_BROADCAST) & pbb);

			// Is GWES ready?
			if (!gfGwesReady)
			{
				if (WaitForAPIReady (SH_GDI, 0) == WAIT_OBJECT_0)
				{
					gfGwesReady = TRUE;
				}
			}

			// Are we suspending?
			if (fSuspendSystem && gpfnGwesPowerDown != NULL)
			{
				// Start the process of suspending GWES:
				if (gfGwesReady)
				{
					fWantStartupScreen = gpfnGwesPowerDown ();
				}
			}

			// Update global system state variables:
			PMLOCK ();
			PSYSTEM_POWER_STATE pOldSystemPowerState = gpSystemPowerState;
			PDEVICE_POWER_RESTRICTION pOldCeilingDx = gpCeilingDx;

			if (gpSystemPowerState != NULL
				&& (gpSystemPowerState->
					dwFlags & (POWER_STATE_SUSPEND | POWER_STATE_OFF | POWER_STATE_CRITICAL)) != 0)
			{
				// We are exiting a suspended state:
				fResumeSystem = TRUE;
			}
			gpSystemPowerState = pNewSystemPowerState;
			gpCeilingDx = pNewCeilingDx;
			PMUNLOCK ();

			// Are we suspending, resuming, or neither?
			if (fSuspendSystem)
			{
				INT iCurrentPriority;

				// We're suspending: update all devices other than block devices,
				// in case any of them need to access the registry or write files.

				PMLOGMSG (ZONE_PLATFORM || ZONE_RESUME,
						  (_T ("%s: suspending - notifying non-block drivers\r\n"), pszFname));
				for (pdl = gpDeviceLists; pdl != NULL; pdl = pdl->pNext)
				{
					if (*pdl->pGuid != idBlockDevices)
					{
						UpdateClassDeviceStates (pdl);
					}
				}

				// Notify the kernel that we are about to suspend.  This gives the
				// kernel an opportunity to clear wake source flags before we initiate
				// the suspend process.  If we don't do this and a wake source interrupt
				// occurs between the time we call PowerOffSystem() and the time
				// OEMPowerOff() is invoked, it is hard for the kernel to know whether or
				// not to suspend.

				PMLOGMSG (ZONE_PLATFORM || ZONE_RESUME,
						  (_T ("%s: calling KernelIoControl(IOCTL_HAL_PRESUSPEND)\r\n"), pszFname));
				KernelIoControl (IOCTL_HAL_PRESUSPEND, NULL, 0, NULL, 0, NULL);
				iCurrentPriority = CeGetThreadPriority (GetCurrentThread ());
				DEBUGCHK (iCurrentPriority != THREAD_PRIORITY_ERROR_RETURN);
				if (iCurrentPriority != THREAD_PRIORITY_ERROR_RETURN)
				{
					CeSetThreadPriority (GetCurrentThread (), giPreSuspendPriority);
					Sleep (0);
					CeSetThreadPriority (GetCurrentThread (), iCurrentPriority);
				}

				// Notify file systems that their block drivers will soon go away.
				// After making this call, this thread is the only one that can access
				// the file system (including registry and device drivers) without
				// blocking.  Unfortunately, this API takes and holds the file system
				// critical section, so other threads attempting to access the registry
				// or files may cause priority inversions.  To avoid priority problem
				// that may starve the Power Manager, we may raise our own priority to a
				// high level.  Do this if giSuspendPriority is non-zero.

				if (giSuspendPriority != 0)
				{
					iPreSuspendPriority = CeGetThreadPriority (GetCurrentThread ());
					DEBUGCHK (iPreSuspendPriority != THREAD_PRIORITY_ERROR_RETURN);
					PMLOGMSG (ZONE_PLATFORM,
							  (_T
							   ("%s: suspending: raising thread priority for 0x%08x from %d to %d\r\n"),
							   pszFname, GetCurrentThreadId (), iPreSuspendPriority,
							   giSuspendPriority));
					CeSetThreadPriority (GetCurrentThread (), giSuspendPriority);
				}

                // Discard code pages from drivers.  This is a diagnostic tool to
                // forcibly expose paging-related bugs that could cause apparently
                // random system crashes or hangs.  Optionally, OEMs can disable this
                // for production systems to speed up resume times.  We have to call
                // PageOutMode before FileSys Shutdown. Otherwise, it cause dead lock
                // between filesystem and loader.

				if (gfPageOutAllModules)
				{
					ForcePageout ();
				}

				if (g_pSysRegistryAccess)
					g_pSysRegistryAccess->EnterLock ();
				gfFileSystemsAvailable = FALSE;

				if ((dwNewStateFlags & POWER_STATE_RESET) != 0)
				{
					// Is this to be a cold boot?
					if (_tcscmp (pszName, _T ("coldreboot")) == 0)
					{
						SetCleanRebootFlag ();
					}
				}

				FileSystemPowerFunction (FSNOTIFY_POWER_OFF);

				// Update block device power states:
				PMLOGMSG (ZONE_PLATFORM || ZONE_RESUME,
						  (_T ("%s: suspending - notifying block drivers\r\n"), pszFname));
				pdl = GetDeviceListFromClass (&idBlockDevices);
				if (pdl != NULL)
				{
					UpdateClassDeviceStates (pdl);
				}

				// Handle resets and shutdowns here, after flushing files.  Since Windows CE does
				// not define a standard mechanism for handling shutdown (via POWER_STATE_OFF),
				// OEMs will need to fill in the appropriate code here.  Similarly, if an OEM does
				// not support IOCTL_HAL_REBOOT, they should not support POWER_STATE_RESET.

				if ((dwNewStateFlags & POWER_STATE_RESET) != 0)
				{
					// Should not return from this call, but if we do just suspend the system:
					KernelLibIoControl ((HANDLE) KMOD_OAL, IOCTL_HAL_REBOOT, NULL, 0, NULL, 0,
										NULL);
					RETAILMSG (TRUE,
							   (_T
								("PM: PlatformSetSystemPowerState: KernelIoControl(IOCTL_HAL_REBOOT) returned!\r\n")));
					DEBUGCHK (FALSE);	// Break into the debugger.
				}
			}
			else if (fResumeSystem)
			{
				// We're waking up from a resume -- update block device power states
				// so we can access the registry and/or files.

				PMLOGMSG (ZONE_PLATFORM || ZONE_RESUME,
						  (_T ("%s: resuming - notifying block drivers\r\n"), pszFname));
				pdl = GetDeviceListFromClass (&idBlockDevices);
				if (pdl != NULL)
				{
					UpdateClassDeviceStates (pdl);
				}

				// Notify file systems that their block drivers are back.

				FileSystemPowerFunction (FSNOTIFY_POWER_ON);
				gfFileSystemsAvailable = TRUE;
				if (g_pSysRegistryAccess)
					g_pSysRegistryAccess->LeaveLock ();

				// Update all devices other than block devices:

				PMLOGMSG (ZONE_PLATFORM || ZONE_RESUME,
						  (_T ("%s: resuming - notifying block drivers\r\n"), pszFname));
				for (pdl = gpDeviceLists; pdl != NULL; pdl = pdl->pNext)
				{
					if (*pdl->pGuid != idBlockDevices)
					{
						UpdateClassDeviceStates (pdl);
					}
				}

				// Tell GWES to wake up:
				if (gpfnGwesPowerUp != NULL && gfGwesReady)
				{
					gpfnGwesPowerUp (fWantStartupScreen);
					fWantStartupScreen = FALSE;
				}

				// Send out resume notification:
				pbb.Message = PBT_RESUME;
				pbb.Flags = 0;
				pbb.Length = 0;
				pbb.SystemPowerState[0] = 0;
				GenerateNotifications ((PPOWER_BROADCAST) & pbb);
			}
			else
			{
				// Update all devices without any particular ordering:
				UpdateAllDeviceStates ();
			}

			// Release the old state information:
			SystemPowerStateDestroy (pOldSystemPowerState);
			while (pOldCeilingDx != NULL)
			{
				PDEVICE_POWER_RESTRICTION pdpr = pOldCeilingDx->pNext;

				PowerRestrictionDestroy (pOldCeilingDx);
				pOldCeilingDx = pdpr;
			}

			// Are we suspending?
			if (fSuspendSystem)
			{
				// Set a flag to notify the resume thread that this was a controlled suspend.
				gfSystemSuspended = TRUE;

				PMLOGMSG (ZONE_PLATFORM
						  || ZONE_RESUME, (_T ("%s: calling PowerOffSystem()\r\n"), pszFname));
				PowerOffSystem ();	    // Sets a flag in the kernel for the scheduler.
				Sleep (0);	            // Force the scheduler to run.
				PMLOGMSG (ZONE_PLATFORM
						  || ZONE_RESUME, (_T ("%s: back from PowerOffSystem()\r\n"), pszFname));

				// Clear the suspend flag:
				gfSystemSuspended = FALSE;
			}
		}
		else
		{
			// Release the unused new state information:
			SystemPowerStateDestroy (pNewSystemPowerState);
			while (pNewCeilingDx != NULL)
			{
				PDEVICE_POWER_RESTRICTION pdpr = pNewCeilingDx->pNext;

				PowerRestrictionDestroy (pNewCeilingDx);
				pNewCeilingDx = pdpr;
			}
		}
	}

	// Restore our priority if we updated it during a suspend transition:
	if (giSuspendPriority != 0 && iPreSuspendPriority != 0)
	{
		PMLOGMSG (ZONE_PLATFORM, (_T ("%s: restoring thread priority to %d\r\n"),
								  pszFname, iPreSuspendPriority));
		CeSetThreadPriority (GetCurrentThread (), iPreSuspendPriority);
	}

	return dwStatus;
}

///////////////////////////////////////////////////////////////////////////////
//
// RegReadStateTimeout
//
// Reads a power state timeout value from the registry (where the timeout
// value should be expressed in seconds -- this function converts it into
// milliseconds on return).
//
// Arguments:
//
//   hk               Handle to an open registry key.
//
//   pszName          The name of the subkey of the hk parameter for which 
//                    the default value is retrieved.
//
//   dwDefault        Default value to return (in milliseconds) if this
//                    function is unable to read the power state timeout
//                    value from the registry.
//
// Returns:           The timeout value expressed in milliseconds.
//
///////////////////////////////////////////////////////////////////////////////

DWORD
RegReadStateTimeout (HKEY hk, LPCTSTR pszName, DWORD dwDefault)
{
    DWORD dwValue, dwSize, dwStatus;

    dwSize = sizeof (dwValue);
    dwStatus = RegQueryTypedValue (hk, pszName, &dwValue, &dwSize, REG_DWORD);
    if (dwStatus != ERROR_SUCCESS)
    {
        dwValue = dwDefault;
    }
    else
    {
        // Translate zero to infinite timeout. If the timeout is not
        // zero, enforce a ceiling for the timeout value.

        if (dwValue == 0)
        {
            dwValue = INFINITE;
        }
        else
        {
            if (dwValue > MAXACTIVITYTIMEOUT)
            {
                dwValue = MAXACTIVITYTIMEOUT;
            }
        }
    }

    // Convert to milliseconds:
    if (dwValue != INFINITE)
    {
        dwValue *= 1000;
    }
    return dwValue;
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformManageSystemPower
//
// Implements the main power manager event loop. This loop implements
// the power manager's response to external events, such as being docked
// in a cradle, running low on battery power, going on or off AC power,
// etc.  This sample implementation simply monitors battery level
// changes and generates the appropriate notifications.
//
// Arguments:
//
//   hevReady   Handle to the event object to be signaled when
//              the power manager event loop thread is started.
//
///////////////////////////////////////////////////////////////////////////////

EXTERN_C VOID WINAPI
PlatformManageSystemPower (HANDLE hevReady)
{
	BOOL fDone = FALSE;
	HANDLE hqNotify = NULL;
	HMODULE hmCoreDll = NULL;

	SETFNAME (_T ("PlatformManageSystemPower"));

	PMLOGMSG (ZONE_INIT || ZONE_PLATFORM, (_T ("+%s\r\n"), pszFname));

	// Initialize globals:
	ghevReloadActivityTimeouts = NULL;
	ghevRestartTimers = NULL;

	// Determine thread priority settings while we're suspending (in case
	// of priority inversion):

	if (!GetPMThreadPriority (_T ("PreSuspendPriority256"), &giPreSuspendPriority))
	{
		giPreSuspendPriority = DEF_PRESUSPEND_THREAD_PRIORITY;
	}
	if (!GetPMThreadPriority (_T ("SuspendPriority256"), &giSuspendPriority))
	{
		giSuspendPriority = DEF_SUSPEND_THREAD_PRIORITY;
	}

	// Get pointers to GWES's suspend/routine APIs.  These require GWES, so the OEM may
	// not have them on this platform.  Also get battery level APIs, which require a
	// battery driver and may not be present.

	hmCoreDll = (HMODULE) LoadLibrary (_T ("coredll.dll"));
	gfGwesReady = FALSE;
	PmInitPowerStatus (hmCoreDll);
	if (hmCoreDll != NULL)
	{
		gpfnGwesPowerDown = (PFN_GwesPowerDown) GetProcAddress (hmCoreDll, _T ("GwesPowerDown"));
		gpfnGwesPowerUp = (PFN_GwesPowerUp) GetProcAddress (hmCoreDll, _T ("GwesPowerUp"));

		// Do we have both GWES suspend/resume APIs?
		if (gpfnGwesPowerDown == NULL || gpfnGwesPowerUp == NULL)
		{
			// No, ignore GWES.
			gpfnGwesPowerDown = NULL;
			gpfnGwesPowerUp = NULL;
		}
	}

	// Create events:
	ghevReloadActivityTimeouts =
		CreateEvent (NULL, FALSE, FALSE, _T ("PowerManager/ReloadActivityTimeouts"));
	ghevRestartTimers = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (ghevReloadActivityTimeouts == NULL || ghevRestartTimers == NULL)
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s: CreateEvent() failed for system event\r\n"), pszFname));
		goto done;
	}

	// Create our notification queue:
	hqNotify = PmPolicyCreateNotificationQueue ();
	if (hqNotify == NULL)
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s: PmPolicyCreateNotificationQueue() failed\r\n"), pszFname));
		goto done;
	}

	if (!fDone)
	{
        // Instantiate the PowerStateManager object and call its Init method:

		PowerStateManager *pPowerStateManager = new PowerStateManager ();

		if (pPowerStateManager && pPowerStateManager->Init ())
		{
			g_pPowerStateManager = pPowerStateManager;

			// We're up and running:
			SetEvent (hevReady);
			g_pPowerStateManager->ThreadRun ();
		}
		else
		{
            // Power Manager initialization failed:
			ASSERT (FALSE);
			if (pPowerStateManager)
				delete pPowerStateManager;

			PMLOGMSG (ZONE_INIT || ZONE_ERROR,
					  (_T ("%s: PowerStateManager Intialization Failed!!!\r\n"), pszFname));
		}
		if (g_pPowerStateManager)
		{
			delete g_pPowerStateManager;
			g_pPowerStateManager = NULL;
		}
	}

  done:
	// Clean up before exiting:
	if (ghevReloadActivityTimeouts == NULL)
	{
		CloseHandle (ghevReloadActivityTimeouts);
		ghevReloadActivityTimeouts = NULL;
	}
	if (ghevRestartTimers != NULL)
	{
		CloseHandle (ghevRestartTimers);
		ghevRestartTimers = NULL;
	}
	if (hqNotify != NULL)
	{
		PmPolicyCloseNotificationQueue (hqNotify);
		hqNotify = NULL;
	}

	if (hmCoreDll != NULL)
		FreeLibrary (hmCoreDll);

	PMLOGMSG (ZONE_PLATFORM, (_T ("-%s: exiting\r\n"), pszFname));
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformResumeSystem
//
// Called when the system has resumed.  This function is responsible for
// determining why the system woke up and initiating the appropriate
// system power state transition.  This function is invoked in the
// context of the Power Manager's resume thread.  
//
// NOTE: this thread must be running at a higher priority than any
// thread that might suspend the system using SetSystemPowerState().
// Otherwise, it cannot tell whether it needs to update the system power
// state on its own.  In general, OEMs should not suspend the system
// without calling SetSystemPowerState() -- the code in this routine
// that handles unexpected suspends is present only as a fallback.
//
///////////////////////////////////////////////////////////////////////////////

EXTERN_C VOID WINAPI
PlatformResumeSystem (void)
{
	SETFNAME (_T ("PlatformResumeSystem"));

	PMLOGMSG (ZONE_RESUME, (_T ("+%s: suspend flag is %d\r\n"), pszFname, gfSystemSuspended));

	// Was this an unexpected resume event?  If so, there may be a thread priority problem
	// or some piece of software suspended the system without calling SetSystemPowerState().

	DEBUGCHK (gfSystemSuspended);
	if (!gfSystemSuspended)
	{
		// Unexpected resume -- go to the resuming state.  This should not happen unless
		// somebody is illegally calling PowerOffSystem() directly.

		PMLOGMSG (ZONE_WARN || ZONE_RESUME, (_T ("%s: WARNING: unexpected resume!\r\n"), pszFname));

		// Go into the new state.  OEMs that choose to support unexpected resumes may want to
		// lock PM variables with PMLOCK(), then set the curDx and actualDx values for all
		// devices to PwrDeviceUnspecified before calling PmSetSystemPowerState_I().  This will
		// force an update IOCTL to all devices.

		DEBUGCHK (ghevRestartTimers != NULL);
		SetEvent (ghevRestartTimers);
	}
	else
	{
		DWORD dwWakeSource, dwBytesReturned;
		BOOL fOk;

		// Get the system wake source to help determine which power state we resume into:
		fOk = KernelIoControl (IOCTL_HAL_GET_WAKE_SOURCE, NULL, 0, &dwWakeSource,
							   sizeof (dwWakeSource), &dwBytesReturned);
		if (fOk)
		{
			// IOCTL succeeded (not all platforms necessarily support it), but sanity check
			// the return value, just in case.

			if (dwBytesReturned != sizeof (dwWakeSource))
			{
				PMLOGMSG (ZONE_WARN, (_T ("%s: KernelIoControl() returned an invalid size %d\r\n"),
									  pszFname, dwBytesReturned));
			}
			else
			{
				// Look for an activity timer corresponding to this wake source.
				PACTIVITY_TIMER pat = ActivityTimerFindByWakeSource (dwWakeSource);

				if (pat != NULL)
				{
					PMLOGMSG (ZONE_RESUME, (_T ("%s: signaling '%s' activity at resume\r\n"),
											pszFname, pat->pszName));

					// Is there an activity timer we need to reset?
					if (pat->hevReset != NULL)
					{
						// Found a timer, elevate the timer management priority thread so that it
						// executes before the suspending thread.

						DWORD dwOldPriority = CeGetThreadPriority (ghtActivityTimers);
						DWORD dwNewPriority = (CeGetThreadPriority (GetCurrentThread ()) - 1);

						DEBUGCHK (dwNewPriority >= 0);
						SetEvent (pat->hevReset);
						CeSetThreadPriority (ghtActivityTimers, dwNewPriority);
						CeSetThreadPriority (ghtActivityTimers, dwOldPriority);
					}
				}
			}
		}
	}
	PMLOGMSG (ZONE_RESUME, (_T ("-%s\r\n"), pszFname));
}

///////////////////////////////////////////////////////////////////////////////
//
// PlatformSendSystemPowerState
//
// Calls the Power Manager's SendSystemPowerState function to set the 
// system power state.
//
// Arguments:
//
//  pwsState            The name of the system power state to set.
// 
//  dwStateHint         The power state hint value (such as POWER_STATE_ON, 
//                      POWER_STATE_SUSPEND).
//
//  dwOptions           Uses the optional POWER_STATE flag to indicate that the
//                      system power state transfer is urgent.
//
// Returns:
//
//  ERROR_SUCCESS       The system power state has been set successfully.
//
//  ERROR_GEN_FAILURE   The Power State Manager is not initialized.
//
//  Error code          The system power state was not set.
//
///////////////////////////////////////////////////////////////////////////////

DWORD WINAPI
PlatformSendSystemPowerState (LPCWSTR pwsState, DWORD dwStateHint, DWORD dwOptions)
{
    DWORD dwReturn = ERROR_GEN_FAILURE;

    if (g_pPowerStateManager)
    {
        dwReturn = g_pPowerStateManager->SendSystemPowerState (pwsState, dwStateHint, dwOptions);
    }
    return dwReturn;
}
