
#include <Windows.h>

enum DebugState
{
    DS_NULL,
    DS_BREAK,
    DS_STEP,            // step-in
    DS_NEXT,            // step-out
    DS_RUN,
    // For lua handler
    DS_NOTHANDLE,
};

struct DbgData
{
    HANDLE ev_raw;      // Event in the debuggee
    DebugState dbg_state;
    DWORD threadid;
    EXCEPTION_RECORD record;
    CONTEXT context;
};
