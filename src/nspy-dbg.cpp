
#include <assert.h>

#include "nbox.h"
#include "nspy-lua.h"
#include "nspy-dbg.h"

using namespace std;
using namespace nbox;

unordered_set<DWORD> g_ignore_threads;

static recursive_mutex g_veh_mutex;
static unordered_map<void*, Breakpoint*> g_bps;

unordered_set<Breakpoint*> Breakpoint::s_revert;

vector<Breakpoint*> Breakpoint::Set()
{
    vector<Breakpoint*> vecs;
    vecs.reserve(g_bps.size());
    for (auto it : g_bps)
        vecs.push_back(it.second);
    return vecs;
}

Breakpoint *Breakpoint::Get(void *addr)
{
    lock_guard<recursive_mutex> lock(g_veh_mutex);

    return g_bps.count(addr) ? g_bps[addr] : nullptr;
}

Breakpoint::Breakpoint(void *addr, uint8_t raw)
    : address(addr), rawcode(raw) {}

Breakpoint *Breakpoint::Add(void *addr, BrkptHandler h, bool temp)
{
    lock_guard<recursive_mutex> lock(g_veh_mutex);

    auto bp = Get(addr);
    uint8_t rawcode;
    if (!bp && readValue(addr, rawcode))
    {
        bp = new Breakpoint(addr, rawcode);
        if (!bp->enable())
        {
            delete bp; return nullptr;
        }
        g_bps[addr] = bp;
        printf("BPADD: %p %p\n", addr, bp);
    }
    bp->handler = h, bp->temp = temp;
    return bp;
}

int Breakpoint::remove()
{
    lock_guard<recursive_mutex> lock(g_veh_mutex);

    this->disable();
    g_bps.erase(address), s_revert.erase(this);
    delete this;
    return 0;
}

bool Breakpoint::revert()
{
    s_revert.insert(this);
    return disable();
}

bool Breakpoint::enable(bool b)
{
    uint8_t data = b ? 0xCC : this->rawcode;
    return writeValue(address, data) && FlushInstructionCache(nbox::hProcess, address, 1);
}
