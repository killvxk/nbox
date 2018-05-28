
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <string>
#include <iostream>

#include "nbox.h"
#include "nbox-lua.h"
#include "nbox-dbg.h"
#include "nbox-funcs.h"




struct Event
{
    Event() : _handle() {}
    ~Event() { CloseHandle(_handle); }

    int wait(DWORD ms = INFINITE)
    {
        return WaitForSingleObject(_handle, ms);
    }
    int set() { return SetEvent(_handle); }
    void reset() { ResetEvent(_handle); }

    const HANDLE _handle;
};

Event g_ev_dbgev;       // debug event
Event g_ev_debcnt;      // debug continue
Event g_ev_waitui;
DebugState g_debugstate;

EXCEPTION_POINTERS *g_except;
const char *g_break_msg;


// inline void checkUpdateAssembly()

int WorkThread()
{
}

