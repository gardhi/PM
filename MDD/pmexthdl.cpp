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
// This module implements a set of function that support OEM power extension
//
#include <windows.h>
#include <pmimpl.h>
#include <pmext.h>
#include "pmexthdl.hpp"
PMExtension::PMExtension(HKEY hKey, LPCTSTR lpRegistryPath, PMExtension * pNextExt)
:   CRegistryEdit(hKey,lpRegistryPath)
,   m_pNextPMExt(pNextExt)
{
    m_dwContext = 0 ;
    

    m_pInit =NULL;
    m_pDeinit = NULL;;
    
    m_pGetNotificationHandle =NULL;
    m_pEventNotification=NULL;

    m_pPMExt_PMBeforeNewSystemState = NULL;
    m_pPMExt_PMAfterNewSystemState = NULL;

    m_pPMExt_PMBeforeNewDeviceState = NULL;
    m_pPMExt_PMAfterNewDeviceState = NULL;

    m_hLib = NULL;

    TCHAR DevDll[DEVDLL_LEN];
    DWORD Flags;
    if (IsKeyOpened() && GetRegValue( DEVLOAD_DLLNAME_VALNAME, (LPBYTE) DevDll, sizeof(DevDll) )) {
        if (!GetRegValue( DEVLOAD_FLAGS_VALNAME, (LPBYTE) &Flags, sizeof(Flags)))
            Flags = DEVFLAGS_NONE;
        m_hLib = (Flags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(DevDll) : LoadDriver(DevDll);
        if (!m_hLib) {
            DEBUGMSG(ZONE_ERROR, (_T("PMExtension::PMExtension: couldn't load '%s' -- error %d\r\n"), 
                DevDll, GetLastError()));
        } 
        else { // Load entry.
            m_pInit = (PFN_PMExt_Init)GetProcAddress(m_hLib, PMExt_Init_NAME);
            m_pDeinit = (PFN_PMExt_DeInit)GetProcAddress(m_hLib, PMExt_DeInit_NAME);

            m_pGetNotificationHandle = (PFN_PMExt_GetNotificationHandle)GetProcAddress(m_hLib,PMExt_GetNotificationHandle_NAME);
            m_pEventNotification = (PFN_PMExt_EventNotification)GetProcAddress(m_hLib,PMExt_EventNotification_NAME);

            m_pPMExt_PMBeforeNewSystemState = (PFN_PMExt_PMBeforeNewSystemState)GetProcAddress(m_hLib,PMExt_PMBeforeNewSystemState_NAME);
            m_pPMExt_PMAfterNewSystemState = (PFN_PMExt_PMAfterNewSystemState)GetProcAddress(m_hLib,PMExt_PMAfterNewSystemState_NAME);

            m_pPMExt_PMBeforeNewDeviceState = (PFN_PMExt_PMBeforeNewDeviceState)GetProcAddress(m_hLib,PMExt_PMBeforeNewDeviceState_NAME);
            m_pPMExt_PMAfterNewDeviceState = (PFN_PMExt_PMAfterNewDeviceState)GetProcAddress(m_hLib,PMExt_PMAfterNewDeviceState_NAME);
        }
        
    }
    if (m_pInit && m_pDeinit) {
        m_dwContext = PMExt_Init(hKey,lpRegistryPath);
    }
}
PMExtension::~PMExtension()
{
    if (m_dwContext) {
        PMExt_DeInit();
    }
    if (m_hLib) {
        VERIFY(FreeLibrary( m_hLib ));
    }
}
PMExtensionMgr::PMExtensionMgr()
{
    m_dwNumOfHandle = 0; 

    m_PMExtensionList = NULL;

    CRegistryEdit pmExtReg(HKEY_LOCAL_MACHINE, PMExt_Registry_Root );
    if (pmExtReg.IsKeyOpened()) {
        WCHAR DevName[DEVKEY_LEN];
        DWORD DevNameLength = _countof(DevName);
        DWORD dwRegIndex = 0;
        while (pmExtReg.RegEnumKeyEx(dwRegIndex, DevName , &DevNameLength, NULL, NULL, NULL, NULL)) {
            PMExtension * pmExt = new PMExtension(pmExtReg.GetHKey(), DevName, m_PMExtensionList);
            if (pmExt && pmExt->Init()) {
                PMLOGMSG(ZONE_INIT, (_T("Load PM extension from %s Successfully \r\n"), DevName));
                m_PMExtensionList = pmExt;
                // We try to get handle.
                if (m_dwNumOfHandle<MAXIMUM_WAIT_OBJECTS) {
                    HANDLE hPmExtHandle = pmExt->PMExt_GetNotificationHandle();
                    if (hPmExtHandle) {
                        m_hPmExended[m_dwNumOfHandle] = hPmExtHandle;
                        m_OwnerPMExt[m_dwNumOfHandle] = pmExt;
                        m_dwNumOfHandle++;
                    }
                }
                else if (pmExt->PMExt_GetNotificationHandle()!=NULL) {
                    PMLOGMSG(ZONE_ERROR, (_T("Load PM extension handle on %s overflow.\r\n"), DevName));
                }
            }
            else {
                PMLOGMSG(ZONE_ERROR, (_T("Load PM extension from %s failed \r\n"), DevName));
                if (pmExt)
                    delete pmExt;
            }
            dwRegIndex++;
            DevNameLength = _countof(DevName);
        }
    }
}
PMExtensionMgr::~PMExtensionMgr()
{
    while (m_PMExtensionList) {
        PMExtension * pNext = m_PMExtensionList->GetNextPmExt();
        delete m_PMExtensionList;
        m_PMExtensionList = pNext ;
    }
}
BOOL  PMExtensionMgr::PMExt_GetNotificationHandle(DWORD& dwNumEvent, HANDLE PmEventArray[])
{
    DWORD dwMaxNumEvent= dwNumEvent;
    dwNumEvent = 0 ;
    if (PmEventArray!=NULL && m_dwNumOfHandle < dwMaxNumEvent) {
        for (dwNumEvent = 0; dwNumEvent< m_dwNumOfHandle; dwNumEvent++) {
            PmEventArray[dwNumEvent] = m_hPmExended[dwNumEvent];
        }
        return TRUE;
    }
    else {
        PMLOGMSG(ZONE_ERROR, (_T("PM extension handle overflow.\r\n")));
        return FALSE;
    }
}
VOID   PMExtensionMgr::PMExt_EventNotification(PLATFORM_ACTIVITY_EVENT platformActivityEvent)
{
    PMExtension * pCurExt = m_PMExtensionList ;
    if (platformActivityEvent<PowerManagerExt) { // This is public event. Every extension got it.
        while (pCurExt) {
            pCurExt->PMExt_EventNotification(platformActivityEvent);
            pCurExt = pCurExt->GetNextPmExt();
        }
    }
    else if (platformActivityEvent< ExternedEvent) { // This is PMExt Event.
        DWORD dwIndex = (DWORD)(platformActivityEvent- PowerManagerExt);
        if (dwIndex < _countof(m_OwnerPMExt)) {
            if (dwIndex<m_dwNumOfHandle && m_OwnerPMExt[dwIndex]!=NULL)
                m_OwnerPMExt[dwIndex]->PMExt_EventNotification(PowerManagerExt);
        } else {
            ASSERT(FALSE);
        }
    }
}
PMExtensionMgr * g_pPMExtMgr = NULL;

BOOL    PMExt_Init()
{
    if (g_pPMExtMgr == NULL ) {
        g_pPMExtMgr = new PMExtensionMgr();
    }
    return g_pPMExtMgr!=NULL;
}
BOOL    PMExt_DeInit()
{
    if (g_pPMExtMgr)
        delete g_pPMExtMgr;
    g_pPMExtMgr = NULL;
    return TRUE;
}
BOOL    PMExt_GetNotificationHandle(DWORD& dwNumEvent, HANDLE PmEventArray[])
{
    if (g_pPMExtMgr)
        return g_pPMExtMgr->PMExt_GetNotificationHandle(dwNumEvent, PmEventArray);
    else
        return FALSE;
}
VOID    PMExt_EventNotification(PLATFORM_ACTIVITY_EVENT platformActivityEvent)
{
    if (g_pPMExtMgr)
        g_pPMExtMgr->PMExt_EventNotification(platformActivityEvent);
}
VOID    PMExt_PMBeforeNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags)
{
    if (g_pPMExtMgr)
        g_pPMExtMgr->PMExt_PMBeforeNewSystemState(lpNewStateName,dwFlags);
}
VOID    PMExt_PMAfterNewSystemState(LPCTSTR lpNewStateName, DWORD dwFlags)
{
    if (g_pPMExtMgr)
        g_pPMExtMgr->PMExt_PMAfterNewSystemState(lpNewStateName,dwFlags);
}
VOID    PMExt_PMBeforeNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE curDx, CEDEVICE_POWER_STATE reqDx)
{
    if (g_pPMExtMgr)
        g_pPMExtMgr->PMExt_PMBeforeNewDeviceState(pszName,curDx,reqDx);
}
VOID    PMExt_PMAfterNewDeviceState(LPCTSTR pszName, CEDEVICE_POWER_STATE prevDx, CEDEVICE_POWER_STATE curDx)
{
    if (g_pPMExtMgr)
        g_pPMExtMgr->PMExt_PMAfterNewDeviceState(pszName,prevDx,curDx);
}


