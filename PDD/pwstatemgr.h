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

#ifndef __PMSTATEMGR_H
#define __PMSTATEMGR_H

#include <Csync.h>
#include <cRegEdit.h>
#include <pmext.h>
#include "pwstates.h"
#include <pmimpl.h> //Gard

///===================================================================
/// Edits by Gard
// Declares from pwButton.cpp so that they can be used in pwrMgr.cpp

//static DWORD WINAPI PwrBtnThread (LPVOID lpParam);


///===================================================================


//////////////////////////////////////////////////////////////////////////////
//
// PMSystemAPI
//
// PMSystemAPI is the base class for PowerStateManager; it implements 
// the serialization logic for state transitions and event-handling code 
// for the main Power Manager event loop.
//
//////////////////////////////////////////////////////////////////////////////

class PMSystemAPI : public CLockObject
{
  public:
	PMSystemAPI ();
    ~PMSystemAPI ();
	BOOL Init ();
	PLATFORM_ACTIVITY_STATE RequestedSystemPowerState ();
	void RequestComplete (DWORD dwStatus = ERROR_SUCCESS);
	DWORD SendSystemPowerState (LPCWSTR pwsState, DWORD dwStateHint, DWORD dwOptions);
	HANDLE GetAPISignalHandle ()
	{
		return m_hNotEmpty;
	};
	virtual PLATFORM_ACTIVITY_STATE SystemStateToActivityState (LPCTSTR lpState) = 0;

  private:
	DWORD m_dwStateHint;
	DWORD m_dwOptions;
	DWORD m_dwResult;

	HANDLE m_hEmpty;
	HANDLE m_hNotEmpty;
	HANDLE m_hComplete;
	TCHAR m_szStateName[MAX_PATH];
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// NotifyRegKey
//
// Utility class for creating a change notification handle based on the specified 
// registry key. This class is used by PowerStateManager.
//
//////////////////////////////////////////////////////////////////////////////////////////////

class NotifyRegKey : public CRegistryEdit
{
  public:
	NotifyRegKey (HKEY hCurrentOpenKey, LPCTSTR lpKeyPath)
        : CRegistryEdit (hCurrentOpenKey, lpKeyPath)
	{
		m_hNotifyEvent = INVALID_HANDLE_VALUE;
		if (GetHKey () != NULL)
		{
			m_hNotifyEvent = CeFindFirstRegChange (GetHKey (), TRUE, REG_NOTIFY_CHANGE_LAST_SET);
		}
	}
    ~NotifyRegKey ()
	{
		if (m_hNotifyEvent != INVALID_HANDLE_VALUE)
			CloseHandle (m_hNotifyEvent);
	}
	BOOL Init ()
	{
		return m_hNotifyEvent != INVALID_HANDLE_VALUE;
	};
	BOOL GetRegValue (LPCWSTR lpcName, PDWORD lpdwData)
	{
		DWORD dwValue = 0;

		if (CRegistryEdit::GetRegValue (lpcName, (PBYTE) & dwValue, sizeof (dwValue)))
		{
			if (lpdwData)
			{
				if (dwValue >= INFINITE / 1000)
					dwValue = INFINITE / 1000;

				*lpdwData = (dwValue != 0 ? dwValue * 1000 : INFINITE);
			}
			return TRUE;
		}
		else
			return FALSE;
	}
	BOOL AckNotification ()
	{
		return CeFindNextRegChange (m_hNotifyEvent);
	};
	HANDLE GetNotificationHandle ()
	{
		return m_hNotifyEvent;
	};
  private:
	HANDLE m_hNotifyEvent;
};

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

class PowerStateManager : public PMSystemAPI
{
  public:
	PowerStateManager ();
	~PowerStateManager ();
	BOOL Init ();
	virtual PLATFORM_ACTIVITY_STATE SystemStateToActivityState (LPCTSTR lpState);
	virtual LPCTSTR ActivityStateToSystemState (PLATFORM_ACTIVITY_STATE platActiveState);

