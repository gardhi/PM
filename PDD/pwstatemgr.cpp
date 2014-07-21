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
#include <pmexthdl.hpp>
#include "pwstates.h"
#include "pwstatemgr.h"

#define PM_SHUTDOWN_EVENT                   0
#define PM_RELOAD_ACTIVITY_TIMEOUTS_EVENT   1
#define PM_MSGQUEUE_EVENT                   2
#define PM_RESTART_TIMER_EVENT              3
#define PM_SYSTEM_TIMEIDLE_RESET            4
#define PM_USER_ACTIVITY_EVENT              5
#define PM_SYSTEM_API_EVENT                 6
#define PM_BOOTPHASE2_EVENT                 7   // This has to be last one.

#define PM_BASE_TOTAL_EVENT                 8

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateManager
//
// The PowerStateManager class contains the main Power Manager event loop. This 
// loop implements Power Manager's response to external events such as being 
// docked in a cradle, running low on battery power, or going on or off AC power.
//
// PowerStateManager also manages system power state objects; it instantiates the
// linked list of system power state objects that Power Manager uses for system
// power state transitions.
//
//////////////////////////////////////////////////////////////////////////////

extern HANDLE ghevPmShutdown;
extern HANDLE ghevReloadActivityTimeouts;
extern HANDLE ghevRestartTimers;
extern DWORD RegReadStateTimeout(HKEY hk, LPCTSTR pszName, DWORD dwDefault);
// need "C" linkage for compatibility with C language PDD implementations:
extern "C" extern POWER_BROADCAST_POWER_INFO gSystemPowerStatus;



PowerStateManager::PowerStateManager ()
{
	// Create events
	m_hevBootPhase2 = OpenEvent (EVENT_ALL_ACCESS, FALSE, _T ("SYSTEM/BootPhase2"));

	m_hevReloadActivityTimeouts =
		CreateEvent (NULL, FALSE, FALSE, _T ("PowerManager/ReloadActivityTimeouts"));
	m_hevRestartTimers = CreateEvent (NULL, FALSE, FALSE, NULL);
	m_hevSystemIdleTimerReset =
		CreateEvent (NULL, FALSE, FALSE, _T ("PowerManager/SystemIdleTimerReset"));
	m_hqNotify = PmPolicyCreateNotificationQueue ();
	// Using Global Event
	m_hevPmShutdown = ghevPmShutdown;

	m_pPowerStateList = NULL;

	m_dwUnattendedModeRef = 0;
	m_pLegacySPScreenOff = NULL;
	m_pLegacyBacklightOff = NULL;
}

