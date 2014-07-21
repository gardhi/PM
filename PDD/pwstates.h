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

#ifndef __PMSTATES_H
#define __PMSTATES_H

#include <Csync.h>
#include <cRegEdit.h>
#include <pmext.h>
#include <pmimpl.h> //Gard



///=============================================================
/// Edits by Gard
// Included from pmsystate.cpp
/*DWORD
PmSetSystemPowerState_I(LPCWSTR pwsState,
                        DWORD dwStateHint,
                        DWORD dwOptions,
                        BOOL fInternal);*/
//supressing warning, everything the compiler sees as unused will be a wrn -> err
#pragma warning(push) //saves warning settings on stack (will be fetched back by pop @end)
#pragma warning( disable:4512 )

//end edit by Gard
///=============================================================


// Enumeration types for timeouts:

typedef enum {
    NoTimeoutItem,
    SuspendTimeout,
    ResumingSuspendTimeout,
    BacklightTimeout,
    UserActivityTimeout,
 } TIMEOUT_ITEM, *PTIMEOUT_ITEM;

// Enumeration types for system power states:

typedef enum { 
    On,                     // The system is running normally with UI enabled.
    UserIdle,               // The system is in User Idle state.
    Unattended,             // The system is working in unattended mode.
    Resuming,               // The system is determining what to do after a resume.
    Suspend,                // The system is suspended; all devices are off (or wake-enabled).
    ScreenOff,              // The system is running normally with minimal UI.
    BacklightOff,           // The system is running normally with backlight off.
    ColdReboot,
    Reboot,
    Off,                    // The system is off -- Gard
    UnknownState = (-1)     // Unknown
} PLATFORM_ACTIVITY_STATE, *PPLATFORM_ACTIVITY_STATE;
    
#define STRING_ON           _T("on")
#define STRING_OFF          _T("off") //Gard
#define STRING_SCREENOFF    _T("screenoff")
#define STRING_BACKLIGHTOFF _T("backlightoff")
#define STRING_USERIDLE     _T("useridle")
#define STRING_UNATTENDED   _T("unattended")
#define STRING_RESUMING     _T("resuming")
#define STRING_SUSPEND      _T("suspend")
#define STRING_ColdReboot   _T("coldreboot")
#define STRING_Reboot       _T("reboot")

#define MAX_EVENT_ARRAY MAXIMUM_WAIT_OBJECTS 

#define DEF_ACSUSPENDTIMEOUT            600         // In seconds, 0 to disable
#define DEF_ACRESUMINGSUSPENDTIMEOUT    15          // In seconds, 0 to disable
#define DEF_ACBACKLIGHTTIMEOUT          60          // In seconds
#define DEF_ACUSERIDLETIMEOUT           120         // In seconds

#define DEF_BATTSUSPENDTIMEOUT          300         // In seconds, 0 to disable
#define DEF_BATTRESUMINGSUSPENDTIMEOUT  15          // In seconds, 0 to disable
#define DEF_BATTBACKLIGHTTIMEOUT        30          // In seconds.
#define DEF_BATTUSERIDLETIMEOUT         60          // In seconds,

class PowerStateManager;




//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerState
//
// Base class for system power state classes declared later in this file.
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerState
{
  public:
	PowerState (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState);
	virtual ~PowerState ();
	virtual void EnterState ();
	virtual BOOL Init ();
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE DefaultEventHandle (PLATFORM_ACTIVITY_EVENT dwHandleIndex);
	virtual PLATFORM_ACTIVITY_STATE GetState () = NULL;
	virtual LPCTSTR GetStateString () = NULL;
	virtual DWORD StateValidateRegistry (DWORD dwDState = 0, DWORD dwFlag = POWER_STATE_ON);
	virtual PLATFORM_ACTIVITY_STATE GetLastNewState ()
	{
		return m_LastNewState;
	};
	virtual PLATFORM_ACTIVITY_STATE SetSystemAPIState (PLATFORM_ACTIVITY_STATE apiState)
	{
		if (UnknownState != apiState)
			m_LastNewState = apiState;
		return m_LastNewState;
	}
	PowerState *GetNextPowerState ()
	{
		return m_pNextPowerState;
	};
	virtual BOOL AppsCanRequestState ()
	{
		return FALSE;
	}

  protected:
	PLATFORM_ACTIVITY_STATE m_LastNewState;
	PowerStateManager *const m_pPwrStateMgr;
	virtual PLATFORM_ACTIVITY_EVENT MsgQueueEvent ();

	HANDLE m_hUnsignaledHandle;
	DWORD m_dwNumOfEvent;
	PowerState *const m_pNextPowerState;
	HANDLE m_dwEventArray[MAX_EVENT_ARRAY];
};
/////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateOff -- Gard
//
/////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateOff : public PowerState {
    public:
    PowerStateOff (PowerStateManager * pPwrStateMgr, PowerState * pNextPowerState = NULL)
         : PowerState (pPwrStateMgr, pNextPowerState){}
    virtual void EnterState ();
    virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE,
                                                   DWORD dwNumOfExternEvent = 0,
                                                   HANDLE * pExternEventArray = NULL);
    virtual PLATFORM_ACTIVITY_STATE GetState ()
    {
         return Off;
    }
    virtual LPCTSTR GetStateString ()
    {
        return STRING_OFF;
    }
    virtual DWORD StateValidateRegistry (DWORD, DWORD)
    {
        return PowerState::StateValidateRegistry (4, POWER_STATE_OFF);
    }

};


