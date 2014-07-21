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
// This module contains code to handle notifications of devices being added
// and removed from the system.
//

#include "pmsqm.h"
#include "sqmclient.h"

/* put the ids in here that you want to track */
static const DWORD sgSQMDataIds[] = {
    PMSQM_DATAID_POWER_BKL_TOTAL,
    PMSQM_DATAID_POWER_BKL_ON,
    PMSQM_DATAID_POWER_BKL_OFF,
    PMSQM_DATAID_POWER_BAT_CHARGE,
    PMSQM_DATAID_POWER_BAT_DRAIN,
    PMSQM_DATAID_POWER_USER_SHUTDOWNS,
    PMSQM_DATAID_POWER_BAT_START_CHARGE_LEVEL,
};

/* change these if the # of datapoints change or their semantics do */
#define PMSQM_VERSION_HIGH  1
#define PMSQM_VERSION_LOW   0

static HSQMSESSION sghSqmNormalSession = NULL;
static HSQMSESSION sghSqmWeekSession   = NULL;

static BOOL sThreadedSQMAttemptStart(void)
{
    static DWORD numTries = 0;

    /* only call the proxy routines to attempt up to 10 times.  after that, don't bother any more */
    if (numTries>10)
        return FALSE;

    BOOL ret = FALSE;

    do {
        GUID machineId = {0};
        GUID userId = {0};

        /* check/start the normal daily session */
        if (!sghSqmNormalSession)
        {
            /* try to create the new session */
            sghSqmNormalSession = SqmGetManagedSession(TEXT("Global"));
            if (!sghSqmNormalSession)
                break;

            // Check for shared machine ID
            if (!SqmReadSharedMachineId(&machineId))
            {
                SqmCreateNewId(&machineId);
                SqmWriteSharedMachineId(&machineId);

                // Read it out again to make sure it was written.
                SqmReadSharedMachineId(&machineId);
            }
            // Set machine ID for the session
            SqmSetMachineId(sghSqmNormalSession,&machineId);

            // Check for shared user ID
            if (!SqmReadSharedUserId(&userId))
            {
                SqmCreateNewId(&userId);
                SqmWriteSharedUserId(&userId);

                // Read it out again to make sure it was written.
                SqmReadSharedUserId(&userId);
            }
            // Set user ID for the session
            SqmSetUserId(sghSqmNormalSession,&userId);

            // start the session now 
            SqmStartSession(sghSqmNormalSession);

            // normal session is ready to go now 
        }

        /* check/start the weeklong session */
        if (!sghSqmWeekSession)
        {
            /* try to create the new session */
            sghSqmWeekSession = SqmGetManagedSession(TEXT("PMWeek"));
            if (!sghSqmWeekSession)
                break;

            // Set machine ID for the session
            SqmSetMachineId(sghSqmWeekSession,&machineId);

            // Set user ID for the session
            SqmSetUserId(sghSqmWeekSession,&userId);

            // start the session now 
            SqmStartSession(sghSqmWeekSession);

            // weeklong session is ready to go now 
        }

        // if we get here we succeeded
        ret = TRUE;
#pragma warning(suppress:4127)
    } while (false); // yes, while false 

    return ret;
}

static void sThreadedSQMSet(DWORD markerId, DWORD dwValue)
{
    /* PMSQM_Set() calls are filtered through to this thread asynchronously (caller does not wait), 
       but this function will only ever be called synchronously */
    switch (markerId)
    {
    case PMSQM_DATAID_POWER_BKL_TOTAL:
        // total ms backlight on in a standard session
//        RETAILMSG(1, (TEXT("PMSQM: Backlight has been on for a total of %d ms\r\n"),dwValue));
        SqmSet(sghSqmNormalSession,PMSQM_DATAID_POWER_BKL_TOTAL,dwValue);
        break;

    case PMSQM_DATAID_POWER_BKL_ON:
        // individual backlight session lengths in ms in a standard session (Stream)
//        RETAILMSG(1, (TEXT("PMSQM: Backlight was on for a %d ms session\r\n"),dwValue));
        SqmAddToStream(sghSqmNormalSession,PMSQM_DATAID_POWER_BKL_ON,1,dwValue);
        break;
    
    case PMSQM_DATAID_POWER_BKL_OFF:
        // individual backlight off session lengths in ms in a standard session (Stream)
//        RETAILMSG(1, (TEXT("PMSQM: Backlight was off for a %d ms session\r\n"),dwValue));
        SqmAddToStream(sghSqmNormalSession,PMSQM_DATAID_POWER_BKL_OFF,1,dwValue);
        break;
    
    case PMSQM_DATAID_POWER_BAT_CHARGE:
        // total time battery charged in a week-long session
//        RETAILMSG(1, (TEXT("PMSQM: Battery was charged for a %d ms session\r\n"),dwValue));
        SqmSet(sghSqmWeekSession,PMSQM_DATAID_POWER_BAT_CHARGE,dwValue);
        break;
    
    case PMSQM_DATAID_POWER_BAT_DRAIN:
        // individual battery drain session lengths (no charging) in a week-long session (Stream)
//        RETAILMSG(1, (TEXT("PMSQM: Battery was drained for a %d ms session\r\n"),dwValue));
        SqmAddToStream(sghSqmWeekSession,PMSQM_DATAID_POWER_BAT_DRAIN,1,dwValue);
        break;
    
    case PMSQM_DATAID_POWER_USER_SHUTDOWNS:
        // total number of user-initiated shutdowns in a week-long session
//        RETAILMSG(1, (TEXT("PMSQM: User Initiated Shutdown\r\n")));
        SqmIncrement(sghSqmWeekSession,PMSQM_DATAID_POWER_USER_SHUTDOWNS,1);
        break;
    
    case PMSQM_DATAID_POWER_BAT_START_CHARGE_LEVEL:
        // individual percentages of battery charge level when charging session started in a week-long session (Stream)
//        RETAILMSG(1, (TEXT("PMSQM: Battery was at %d%% when charging started\r\n"),dwValue));
        SqmAddToStream(sghSqmWeekSession,PMSQM_DATAID_POWER_BAT_START_CHARGE_LEVEL,1,dwValue);
        break;
    default:
        RETAILMSG(1, (TEXT("ERROR: sThreadedSQMSet - unknown marker id(%d).\r\n"),markerId));
    }
}