PowerStateManager::~PowerStateManager ()
{
	while (m_pPowerStateList != NULL)
	{
		PowerState *pNextState = m_pPowerStateList->GetNextPowerState ();
		delete m_pPowerStateList;

		m_pPowerStateList = pNextState;
	}
	if (m_hevReloadActivityTimeouts)
		CloseHandle (m_hevReloadActivityTimeouts);
	if (m_hevRestartTimers)
		CloseHandle (m_hevRestartTimers);
	if (m_hevSystemIdleTimerReset)
		CloseHandle (m_hevSystemIdleTimerReset);
	if (m_hevBootPhase2)
		CloseHandle (m_hevBootPhase2);
	if (m_hqNotify)
		PmPolicyCloseNotificationQueue (m_hqNotify);
	if (m_pLegacySPScreenOff)
		delete m_pLegacySPScreenOff;

	if (m_pLegacyBacklightOff)
		delete m_pLegacyBacklightOff;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateManager CreatePowerStateList()
//
// This method instantiates the system power state classes to create
// a linked list of system power state objects. If you define a new
// system power state class, be sure to instantiate it here as part
// of this linked list.
//
//////////////////////////////////////////////////////////////////////////////

BOOL 
PowerStateManager::CreatePowerStateList()
{
    if (m_pPowerStateList == NULL ) {
        m_pPowerStateList = 
            new PowerStateOn (this, 
            new PowerStateUserIdle (this, 
            new PowerStateBacklightOff (this, 
            new PowerStateScreenOff (this, 
            new PowerStateUnattended (this, 
            new PowerStateResuming (this, 
            new PowerStateSuspend (this, 
            new PowerStateColdReboot (this, 
            new PowerStateReboot (this,
            new PowerStateOff (this))))))))));
    }
    if (m_pPowerStateList != NULL) {
        PowerState * pCurState = m_pPowerStateList;
        while (pCurState) {
            if (!pCurState->Init()) {
                ASSERT(FALSE);
                return FALSE;
            }
            pCurState = pCurState->GetNextPowerState();
        }
        return TRUE;
    }
    else
        return FALSE;
}

BOOL
PowerStateManager::ReInitLegacyRegistry ()
{
	if (m_pLegacySPScreenOff != NULL)
		delete m_pLegacySPScreenOff;

	m_pLegacySPScreenOff = new NotifyRegKey (HKEY_CURRENT_USER, TEXT ("ControlPanel\\Power"));
	if (m_pLegacySPScreenOff && !m_pLegacySPScreenOff->Init ())
	{  // Fails
		delete m_pLegacySPScreenOff;
		m_pLegacySPScreenOff = NULL;
	}

	if (m_pLegacyBacklightOff != NULL)
		delete m_pLegacyBacklightOff;

	m_pLegacyBacklightOff = new NotifyRegKey (HKEY_CURRENT_USER, TEXT ("ControlPanel\\Backlight"));
	if (m_pLegacyBacklightOff && !m_pLegacyBacklightOff->Init ())
	{  // Fails
		delete m_pLegacyBacklightOff;

		m_pLegacyBacklightOff = NULL;
	}
	return TRUE;
}

BOOL
PowerStateManager::Init ()
{

	SETFNAME (_T ("PowerStateManager::Init"));
	if (ghevReloadActivityTimeouts == NULL || ghevRestartTimers == NULL || m_hqNotify == NULL)
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s: CreateEvent() failed for system event\r\n"), pszFname));
		return FALSE;
	}
	if (!PMSystemAPI::Init ())
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s:  PMSystemAPI::Init() failed\r\n"), pszFname));
		return FALSE;
	}
	m_pUserActivity = ActivityTimerFindByName (_T ("UserActivity"));

	// Check that all of our activity events exist:
	if (m_pUserActivity == NULL || m_pUserActivity->hevActive == NULL
		|| m_pUserActivity->hevInactive == NULL)
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s: UserActivity timer events not found\r\n"), pszFname));
		return FALSE;
	}
	if (m_hqNotify == NULL)
	{
		PMLOGMSG (ZONE_WARN, (_T ("%s: PmPolicyCreateNotificationQueue() failed\r\n"), pszFname));
		return FALSE;
	}

	ReInitLegacyRegistry ();
	PlatformLoadTimeouts ();
	ReInitTimeOuts (TRUE);
	return CreatePowerStateList ();
}

void
PowerStateManager::ReInitTimeOuts (BOOL fIgnoreSuspendResume)
{
	m_dwCurResumingSuspendTimeout =
		(fIgnoreSuspendResume ? INFINITE : GetResumingSuspendTimeout ());
	m_dwCurSuspendTimeout = GetSuspendTimeOut ();
	m_dwCurBacklightTimeout = GetBackLightTimeout ();
	m_dwCurUserIdleTimeout = GetUserIdleTimeOut ();

	// If timer is not set, then it is not supported.
	if (m_dwCurBacklightTimeout == 0)
		m_dwCurBacklightTimeout = INFINITE;
	if (m_dwCurUserIdleTimeout == 0)
		m_dwCurUserIdleTimeout = INFINITE;
	if (m_dwCurSuspendTimeout == 0)
		m_dwCurSuspendTimeout = INFINITE;
}

void
PowerStateManager::ReAdjustTimeOuts ()
{
	if (m_dwCurBacklightTimeout > GetBackLightTimeout ())
	{
		if ((m_dwCurBacklightTimeout = GetBackLightTimeout ()) == 0)
			m_dwCurBacklightTimeout = INFINITE;
	}
	if (m_dwCurSuspendTimeout > GetSuspendTimeOut ())
	{
		if ((m_dwCurSuspendTimeout = GetSuspendTimeOut ()) == 0)
			m_dwCurSuspendTimeout = INFINITE;
	}
	if (m_dwCurUserIdleTimeout > GetUserIdleTimeOut ())
	{
		if ((m_dwCurUserIdleTimeout = GetUserIdleTimeOut ()) == 0)
			m_dwCurUserIdleTimeout = INFINITE;
	}
}

