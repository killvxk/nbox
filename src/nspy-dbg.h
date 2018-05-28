#ifndef __NSPY_DBG_H__
#define __NSPY_DBG_H__

#include <Windows.h>
#include <unordered_set>
#include <mutex>

namespace nbox
{
    // continue if returned ture
    typedef bool(*BrkptHandler)(EXCEPTION_POINTERS *E);

    struct Breakpoint
    {
        static Breakpoint *Add(void *addr, BrkptHandler h = nullptr, bool temp = false);
        static Breakpoint *Get(void *addr);
        static unordered_set<Breakpoint*> s_revert;

        int remove();
        bool enable(bool b = true);
        bool disable() { return enable(false); }
        bool revert();

        bool istemp() { return temp; }

        void *const address;
        BrkptHandler handler;
        bool temp;
        const uint8_t rawcode;

    private:
        Breakpoint(void *addr, uint8_t raw);
        ~Breakpoint() {}
    };
}

extern std::unordered_set<DWORD> g_ignore_threads;

#endif /* __NSPY_DBG_H__ */