//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateOn 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateOn : public PowerState
{
  public:
    PowerStateOn (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	// This state does not need Resume Time out.
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return On;
	};
	virtual LPCTSTR GetStateString ()
	{
		return STRING_ON;
	};
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (0, POWER_STATE_ON | POWER_STATE_BACKLIGHTON);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateUserIdle 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateUserIdle : public PowerState
{
  public:
    PowerStateUserIdle (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	// This state does not need Resume Time out.
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return UserIdle;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_USERIDLE;
    }
	virtual DWORD StateValidateRegistry (DWORD dwDState, DWORD dwFlag)
	{
		return PowerState::StateValidateRegistry (2, POWER_STATE_USERIDLE);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateUnattended 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateUnattended : public PowerState
{
  public:
    PowerStateUnattended (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	// This state does not need Resume Time out.
	virtual PLATFORM_ACTIVITY_STATE DefaultEventHandle (PLATFORM_ACTIVITY_EVENT dwHandleIndex);
	virtual void EnterState ();
	virtual PLATFORM_ACTIVITY_STATE GetLastNewState ();
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return Unattended;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_UNATTENDED;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (4, POWER_STATE_UNATTENDED);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateResuming 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateResuming : public PowerState
{
  public:
    PowerStateResuming (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	virtual void EnterState ();
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return Resuming;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_RESUMING;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (4, 0);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateSuspend 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateSuspend:public PowerState
{
  public:
    PowerStateSuspend (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	virtual void EnterState ();
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL)
	{
		// Suspend is no wait
		return NoActivity;
	}
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return Suspend;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_SUSPEND;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (3, POWER_STATE_SUSPEND);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateScreenOff 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateScreenOff : public PowerState
{
  public:
    PowerStateScreenOff (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	// This state does not need Resume Time out.
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return ScreenOff;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_SCREENOFF;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (4, POWER_STATE_IDLE);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateBacklightOff 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateBacklightOff : public PowerState
{
  public:
    PowerStateBacklightOff (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	// This state does not need Resume Time out.
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL);
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return BacklightOff;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_BACKLIGHTOFF;
    }
	virtual DWORD StateValidateRegistry (DWORD dwDState, DWORD dwFlag)
	{
		return PowerState::StateValidateRegistry (0, POWER_STATE_ON);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateColdReboot 
//
//////////////////////////////////////////////////////////////////////////////////////////////


class PowerStateColdReboot : public PowerState
{
  public:
    PowerStateColdReboot (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	virtual void EnterState ()
	{
		PmSetSystemPowerState_I (GetStateString (), 0, 0, TRUE);
		// Because it wakeup by wakeup source So it automatic enter Resuming State.
	}
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL)
	{
		// Suspend is no wait
		return NoActivity;
	}
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return ColdReboot;
    }
	virtual PLATFORM_ACTIVITY_STATE GetLastNewState ()
	{
		return ColdReboot;;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_ColdReboot;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (4, POWER_STATE_RESET);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
// PowerStateReboot 
//
//////////////////////////////////////////////////////////////////////////////////////////////

class PowerStateReboot : public PowerState
{
  public:
    PowerStateReboot (PowerStateManager *pPwrStateMgr, PowerState *pNextPowerState = NULL)
        : PowerState (pPwrStateMgr, pNextPowerState)
	{
    }

	virtual void EnterState ()
	{
		PmSetSystemPowerState_I (GetStateString (), 0, 0, TRUE);
		// Because it wakeup by wakeup source So it automatic enter Resuming State.
	}
	virtual PLATFORM_ACTIVITY_EVENT WaitForEvent (DWORD dwTimeouts = INFINITE, 
                                                  DWORD dwNumOfExternEvent = 0, 
                                                  HANDLE *pExternEventArray = NULL)
	{
		// Suspend is no wait
		return NoActivity;
	}
	virtual PLATFORM_ACTIVITY_STATE GetState ()
	{
		return Reboot;
    }
	virtual PLATFORM_ACTIVITY_STATE GetLastNewState ()
	{
		return Reboot;
    }
	virtual LPCTSTR GetStateString ()
	{
		return STRING_Reboot;
    }
	virtual DWORD StateValidateRegistry (DWORD, DWORD)
	{
		return PowerState::StateValidateRegistry (4, POWER_STATE_RESET);
	}
	virtual BOOL AppsCanRequestState ()
	{
		return TRUE;
	}
};

///-----------------------------------------------------
// edit by Gard: reset warning settings
#pragma warning( pop )
// end edit by Gard
/// ----------------------------------------------------

#endif