void
PowerStateManager::ResetUserIdleTimeout (BOOL fIdle)
{
	if (fIdle)
	{
		m_dwCurBacklightTimeout = GetBackLightTimeout ();
		m_dwCurUserIdleTimeout = GetUserIdleTimeOut ();
	}
	else
	{
		m_dwCurBacklightTimeout = INFINITE;
		m_dwCurUserIdleTimeout = INFINITE;
	}
	if (m_dwCurBacklightTimeout == 0)
		m_dwCurBacklightTimeout = INFINITE;
	if (m_dwCurUserIdleTimeout == 0)
		m_dwCurUserIdleTimeout = INFINITE;
}

void
PowerStateManager::ResetSystemIdleTimeTimeout (BOOL fIdle)
{
	m_dwCurSuspendTimeout = (fIdle ? GetSuspendTimeOut () : INFINITE);
	if (m_dwCurSuspendTimeout == 0)
		m_dwCurSuspendTimeout = INFINITE;
}

void
PowerStateManager::SubtractTimeout (DWORD dwTicks)
{
	if (m_dwCurSuspendTimeout != INFINITE)
		m_dwCurSuspendTimeout =
			(m_dwCurSuspendTimeout > dwTicks ? m_dwCurSuspendTimeout - dwTicks : 0);
	if (m_dwCurResumingSuspendTimeout != INFINITE)
		m_dwCurResumingSuspendTimeout =
			(m_dwCurResumingSuspendTimeout > dwTicks ? m_dwCurResumingSuspendTimeout - dwTicks : 0);
	if (m_dwCurBacklightTimeout != INFINITE)
		m_dwCurBacklightTimeout =
			(m_dwCurBacklightTimeout > dwTicks ? m_dwCurBacklightTimeout - dwTicks : 0);
	if (m_dwCurUserIdleTimeout != INFINITE)
		m_dwCurUserIdleTimeout =
			(m_dwCurUserIdleTimeout > dwTicks ? m_dwCurUserIdleTimeout - dwTicks : 0);
}

DWORD
PowerStateManager::GetSmallestTimeout (PTIMEOUT_ITEM pTimeoutItem)
{
	DWORD dwReturn = INFINITE;
	TIMEOUT_ITEM activeEvent = NoTimeoutItem;

	if (dwReturn > m_dwCurSuspendTimeout)
	{
		dwReturn = m_dwCurSuspendTimeout;
		activeEvent = SuspendTimeout;
	}
	if (dwReturn > m_dwCurResumingSuspendTimeout)
	{
		dwReturn = m_dwCurResumingSuspendTimeout;
		activeEvent = ResumingSuspendTimeout;
	}
	if (dwReturn > m_dwCurBacklightTimeout)
	{
		dwReturn = m_dwCurBacklightTimeout;
		activeEvent = BacklightTimeout;
	}
	if (dwReturn > m_dwCurUserIdleTimeout)
	{
		dwReturn = m_dwCurUserIdleTimeout;
		activeEvent = UserActivityTimeout;
	}
	if (pTimeoutItem)
	{
		*pTimeoutItem = activeEvent;
	}
	return dwReturn;
}

LPCTSTR
PowerStateManager::ActivityStateToSystemState (PLATFORM_ACTIVITY_STATE platActiveState)
{
	PowerState *curState = m_pPowerStateList;

	while (curState)
	{
		if (curState->GetState () == platActiveState)
			return curState->GetStateString ();
		else
			curState = curState->GetNextPowerState ();
	}
	return NULL;
}

PLATFORM_ACTIVITY_STATE
PowerStateManager::SystemStateToActivityState (LPCTSTR lpState)
{
	if (lpState == NULL)
		return UnknownState;
	PowerState *curState = m_pPowerStateList;

	while (curState)
	{
		if (_tcsicmp (curState->GetStateString (), lpState) == 0)
			return curState->GetState ();
		else
			curState = curState->GetNextPowerState ();
	}
	return UnknownState;
}

