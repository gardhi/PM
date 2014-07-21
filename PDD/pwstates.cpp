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
// PowerState
//
// The PowerState class is the base class for all system power states. When you
// create a new system power state class, your class derives from PowerState. Your
// derived class must use an initializer list for PowerState, passing the following
// arguments to the PowerState constructor:
//
// pPwrStateMgr      Pointer to the PowerStateManager object. Note that 
//                   PowerStateManager calls your constructor (see 
//                   PowerStateManager::CreatePowerStateList in pwstatemgr.cpp), 
//                   so you can pass 'this' for the PowerStateManager object.
//
// pNextPowerState   Pointer to the next PowerState object in the linked list 
//                   of PowerState objects.
//
//////////////////////////////////////////////////////////////////////////////

PowerState::PowerState (PowerStateManager *pPwrStateMgr, PowerState * pNextPowerState)
    : m_pPwrStateMgr (pPwrStateMgr), m_pNextPowerState (pNextPowerState)
{
	memset (m_dwEventArray, 0, sizeof (m_dwEventArray));
	m_hUnsignaledHandle = CreateEvent (NULL, FALSE, FALSE, NULL);
	PREFAST_ASSERT (pPwrStateMgr != NULL);

	m_dwNumOfEvent = PM_BASE_TOTAL_EVENT;
	for (DWORD dwIndex = 0; dwIndex < PM_BASE_TOTAL_EVENT; dwIndex++)
	{
		m_dwEventArray[dwIndex] = m_hUnsignaledHandle;
	}
}

