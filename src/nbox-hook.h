
#include "nbox.h"

namespace nbox
{
    struct HookContext
    {
        // reverse the pushad pushfd
        size_t EFLAGS;
        size_t EDI;
        size_t ESI;
        size_t EBP;
        size_t ESP;
        size_t EBX;
        size_t EDX;
        size_t ECX;
        size_t EAX;
    };

    typedef void (*HookHandler)(void *addr, const HookContext *);

    int sethook(void *addr, HookHandler f);
    int delhook(void *addr);
}