HANDLE
PowerStateManager::GetEventHandle (DWORD dwIndex)
{
	switch (dwIndex)
	{
	  case PM_SHUTDOWN_EVENT:
		  return ghevPmShutdown;
	  case PM_RELOAD_ACTIVITY_TIMEOUTS_EVENT:
		  return ghevReloadActivityTimeouts;
	  case PM_MSGQUEUE_EVENT:
		  return m_hqNotify;
	  case PM_RESTART_TIMER_EVENT:
		  return ghevRestartTimers;
	  case PM_SYSTEM_API_EVENT:
		  return GetAPISignalHandle ();
	  case PM_BOOTPHASE2_EVENT:
		  return m_hevBootPhase2;
	}
	return NULL;
}

void
PowerStateManager::DisablePhase2Event ()
{
	if (m_hevBootPhase2)
	{
		CloseHandle (m_hevBootPhase2);
		m_hevBootPhase2 = NULL;
	}
}

void
PowerStateManager::PlatformLoadTimeouts ()
{
	DWORD dwStatus;
	TCHAR szPath[MAX_PATH];
	HKEY hk;

	SETFNAME (_T ("PowerStateManager::PlatformLoadTimeouts"));

	// Assume default values:
	m_dwACSuspendTimeout = DEF_ACSUSPENDTIMEOUT * 1000;
	m_dwACResumingSuspendTimeout = DEF_ACRESUMINGSUSPENDTIMEOUT * 1000;
	m_dwACBacklightTimeout = DEF_ACBACKLIGHTTIMEOUT * 1000;
	m_dwACUserIdleTimeout = DEF_ACUSERIDLETIMEOUT * 1000;

	m_dwBattSuspendTimeout = DEF_BATTSUSPENDTIMEOUT * 1000;
	m_dwBattResumingSuspendTimeout = DEF_BATTRESUMINGSUSPENDTIMEOUT * 1000;
	m_dwBattBacklightTimeout = DEF_BATTBACKLIGHTTIMEOUT * 1000;
	m_dwBattUserIdleTimeout = DEF_BATTUSERIDLETIMEOUT * 1000;

	// Get timeout thresholds for transitions between states:
	StringCchPrintf (szPath, MAX_PATH, _T ("%s\\%s"), PWRMGR_REG_KEY, _T ("Timeouts"));
	dwStatus = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szPath, 0, 0, &hk);
	if (dwStatus == ERROR_SUCCESS)
	{
		// Read system power state timeouts:
		m_dwACSuspendTimeout =
			RegReadStateTimeout (hk, _T ("ACSuspendTimeout"), DEF_ACSUSPENDTIMEOUT);
		m_dwACResumingSuspendTimeout =
			RegReadStateTimeout (hk, _T ("ACResumingSuspendTimeout"), DEF_ACRESUMINGSUSPENDTIMEOUT);
		m_dwACBacklightTimeout =
			RegReadStateTimeout (hk, _T ("ACBacklightTimeout"), DEF_ACBACKLIGHTTIMEOUT);
		m_dwACUserIdleTimeout = RegReadStateTimeout (hk, _T ("ACUserIdle"), DEF_ACUSERIDLETIMEOUT);

		m_dwBattSuspendTimeout =
			RegReadStateTimeout (hk, _T ("BattSuspendTimeout"), DEF_BATTSUSPENDTIMEOUT);
		m_dwBattResumingSuspendTimeout =
			RegReadStateTimeout (hk, _T ("BattResumingSuspendTimeout"),
								 DEF_BATTRESUMINGSUSPENDTIMEOUT);
		m_dwBattBacklightTimeout =
			RegReadStateTimeout (hk, _T ("BattBacklightTimeout"), DEF_BATTBACKLIGHTTIMEOUT);
		m_dwBattUserIdleTimeout =
			RegReadStateTimeout (hk, _T ("BattUserIdle"), DEF_BATTUSERIDLETIMEOUT);

		// Release resources:
		RegCloseKey (hk);
	}

	// Update Legacy if we find them.
	DWORD dwValue;

	if (m_pLegacySPScreenOff)
	{
		m_pLegacySPScreenOff->AckNotification ();
		if (m_pLegacySPScreenOff->GetRegValue (_T ("Display"), &dwValue))
		{	// Found SmartPhone Legacy Value. Use it.
			m_dwACUserIdleTimeout = m_dwBattUserIdleTimeout = dwValue;
		}
	}

	if (m_pLegacyBacklightOff)
	{
		m_pLegacyBacklightOff->AckNotification ();
		if (m_pLegacyBacklightOff->GetRegValue (_T ("BatteryTimeout"), &dwValue))
		{	// Legacy Backlight Off Registry value.
			m_dwBattBacklightTimeout = dwValue;
		}
		else if (m_pLegacyBacklightOff->GetRegValue (_T ("OldBatteryTimeout"), &dwValue))
		{
			m_dwBattBacklightTimeout = INFINITE;
			m_dwBattUserIdleTimeout = INFINITE;
		}

		if (m_pLegacyBacklightOff->GetRegValue (_T ("ACTimeout"), &dwValue))
		{	// Legacy Backlight Off Registry value.
			m_dwACBacklightTimeout = dwValue;
		}
		else if (m_pLegacyBacklightOff->GetRegValue (_T ("OldACTimeout"), &dwValue))
		{
			m_dwACBacklightTimeout = INFINITE;
			m_dwACUserIdleTimeout = INFINITE;
		}
	}

	PMLOGMSG (ZONE_INIT || ZONE_PLATFORM,
			  (_T
			   ("%s: ACSuspendTimeout %d, ACResumingSuspendTimeout %d,  ACBacklightTimeout %d, ACUserIdleTimeout %d \r\n"),
			   pszFname, m_dwACSuspendTimeout, m_dwACResumingSuspendTimeout, m_dwACBacklightTimeout,
			   m_dwACUserIdleTimeout));
	PMLOGMSG (ZONE_INIT
			  || ZONE_PLATFORM,
			  (_T
			   ("%s: BattSuspendTimeout %d, BattResumingSuspendTimeout %d , BattBacklightTimeout %d, BattUserIdleTimeout%d \r\n"),
			   pszFname, m_dwBattSuspendTimeout, m_dwBattResumingSuspendTimeout,
			   m_dwBattBacklightTimeout, m_dwBattUserIdleTimeout));
}