PowerState::~PowerState ()
{
	if (m_hUnsignaledHandle != NULL)
		CloseHandle (m_hUnsignaledHandle);
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerState Init() 
//
// Initializes PowerState event data structures and validates the registry
// settings for the current state.
//
//////////////////////////////////////////////////////////////////////////////

BOOL
PowerState::Init ()
{
	SETFNAME (_T ("PowerState::Init"));
	if (m_pPwrStateMgr && m_hUnsignaledHandle)
	{
		for (DWORD dwIndex = 0; dwIndex < m_dwNumOfEvent; dwIndex++)
			if (m_dwEventArray[dwIndex] == NULL)
			{
				ASSERT (FALSE);
				return FALSE;
			}
		m_LastNewState = GetState ();	// Point to itself
		DWORD dwReturn = StateValidateRegistry ();

		if (dwReturn != ERROR_SUCCESS)
		{
			PMLOGMSG (ZONE_PLATFORM,
					  (_T ("%s: StateValidateRegistry return (0x%08x) fails\r\n"), pszFname,
					   dwReturn));
			ASSERT (FALSE);
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerState MsgQueueEvent() 
//
// Reads and handles messages from the notification queue; returns
// the activity event (such as PowerSourceChange, PowerButtonPressed, etc.)
// responsible for the notification.
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerState::MsgQueueEvent ()
{
	SETFNAME (_T ("PowerState::MsgQueueEvent"));
	PLATFORM_ACTIVITY_EVENT activeEvent = NoActivity;
	POWERPOLICYMESSAGE ppm;
	DWORD dwStatus =
		PmPolicyReadNotificationQueue (m_dwEventArray[PM_MSGQUEUE_EVENT], &ppm, sizeof (ppm));

	if (dwStatus == ERROR_SUCCESS)
	{
		PMLOGMSG (ZONE_PLATFORM,
				  (_T ("%s: got request 0x%04x (data 0x%08x) from process 0x%08x\r\n"), pszFname,
				   ppm.dwMessage, ppm.dwData, ppm.hOwnerProcess));
		switch (ppm.dwMessage)
		{
		  case PPN_POWERCHANGE:
			  if (PmUpdatePowerStatus ())
			  {
				  activeEvent = PowerSourceChange;
			  }
			  break;
		  case PPN_SUSPENDKEYPRESSED:
			  activeEvent = PowerButtonPressed;
			  break;
		  case PPN_APPBUTTONPRESSED:
			  activeEvent = AppButtonPressed;
			  break;
		  case PPN_UNATTENDEDMODE:
			  // Somebody wants to enter or leave unattended mode:
			  if (ppm.dwData != FALSE)
			  {
				  activeEvent = EnterUnattendedModeRequest;
			  }
			  else
			  {
				  activeEvent = LeaveUnattendedModeRequest;
			  }
			  break;
		  default:
			  // Unhandled notification type, so ignore it:
			  PMLOGMSG (ZONE_WARN,
						(_T ("%s: unhandled policy notification 0x%04x (data 0x%08x)\r\n"),
						 pszFname, ppm.dwMessage, ppm.dwData));
			  break;
		}
	}
	PMLOGMSG (ZONE_PLATFORM, (_T ("%s: return ActiveEvent = 0x%08x\r\n"), pszFname, activeEvent));
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerState EnterState() 
//
// Default method called when Power Manager transitions to this state;
// sets timers to initial values.
//
//////////////////////////////////////////////////////////////////////////////

void
PowerState::EnterState ()
{
	PmSetSystemPowerState_I (GetStateString (), 0, 0, TRUE);
	m_LastNewState = GetState ();	// Point to itself

	// Initial timeout to Active:
	m_dwEventArray[PM_SHUTDOWN_EVENT] = m_pPwrStateMgr->GetEventHandle (PM_SHUTDOWN_EVENT);
	m_dwEventArray[PM_RELOAD_ACTIVITY_TIMEOUTS_EVENT] =
		m_pPwrStateMgr->GetEventHandle (PM_RELOAD_ACTIVITY_TIMEOUTS_EVENT);
	m_dwEventArray[PM_MSGQUEUE_EVENT] = m_pPwrStateMgr->GetEventHandle (PM_MSGQUEUE_EVENT);
	m_dwEventArray[PM_RESTART_TIMER_EVENT] =
		m_pPwrStateMgr->GetEventHandle (PM_RESTART_TIMER_EVENT);
	m_dwEventArray[PM_USER_ACTIVITY_EVENT] = m_pPwrStateMgr->GetUserActivityTimer ()->hevAutoReset;
	m_dwEventArray[PM_SYSTEM_API_EVENT] = m_pPwrStateMgr->GetEventHandle (PM_SYSTEM_API_EVENT);
	m_dwEventArray[PM_SYSTEM_TIMEIDLE_RESET] =
		m_pPwrStateMgr->GetEventHandle (PM_SYSTEM_TIMEIDLE_RESET);
	m_dwEventArray[PM_BOOTPHASE2_EVENT] = m_pPwrStateMgr->GetEventHandle (PM_BOOTPHASE2_EVENT);

	m_dwNumOfEvent = PM_BASE_TOTAL_EVENT;
	for (DWORD dwIndex = 0; dwIndex < PM_BASE_TOTAL_EVENT; dwIndex++)
	{
		if (m_dwEventArray[dwIndex] == NULL)
		{	// Use dummy event.
			m_dwEventArray[dwIndex] = m_hUnsignaledHandle;
		}
	}
	m_pPwrStateMgr->ReAdjustTimeOuts ();
}

////////////////////////////////////////////////////////////////////////////////////
//
// PowerState WaitForEvent()
//
// Default method for setting up the transition to the next system power state.
// Waits for a platform activity event to be signaled and then returns the event
// responsible for the transition.
//
////////////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerState::WaitForEvent (DWORD dwTimeouts, DWORD dwNumOfExternEvent, 
                          HANDLE * pExternEventArray)
{
	DWORD dwNumOfEvent = m_dwNumOfEvent;

	if (dwNumOfExternEvent != 0 && pExternEventArray != NULL)
	{
		for (; dwNumOfExternEvent != 0 && dwNumOfEvent < MAX_EVENT_ARRAY; dwNumOfEvent++)
			if (*pExternEventArray != NULL)
			{
				m_dwEventArray[dwNumOfEvent] = *pExternEventArray;
				pExternEventArray++;
				dwNumOfExternEvent--;
			}
			else
			{
				ASSERT (FALSE);
				return NoActivity;
			}
	}
	DWORD dwTicksElapsed = GetTickCount ();
	DWORD dwReturn =
		WaitForMultipleObjects (min (dwNumOfEvent, MAX_EVENT_ARRAY), m_dwEventArray, FALSE,
								dwTimeouts);
	dwTicksElapsed = GetTickCount () - dwTicksElapsed;

    // We don't need update time because SetSystemPowerState was called:
	if ((dwReturn - WAIT_OBJECT_0) != PM_SYSTEM_API_EVENT)	
		m_pPwrStateMgr->SubtractTimeout (dwTicksElapsed);

	if (dwReturn == WAIT_TIMEOUT)
		return Timeout;
	else if (dwReturn >= WAIT_OBJECT_0 + m_dwNumOfEvent)
	{   // Externed Event:
		return (PLATFORM_ACTIVITY_EVENT) (ExternedEvent + dwReturn - (WAIT_OBJECT_0 + m_dwNumOfEvent));
	}
	else
	{
		PLATFORM_ACTIVITY_EVENT platEvent = NoActivity;

		switch (dwReturn - WAIT_OBJECT_0)
		{
		  case PM_SHUTDOWN_EVENT:
			  platEvent = PmShutDown;
			  break;
		  case PM_RELOAD_ACTIVITY_TIMEOUTS_EVENT:
			  platEvent = PmReloadActivityTimeouts;
			  break;
		  case PM_MSGQUEUE_EVENT:
			  platEvent = MsgQueueEvent ();
			  break;
		  case PM_RESTART_TIMER_EVENT:
			  platEvent = RestartTimeouts;
			  break;
		  case PM_SYSTEM_TIMEIDLE_RESET:
			  m_pPwrStateMgr->ResetSystemIdleTimeTimeout (TRUE);
			  platEvent = SystemActivity;
			  break;
		  case PM_USER_ACTIVITY_EVENT:
			  m_pPwrStateMgr->ResetUserIdleTimeout (TRUE);
			  m_pPwrStateMgr->ResetSystemIdleTimeTimeout (TRUE);
			  platEvent = UserActivity;
              RETAILMSG(0,(L"USER_ACTIVITY_EVENT triggered!")); //Gard
			  break;
		  case PM_SYSTEM_API_EVENT:
			  platEvent = SystemPowerStateAPI;
			  break;
		  case PM_BOOTPHASE2_EVENT:	// This event only signal once.
			  platEvent = BootPhaseChanged;
			  m_dwEventArray[PM_BOOTPHASE2_EVENT] = m_hUnsignaledHandle;
			  m_pPwrStateMgr->DisablePhase2Event ();
			  break;
		}
		return platEvent;
	}
}

PLATFORM_ACTIVITY_STATE
PowerState::DefaultEventHandle (PLATFORM_ACTIVITY_EVENT platActivityEvent)
{
	switch (platActivityEvent)
	{
	  case AppButtonPressed:
		  m_LastNewState = On;
		  break;
	  default:
		  break;
	}
	return GetLastNewState ();
}

////////////////////////////////////////////////////////////////////////////////////
//
// PowerState StateValidateRegistry()
//
// Verifies that the current system power state is configured in the registry.  If
// the registry key for this state doesn't exist, this method will attempt to create
// the registry key and configure its registry values with the passed device power
// state and flags (power state hint) values.
//
////////////////////////////////////////////////////////////////////////////////////

DWORD
PowerState::StateValidateRegistry (DWORD dwDState, DWORD dwFlag)
{
	HKEY hkPM = NULL, hkSubkey;
	TCHAR pszSubKey[MAX_PATH];
	DWORD dwDisposition;

	SETFNAME (_T ("PowerState::StateValidateRegistry"));

	// Open the Power Manager registry key:
	DWORD dwStatus =
		RegCreateKeyEx (HKEY_LOCAL_MACHINE, PWRMGR_REG_KEY, 0, NULL, 0, 0, NULL, &hkPM, &dwDisposition);

	if (dwStatus != ERROR_SUCCESS)
	{
		PMLOGMSG (ZONE_ERROR,
				  (_T ("%s: can't open '%s', error is %d\r\n"), pszFname, PWRMGR_REG_KEY, dwStatus));
	}

	// Verify the system state:
	if (dwStatus == ERROR_SUCCESS)
	{
		StringCchPrintf (pszSubKey, MAX_PATH, _T ("State\\%s"), GetStateString ());
		dwStatus = RegCreateKeyEx (hkPM, pszSubKey, 0, NULL, 0, 0, NULL, &hkSubkey, &dwDisposition);
		if (dwStatus == ERROR_SUCCESS)
		{
			if (dwDisposition == REG_CREATED_NEW_KEY)
			{
				// Allow devices to go to any passed-in power level:
				DWORD dwValue = dwDState;	// D State
				dwStatus = RegSetValueEx (hkSubkey, NULL, 0, REG_DWORD, (LPBYTE) & dwValue, sizeof (dwValue));

				// Write the passed flags value:
				if (dwStatus == ERROR_SUCCESS)
				{
					dwValue = dwFlag;
					dwStatus = RegSetValueEx (hkSubkey, _T ("Flags"), 0, REG_DWORD, (LPBYTE) & dwValue, sizeof (dwValue));
				}
			}
			RegCloseKey (hkSubkey);
		}
		PMLOGMSG (dwStatus != ERROR_SUCCESS && ZONE_ERROR,
				  (_T ("%s: error %d while creating or writing values in '%s\\%s'\r\n"), pszFname,
				   dwStatus, PWRMGR_REG_KEY, pszSubKey));
	}

	// Release resources:
	if (hkPM != NULL)
		RegCloseKey (hkPM);
	return dwStatus;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateOff Methods -- Gard
//
//////////////////////////////////////////////////////////////////////////////

void
PowerStateOff::EnterState ()
{
    PmSetSystemPowerState_I (GetStateString (), 0, 0, TRUE);
}

PLATFORM_ACTIVITY_EVENT
PowerStateOff::WaitForEvent (DWORD, DWORD dwNumOfExternEvent, HANDLE * pExternEventArray)
{
 return NoActivity;
}




//////////////////////////////////////////////////////////////////////////////
//
// PowerStateOn Methods
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerStateOn::WaitForEvent (DWORD, DWORD dwNumOfExternEvent, 
                            HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;

	m_pPwrStateMgr->DisableResumingSuspendTimeout ();

	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = Unattended;
		  break;
	  case Timeout:
		  {
			  switch (TimeoutItem)
			  {
				case BacklightTimeout:
					m_LastNewState = BacklightOff;
					break;
				case UserActivityTimeout:
					m_LastNewState = UserIdle;
					break;
				case SuspendTimeout:
					m_LastNewState = Unattended;
					break;
				default:
					ASSERT (FALSE);
			  }
			  break;
		  }
	}
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateUserIdle Methods
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerStateUserIdle::WaitForEvent (DWORD, DWORD dwNumOfExternEvent, 
                                  HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;

	m_pPwrStateMgr->DisableResumingSuspendTimeout ();
	m_pPwrStateMgr->DisableBacklightTimeout ();
	m_pPwrStateMgr->DisableUserIdleTimeout ();

	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = Unattended;
		  break;
	  case UserActivity:
		  m_LastNewState = On;
		  break;
	  case Timeout:
		  {
			  switch (TimeoutItem)
			  {
				case SuspendTimeout:
					m_LastNewState = Unattended;
					break;
				default:
					ASSERT (FALSE);
			  }
			  break;
		  }
	}
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateUnattended Methods
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_STATE
PowerStateUnattended::DefaultEventHandle (PLATFORM_ACTIVITY_EVENT platActivityEvent)
{
	if (platActivityEvent == EnterUnattendedModeRequest)
	{
		m_pPwrStateMgr->ResetSystemIdleTimeTimeout (TRUE);
	}
	return PowerState::DefaultEventHandle (platActivityEvent);
}

void
PowerStateUnattended::EnterState ()
{
	m_pPwrStateMgr->ResetSystemIdleTimeTimeout (TRUE);
	PowerState::EnterState ();
}

PLATFORM_ACTIVITY_STATE
PowerStateUnattended::GetLastNewState ()
{
	if (m_LastNewState == Unattended)
	{
		if (m_pPwrStateMgr->GetUnattendedRefCount () == 0)
		{	// Unattended ref count is zero.
			return Suspend;
		}
	}
	return m_LastNewState;
}

PLATFORM_ACTIVITY_EVENT
PowerStateUnattended::WaitForEvent (DWORD dwTimeouts, DWORD dwNumOfExternEvent,
									HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;

	m_pPwrStateMgr->DisableResumingSuspendTimeout ();
	m_pPwrStateMgr->DisableBacklightTimeout ();
	m_pPwrStateMgr->DisableUserIdleTimeout ();

	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = On;
		  break;
	  case Timeout:
		  {
			  switch (TimeoutItem)
			  {
				case SuspendTimeout:
					m_LastNewState = Suspend;
					break;
				default:
					ASSERT (FALSE);
			  }
			  break;
		  }
	}
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateResuming Methods
//
//////////////////////////////////////////////////////////////////////////////

void
PowerStateResuming::EnterState ()
{
	PowerState::EnterState ();
	m_pPwrStateMgr->ReInitTimeOuts (FALSE);
}

PLATFORM_ACTIVITY_EVENT
PowerStateResuming::WaitForEvent (DWORD dwTimeouts, DWORD dwNumOfExternEvent,
								  HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;
	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = On;
		  break;
	  case UserActivity:
		  m_LastNewState = On;
		  break;
	  case Timeout:
		  m_LastNewState = Unattended;
		  break;
	}
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateSuspend Methods
//
//////////////////////////////////////////////////////////////////////////////

void
PowerStateSuspend::EnterState ()
{
	PmSetSystemPowerState_I (GetStateString (), 0, 0, TRUE);

	// Because it wakes up by a wakeup source, automatically enter the Resuming state.
	m_LastNewState = Resuming;	// Point to Resuming.

	// Initial timeout to Active:
	m_pPwrStateMgr->ClearUnattendedRefCount ();
	m_dwEventArray[PM_USER_ACTIVITY_EVENT] = m_pPwrStateMgr->GetUserActivityTimer ()->hevActive;
	m_pPwrStateMgr->ReInitTimeOuts (FALSE);
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateScreenOff Methods
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerStateScreenOff::WaitForEvent (DWORD dwTimeouts, DWORD dwNumOfExternEvent,
			 					   HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;

	m_pPwrStateMgr->DisableResumingSuspendTimeout ();
	m_pPwrStateMgr->DisableBacklightTimeout ();
	m_pPwrStateMgr->DisableUserIdleTimeout ();

	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = On;
		  break;
	  case Timeout:
		  {
			  switch (TimeoutItem)
			  {
				case SuspendTimeout:
					m_LastNewState = Unattended;
					break;
				default:
					ASSERT (FALSE);
			  }
			  break;
		  }
	}
	return activeEvent;
}

//////////////////////////////////////////////////////////////////////////////
//
// PowerStateBacklightOff Methods
//
//////////////////////////////////////////////////////////////////////////////

PLATFORM_ACTIVITY_EVENT
PowerStateBacklightOff::WaitForEvent (DWORD, DWORD dwNumOfExternEvent, 
                                      HANDLE * pExternEventArray)
{
	TIMEOUT_ITEM TimeoutItem;

	m_pPwrStateMgr->DisableResumingSuspendTimeout ();
	m_pPwrStateMgr->DisableBacklightTimeout ();

	DWORD dwTimeout = m_pPwrStateMgr->GetSmallestTimeout (&TimeoutItem);
	PLATFORM_ACTIVITY_EVENT activeEvent =
		PowerState::WaitForEvent (dwTimeout, dwNumOfExternEvent, pExternEventArray);
	switch (activeEvent)
	{
	  case PowerButtonPressed:
		  m_LastNewState = Unattended;
		  break;
	  case UserActivity:
		  m_LastNewState = On;
		  break;
	  case Timeout:
		  {
			  switch (TimeoutItem)
			  {
				case UserActivityTimeout:
					m_LastNewState = UserIdle;
					break;
				case SuspendTimeout:
					m_LastNewState = Unattended;
					break;
				default:
					ASSERT (FALSE);
			  }
			  break;
		  }
	}
	return activeEvent;
}
