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

//
// This module contains power manger extention.
//

// This typedef describes the system activity states.  These are independent of
// factors such as AC power vs. battery, in cradle or not, etc.  OEMs may choose
// to add their own activity states if they customize this module.

// This typedef describes activity events such as user activity or inactivity,
// power status changes, etc.  OEMs may choose to factor other events into their
// system power state transition decisions.

#pragma once
#include <CRegEdit.h>
#include <pmext.h>
// PM Extension Handling Class

class PMExtension : protected CRegistryEdit {
public:
    PMExtension(HKEY hKey, LPCTSTR lpRegistryPath, PMExtension * pNextExt);
    ~PMExtension();
    BOOL    Init() { return (m_dwContext!=0); };
    PMExtension *GetNextPmExt() { return m_pNextPMExt; };
    DWORD   PMExt_Init(HKEY hKey, LPCTSTR lpRegistryPath) {
        if (m_dwContext == 0 && m_pInit!=NULL) {
            __try {
                m_dwContext =m_pInit(hKey, lpRegistryPath);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                m_dwContext = 0;
            }
        }
        return m_dwContext;
    }
    VOID    PMExt_DeInit() {
        if (m_dwContext && m_pDeinit) {
            __try {
                m_pDeinit(m_dwContext);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        m_dwContext = 0 ;
    }
    HANDLE  PMExt_GetNotificationHandle() {
        HANDLE hHandle = NULL;
        if (m_dwContext && m_pGetNotificationHandle) {
            __try {
                hHandle =m_pGetNotificationHandle(m_dwContext);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                hHandle = NULL;
            }
        }
        return hHandle;
    }
    VOID    PMExt_EventNotification(PLATFORM_ACTIVITY_EVENT platformActivityEvent) {
        if (m_dwContext && m_pEventNotification) {
            __try {
                m_pEventNotification(m_dwContext,platformActivityEvent);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    VOID    PMExt_PMBeforeNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags) {
        if (m_dwContext && m_pPMExt_PMBeforeNewSystemState) {
            __try {
                m_pPMExt_PMBeforeNewSystemState(m_dwContext,lpNewStateName,dwFlags);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    VOID    PMExt_PMAfterNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags) {
        if (m_dwContext && m_pPMExt_PMAfterNewSystemState) {
            __try {
                m_pPMExt_PMAfterNewSystemState(m_dwContext,lpNewStateName,dwFlags);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    VOID    PMExt_PMBeforeNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx) {
        if (m_dwContext && m_pPMExt_PMBeforeNewDeviceState) {
            __try {
                m_pPMExt_PMBeforeNewDeviceState(m_dwContext,pszName,curDx,reqDx);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        
    }
    VOID    PMExt_PMAfterNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx) {
        if (m_dwContext && m_pPMExt_PMAfterNewDeviceState) {
            __try {
                m_pPMExt_PMAfterNewDeviceState(m_dwContext,pszName,curDx,reqDx);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

protected:
    HINSTANCE   m_hLib; 
    DWORD       m_dwContext;
    PMExtension *m_pNextPMExt;

    
    PFN_PMExt_Init  m_pInit;
    PFN_PMExt_DeInit    m_pDeinit;
    
    PFN_PMExt_GetNotificationHandle m_pGetNotificationHandle;
    PFN_PMExt_EventNotification     m_pEventNotification;

    PFN_PMExt_PMBeforeNewSystemState    m_pPMExt_PMBeforeNewSystemState;
    PFN_PMExt_PMAfterNewSystemState     m_pPMExt_PMAfterNewSystemState;

    PFN_PMExt_PMBeforeNewDeviceState    m_pPMExt_PMBeforeNewDeviceState;
    PFN_PMExt_PMAfterNewDeviceState     m_pPMExt_PMAfterNewDeviceState;

};


class PMExtensionMgr {
public:
    PMExtensionMgr();
    ~PMExtensionMgr();
    BOOL     PMExt_GetNotificationHandle(DWORD& dwNumEvent, HANDLE PmEventArray[]);
    VOID     PMExt_EventNotification(PLATFORM_ACTIVITY_EVENT platformActivityEvent);
    VOID     PMExt_PMBeforeNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags) {
        PMExtension * pCurExt = m_PMExtensionList ;
        while (pCurExt) {
            pCurExt->PMExt_PMBeforeNewSystemState(lpNewStateName,dwFlags);
            pCurExt = pCurExt->GetNextPmExt();
        }
    }
    VOID     PMExt_PMAfterNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags) {
        PMExtension * pCurExt = m_PMExtensionList ;
        while (pCurExt) {
            pCurExt->PMExt_PMAfterNewSystemState(lpNewStateName,dwFlags);
            pCurExt = pCurExt->GetNextPmExt();
        }
    }

    VOID     PMExt_PMBeforeNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx){
        PMExtension * pCurExt = m_PMExtensionList ;
        while (pCurExt) {
            pCurExt->PMExt_PMBeforeNewDeviceState(pszName, curDx, reqDx);
            pCurExt = pCurExt->GetNextPmExt();
        }
    }
        
    VOID     PMExt_PMAfterNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx) {
        PMExtension * pCurExt = m_PMExtensionList ;
        while (pCurExt) {
            pCurExt->PMExt_PMAfterNewDeviceState(pszName, curDx, reqDx);
            pCurExt = pCurExt->GetNextPmExt();
        }
    }
protected:
    PMExtension *   m_PMExtensionList;

    DWORD           m_dwNumOfHandle;
    HANDLE          m_hPmExended[MAXIMUM_WAIT_OBJECTS];
    PMExtension *   m_OwnerPMExt[MAXIMUM_WAIT_OBJECTS];
    
};
extern PMExtensionMgr * g_pPMExtMgr;

BOOL    PMExt_Init();
BOOL    PMExt_DeInit();
BOOL    PMExt_GetNotificationHandle(DWORD& dwNumEvent, HANDLE PmEventArray[]);
VOID    PMExt_EventNotification(PLATFORM_ACTIVITY_EVENT platformActivityEvent);
VOID    PMExt_PMBeforeNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags);
VOID    PMExt_PMAfterNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags);
VOID    PMExt_PMBeforeNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx);
VOID    PMExt_PMAfterNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE prevDx, CEDEVICE_POWER_STATE curDx);