DWORD
PowerStateManager::GetSuspendTimeOut ()
{
	return (gSystemPowerStatus.bACLineStatus ==
			AC_LINE_OFFLINE ? m_dwBattSuspendTimeout : m_dwACSuspendTimeout);
}

DWORD
PowerStateManager::GetResumingSuspendTimeout ()
{
	return (gSystemPowerStatus.bACLineStatus ==
			AC_LINE_OFFLINE ? m_dwBattResumingSuspendTimeout : m_dwACResumingSuspendTimeout);
}

DWORD
PowerStateManager::GetBackLightTimeout ()
{
	return (gSystemPowerStatus.bACLineStatus ==
			AC_LINE_OFFLINE ? m_dwBattBacklightTimeout : m_dwACBacklightTimeout);
}

DWORD
PowerStateManager::GetUserIdleTimeOut ()
{
	return (gSystemPowerStatus.bACLineStatus ==
			AC_LINE_OFFLINE ? m_dwBattUserIdleTimeout : m_dwACUserIdleTimeout);
}

PowerState *
PowerStateManager::GetStateObject (PLATFORM_ACTIVITY_STATE newState)
{
	if (m_pPowerStateList != NULL)
	{
		PowerState *pCurState = m_pPowerStateList;

		while (pCurState)
		{
			if (pCurState->GetState () == newState)
			{
				return pCurState;
			}
			pCurState = pCurState->GetNextPowerState ();
		}
	}
	return NULL;
}