/*--------------------------------------------------------------------------*/
/* support code - should not need to modify stuff below here                */
/*--------------------------------------------------------------------------*/




#define NUM_SQM_EVENTS (sizeof(sgSQMDataIds)/sizeof(DWORD))

static HANDLE sghSQMEvents[NUM_SQM_EVENTS];
static DWORD  sgSQMDataArg[NUM_SQM_EVENTS];
static HANDLE sghSQMThread = NULL;
static volatile DWORD sgSQMThreadId = 0;

static DWORD WINAPI sSQMThread(void * /*pParam*/)
{
    DWORD   myId = GetCurrentThreadId();

    /* set up event handles */
    DWORD   iEvent;
    ZeroMemory(sghSQMEvents,sizeof(HANDLE)*NUM_SQM_EVENTS);
    for(iEvent=0;iEvent<NUM_SQM_EVENTS;iEvent++)
    {
        sghSQMEvents[iEvent] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!sghSQMEvents[iEvent])
            break;
    }
    if (iEvent<NUM_SQM_EVENTS)
    {
        /* failed in creating an event - back out closing the other ones */
        while (iEvent>0)
        {
            iEvent--;
            CloseHandle(sghSQMEvents[iEvent]);
            sghSQMEvents[iEvent] = NULL;
        }
        /* set thread startup error code - relevant to thread id */
        sgSQMThreadId = (myId+1);
        return 0;
    }

    /* setting this to the proper value signifies the thread started ok */
    sgSQMThreadId = myId;

    /* wait for start condition (sqm is ready) */
    /* wait up to 15 * 10 seconds from power on start, then bail */
    DWORD dwTimeout = 15;
    do {
        Sleep(10000);
        if (sThreadedSQMAttemptStart())
            break;
    } while (--dwTimeout);

    if (dwTimeout>0)
    {
        /* SQM Session(s) started ok (we didn't time out waiting).  we can start waiting on sqm events now */
        for (;;) {
            /* do the wait, to the maximum of one day */
            DWORD dwWaitRet = WaitForMultipleObjects(NUM_SQM_EVENTS,sghSQMEvents,FALSE,INFINITE);

            /* check to see if one of our events woke us up */
            if ((dwWaitRet>=WAIT_OBJECT_0) && (dwWaitRet<WAIT_OBJECT_0+NUM_SQM_EVENTS))
            {
                sThreadedSQMSet(sgSQMDataIds[dwWaitRet-WAIT_OBJECT_0],sgSQMDataArg[dwWaitRet-WAIT_OBJECT_0]);
            }

        }
        /* should run forever (should never get here) */
    }

    /* close event handles */
    for(iEvent=0;iEvent<NUM_SQM_EVENTS;iEvent++)
    {
        if (sghSQMEvents[iEvent])
        {
            CloseHandle(sghSQMEvents[iEvent]);
            sghSQMEvents[iEvent] = NULL;
        }
    }

    return 0;
}

// this routine initializes the sqm (software quality metrics) functions for use
// A worker thread is started to handle the signal notifications from the mainline code.
BOOL
PMSQM_Start(void)
{
    /* make sure not already started */
    if (sghSQMThread)
        return FALSE;

    /* start sqm watcher thread */
    DWORD dwThreadId;
    sgSQMThreadId = 0;
    sghSQMThread = CreateThread(NULL,64*1024,sSQMThread,NULL,0,&dwThreadId);
    if (!sghSQMThread)
        return FALSE;
    DWORD dwTimeOut = 100;
    do {
        if (sgSQMThreadId)
            break;
        Sleep(10);
    } while (--dwTimeOut);

    if (sgSQMThreadId==dwThreadId)
        return TRUE;

    /* thread failed on startup or timed out starting up */

    if (sgSQMThreadId)
        TerminateThread(sghSQMThread,(DWORD)-1);
    WaitForSingleObject(sghSQMThread,INFINITE);
    CloseHandle(sghSQMThread);
    sghSQMThread = NULL;

//    RETAILMSG(1, (TEXT("PMSQM: Failed to start with error code %d\r\n"),sgSQMThreadId-dwThreadId));

    return FALSE;
}

// this function just sets an event to notify the main sqm thread that it should 
// call the sqm proxy routines to record the associated data point.  Possible data 
// points are enumerated with PMSQM_DATAID_xxx and match the DATAID_xxx on the 
// server side of the sqm stuff.
void 
PMSQM_Set(DWORD markerId, DWORD dwValue)
{
    /* this routine can never block
       and should not call any OS call other than SetEvent() */
    if (!sghSQMThread)
        return; // sqm not started or failed to start 
    DWORD iEvent = 0;
    while (iEvent<NUM_SQM_EVENTS)
    {
        if (sgSQMDataIds[iEvent]==markerId)
            break;
        iEvent++;
    }
    if (iEvent<NUM_SQM_EVENTS)
    {
        HANDLE hToSet = sghSQMEvents[iEvent];
        if (hToSet)
        {
            sgSQMDataArg[iEvent] = dwValue;
            SetEvent(sghSQMEvents[iEvent]);
        }
    }
}