	// Timer Function.
	virtual void ReInitTimeOuts (BOOL fIgnoreSuspendResume);
	virtual void ReAdjustTimeOuts ();
	virtual void ResetUserIdleTimeout (BOOL fIdle);
	virtual void ResetSystemIdleTimeTimeout (BOOL fIdle);
	virtual void SubtractTimeout (DWORD dwTicks);
	virtual DWORD GetSmallestTimeout (PTIMEOUT_ITEM pTimeoutItem);
	void DisableBacklightTimeout ()
	{
		m_dwCurBacklightTimeout = INFINITE;
	};
	void DisableUserIdleTimeout ()
	{
		m_dwCurUserIdleTimeout = INFINITE;
	};
	void DisableSuspendTimeout ()
	{
		m_dwCurSuspendTimeout = INFINITE;
	};
	void DisableResumingSuspendTimeout ()
	{
		m_dwCurResumingSuspendTimeout = INFINITE;
	};
	virtual void DisablePhase2Event ();

	HANDLE GetEventHandle (DWORD dwIndex);
	PowerState *GetFirstPowerState ();
	virtual DWORD ThreadRun ();
	virtual void PlatformLoadTimeouts ();
	virtual BOOL CreatePowerStateList ();
	PowerState *GetStateObject (PLATFORM_ACTIVITY_STATE newState);

	// Timeout Methods.
	DWORD GetSuspendTimeOut ();
	DWORD GetResumingSuspendTimeout ();
	DWORD GetBackLightTimeout ();
	DWORD GetUserIdleTimeOut ();

	// Unattended Mode Ref Count Methods.
	void ClearUnattendedRefCount ()
	{
		m_dwUnattendedModeRef = 0;
    }
	DWORD IncUnattendedRefCount ()
	{
		return (++m_dwUnattendedModeRef);
    }
	DWORD DecUnattendedRefCount ()
	{
        if (m_dwUnattendedModeRef)
        {
			return (--m_dwUnattendedModeRef);
        }
        else
        {
			return 0;
        }
    }
	DWORD GetUnattendedRefCount ()
	{
		return m_dwUnattendedModeRef;
    }
	PowerState *GetPowerStatesList ()
	{
		return m_pPowerStateList;
    }
	PACTIVITY_TIMER GetUserActivityTimer ()
	{
		return m_pUserActivity;
    }

  protected:
	// Created Event.
	HANDLE m_hevBootPhase2;
	HANDLE m_hevReloadActivityTimeouts;
	HANDLE m_hevRestartTimers;
	HANDLE m_hevSystemIdleTimerReset;

	// Global Event Handle.
	HANDLE m_hevPmShutdown;
	HANDLE m_hqNotify;
	NotifyRegKey *m_pLegacySPScreenOff;
	NotifyRegKey *m_pLegacyBacklightOff;

	PACTIVITY_TIMER m_pUserActivity;

	// TimeOut Parameter.
	DWORD m_dwACSuspendTimeout;
	DWORD m_dwACResumingSuspendTimeout;
	DWORD m_dwACBacklightTimeout;
	DWORD m_dwACUserIdleTimeout;

	DWORD m_dwBattSuspendTimeout;
	DWORD m_dwBattResumingSuspendTimeout;
	DWORD m_dwBattBacklightTimeout;
	DWORD m_dwBattUserIdleTimeout;

	PowerState *m_pPowerStateList;

	DWORD m_dwCurSuspendTimeout;
	DWORD m_dwCurResumingSuspendTimeout;
	DWORD m_dwCurBacklightTimeout;
	DWORD m_dwCurUserIdleTimeout;

	// Unattended Mode Ref Count.
	DWORD m_dwUnattendedModeRef;

  private:
	PowerState *SetSystemState (PowerState *pCurPowerState);
	BOOL ReInitLegacyRegistry ();
};
#endif