PowerState *
PowerStateManager::GetFirstPowerState ()
{
	SETFNAME (_T ("PowerStateManager::GetFirstPowerState"));
	PowerState *curState = NULL;
	HANDLE hEvents[2];

	hEvents[0] = ghevPmShutdown;
	hEvents[1] = GetAPISignalHandle ();
	DWORD dwStatus = WaitForMultipleObjects (2, hEvents, FALSE, INFINITE);

	switch (dwStatus)
	{
	  case (WAIT_OBJECT_0 + 0):
		  PMLOGMSG (ZONE_INIT
					|| ZONE_WARN, (_T ("%s: shutdown event signaled, exiting\r\n"), pszFname));
		  break;
	  case (WAIT_OBJECT_0 + 1):
		  {
			  PMLOGMSG (ZONE_INIT, (_T ("%s: initialization complete\r\n"), pszFname));
			  PLATFORM_ACTIVITY_STATE apiState = RequestedSystemPowerState ();

			  curState = m_pPowerStateList;
			  while (curState)
			  {
				  if (curState->GetState () == apiState)
					  break;
				  else
					  curState = curState->GetNextPowerState ();
			  }
			  RequestComplete ();
		  }
		  break;
	  default:
		  PMLOGMSG (ZONE_INIT
					|| ZONE_WARN, (_T ("%s: WaitForMultipleObjects() returned %d, exiting\r\n"),
								   pszFname, dwStatus));
		  break;
	}
	return curState;
}

DWORD
PowerStateManager::ThreadRun ()
{
	SETFNAME (_T ("PowerStateManager::ThreadRun"));

	// Assume that the first state is PowerStateOn. 
	if (m_pPowerStateList)
	{
		// Get first SetSystemPower from the device to make the initial power state correct.
		PowerState *pCurPowerState = GetFirstPowerState ();

		if (pCurPowerState != NULL)
		{
			pCurPowerState->EnterState ();
			BOOL fDone = FALSE;

			// Create Legacy Registry modify notification event array.
			while (!fDone && pCurPowerState)
			{
				HANDLE hEvents[MAXIMUM_WAIT_OBJECTS];
				DWORD dwNumOfLegacyEvent = 0;

				if (m_pLegacySPScreenOff)
				{
					hEvents[dwNumOfLegacyEvent] = m_pLegacySPScreenOff->GetNotificationHandle ();
					dwNumOfLegacyEvent++;
				}
				if (m_pLegacyBacklightOff)
				{
					hEvents[dwNumOfLegacyEvent] = m_pLegacyBacklightOff->GetNotificationHandle ();
					dwNumOfLegacyEvent++;
				}
				DWORD dwNumOfExtEvent = MAXIMUM_WAIT_OBJECTS - dwNumOfLegacyEvent;

				if (!PMExt_GetNotificationHandle (dwNumOfExtEvent, hEvents + dwNumOfLegacyEvent))
					dwNumOfExtEvent = 0;

				ASSERT (dwNumOfExtEvent + dwNumOfLegacyEvent < MAXIMUM_WAIT_OBJECTS);

				PLATFORM_ACTIVITY_EVENT activityEvent =
					pCurPowerState->WaitForEvent (INFINITE, dwNumOfExtEvent + dwNumOfLegacyEvent,
												  hEvents);
				PMLOGMSG (ZONE_PLATFORM,
						  (_T ("%s: activityEvent = %d  \r\n"), pszFname, activityEvent));
				if (activityEvent >= ExternedEvent
					&& activityEvent <
					(PLATFORM_ACTIVITY_EVENT) (ExternedEvent + dwNumOfLegacyEvent))
				{	// Legacy registry Event.
					PlatformLoadTimeouts ();	// No break we need run ReInitTimeouts.
					ReInitTimeOuts (TRUE);
					pCurPowerState->DefaultEventHandle (RestartTimeouts);
				}
				else if (activityEvent >=
						 (PLATFORM_ACTIVITY_EVENT) (ExternedEvent + dwNumOfLegacyEvent)
						 && activityEvent <
						 (PLATFORM_ACTIVITY_EVENT) (ExternedEvent + dwNumOfLegacyEvent +
													dwNumOfExtEvent))
				{
					PMExt_EventNotification ((PLATFORM_ACTIVITY_EVENT)
											 (activityEvent - ExternedEvent - dwNumOfLegacyEvent +
											  PowerManagerExt));
				}
				else
				{
					switch (activityEvent)
					{
					  case PmShutDown:
						  fDone = TRUE;
						  break;
					  case BootPhaseChanged:
						  ReInitLegacyRegistry ();	// No break because we need do following as well.
					  case PmReloadActivityTimeouts:
						  PlatformLoadTimeouts ();	// No break we need run ReInitTimeouts.
					  case RestartTimeouts:
					  case PowerSourceChange:
					  case SystemPowerStateChange:
					  case PowerButtonPressed:
					  case AppButtonPressed:
						  ReInitTimeOuts (TRUE);
						  pCurPowerState->DefaultEventHandle (activityEvent);
						  break;
					  case SystemPowerStateAPI:
						  {
							  PLATFORM_ACTIVITY_STATE apiState = RequestedSystemPowerState ();
							  PowerState *pNewPowerState = GetStateObject (apiState);

							  if (pNewPowerState && !pNewPowerState->AppsCanRequestState ())
							  {
								  RequestComplete (ERROR_INVALID_PARAMETER);
								  activityEvent = NoActivity;
							  }
							  else
							  {
								  pCurPowerState->SetSystemAPIState (apiState);
							  }
							  break;
						  }
					  case EnterUnattendedModeRequest:
						  IncUnattendedRefCount ();
						  pCurPowerState->DefaultEventHandle (EnterUnattendedModeRequest);
						  break;
					  case LeaveUnattendedModeRequest:
						  DecUnattendedRefCount ();
						  pCurPowerState->DefaultEventHandle (LeaveUnattendedModeRequest);
						  break;
					  default:
						  pCurPowerState->DefaultEventHandle (activityEvent);
						  break;
					}
					PMExt_EventNotification (activityEvent);
				}
				pCurPowerState = SetSystemState (pCurPowerState);
				ASSERT (pCurPowerState != NULL);
				if (activityEvent == SystemPowerStateAPI)
				{
					RequestComplete ();
				}
			}
		}
		else
			ASSERT (FALSE);
	}
	return 0;
}

PowerState *
PowerStateManager::SetSystemState (PowerState * pCurPowerState)
{
	SETFNAME (_T ("PowerStateManager::SetSystemState"));
	if (pCurPowerState != NULL)
	{
		PLATFORM_ACTIVITY_STATE curState = pCurPowerState->GetState ();
		PLATFORM_ACTIVITY_STATE newState = curState;

		do
		{	// Switch to new stable state.
			newState = pCurPowerState->GetLastNewState ();
			if (newState != curState)
			{
				PowerState *pNewPowerState = GetStateObject (newState);

				if (pNewPowerState == NULL)
				{	// Wrong state or unsupported state.
					PMLOGMSG (ZONE_WARN
							  || ZONE_PLATFORM, (_T ("Unsupported state %d \r\n"), newState));
					ASSERT (FALSE);
					newState = curState;
				}
				else if (pNewPowerState != pCurPowerState)
				{
					PMLOGMSG (ZONE_INIT
							  || ZONE_PLATFORM, (_T ("%s: state change from \"%s\" to \"%s\" \r\n"),
												 pszFname, pCurPowerState->GetStateString (),
												 pNewPowerState->GetStateString ()));
					pCurPowerState = pNewPowerState;
					pCurPowerState->EnterState ();

					// Update to new state:
					curState = newState;
					newState = pCurPowerState->GetLastNewState ();
				}
				else
					newState = curState;
			}
		} while (newState != curState);	    // Change to stable state.

		ASSERT (pCurPowerState != NULL);
	}
	return pCurPowerState;
}

//////////////////////////////////////////////////////////////////////////////
//
// PMSystemAPI
//
// PMSystemAPI is the base class for PowerStateManager; it implements 
// the serialization logic for state transitions and event-handling code 
// for the main Power Manager event loop.
//
//////////////////////////////////////////////////////////////////////////////

// Help function.
PMSystemAPI::PMSystemAPI ()
{
	m_szStateName[0] = 0;
	m_dwStateHint = 0;
	m_dwOptions = 0;

	m_hEmpty = CreateEvent (NULL, FALSE, TRUE, NULL);
	m_hNotEmpty = CreateEvent (NULL, TRUE, FALSE, NULL);
	m_hComplete = CreateEvent (NULL, TRUE, FALSE, NULL);
}

BOOL
PMSystemAPI::Init ()
{
	return (m_hEmpty != NULL && m_hNotEmpty != NULL && m_hComplete != NULL);
}

PMSystemAPI::~PMSystemAPI ()
{
	if (m_hEmpty)
		CloseHandle (m_hEmpty);
	if (m_hNotEmpty)
		CloseHandle (m_hNotEmpty);
	if (m_hComplete)
		CloseHandle (m_hComplete);
}

PLATFORM_ACTIVITY_STATE
PMSystemAPI::RequestedSystemPowerState ()
{
	PLATFORM_ACTIVITY_STATE activeState = UnknownState;

	SETFNAME (_T ("PMSystemAPI::RequestedSystemPowerState"));
	Lock ();
	if (WaitForSingleObject (m_hNotEmpty, 0) == WAIT_OBJECT_0)
	{  // Signaled.
		DWORD dwStatus = ERROR_SUCCESS;

		PMLOGMSG (ZONE_API, (_T ("+%s: name \"%s\", hint 0x%08x, options 0x%08x, fInternal %d\r\n"),
							 pszFname, m_szStateName, m_dwStateHint, m_dwOptions));

		// If the user passes a null state name, use the hints flag to try to find a match.
		if (m_szStateName[0] == 0)
		{
			// Try to match the hint flag to a system state
			dwStatus = PlatformMapPowerStateHint (m_dwStateHint, m_szStateName, _countof (m_szStateName));
		}

		// Go ahead and do the update?
		if (dwStatus == ERROR_SUCCESS)
		{
			activeState = SystemStateToActivityState (m_szStateName);
			if (activeState == UnknownState)
			{
				dwStatus = ERROR_INVALID_PARAMETER;
			}
		}

		PMLOGMSG (ZONE_API, (_T ("-%s: returning dwStatus %d\r\n"), pszFname, dwStatus));
		m_dwResult = dwStatus;
		ResetEvent (m_hNotEmpty);
	}
	else
	{
		PMLOGMSG (ZONE_ERROR, (_T ("-%s: Fails m_hNotEmpty is not Set \r\n")));
	}
	Unlock ();
	return activeState;
}

void
PMSystemAPI::RequestComplete (DWORD dwStatus)
{
	if (ERROR_SUCCESS != dwStatus)
		m_dwResult = dwStatus;
	SetEvent (m_hComplete);
}

DWORD
PMSystemAPI::SendSystemPowerState (LPCWSTR pwsState, DWORD dwStateHint, DWORD dwOptions)
{
	DWORD dwReturn = WaitForSingleObject (m_hEmpty, INFINITE);

	SETFNAME (_T ("PMSystemAPI::SendSystemPowerState"));
	PMLOGMSG (ZONE_API, (_T ("+%s: name %s, hint 0x%08x, options 0x%08x, fInternal %d\r\n"),
						 pszFname, pwsState != NULL ? pwsState : _T ("<NULL>"), dwStateHint,
						 dwOptions));
	ASSERT (dwReturn == WAIT_OBJECT_0);
	Lock ();
	ResetEvent (m_hComplete);
	m_dwResult = ERROR_SUCCESS;
	m_szStateName[0] = 0;
	if (pwsState)
	{
		__try
		{
			StringCchCopy (m_szStateName, MAX_PATH, pwsState);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			PMLOGMSG (ZONE_WARN, (_T ("%s: exception copying state name\r\n"), pszFname));
			m_szStateName[0] = 0;
			dwReturn = ERROR_INVALID_PARAMETER;
		}
	}
	m_dwStateHint = dwStateHint;
	m_dwOptions = dwOptions;
	SetEvent (m_hNotEmpty);
	Unlock ();
	dwReturn = WaitForSingleObject (m_hComplete, INFINITE);
	ASSERT (dwReturn == WAIT_OBJECT_0);
	dwReturn = m_dwResult;
	SetEvent (m_hEmpty);

	if ((ERROR_SUCCESS == dwReturn) && ((dwOptions & POWER_DUMPDW) != 0))
	{
		CaptureDumpFileOnDevice (GetCurrentProcessId (), GetCurrentThreadId (), NULL);
	}
	return dwReturn;
}
