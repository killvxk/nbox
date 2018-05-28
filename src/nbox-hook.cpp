
#include <assert.h>
#include <map>

#include <X86Disasm.hh>

#include "nbox-hook.h"

#define SIZEOFCALL 5        // Same as size of JMP

static void suspendThreads()
{
}

static void resumeThreads()
{
}

namespace nbox
{
    struct HookData;
    static std::map<void*, HookData *> g_hookmap;

    struct HookData
    {
    private:
        HookData() {}
        ~HookData() {}

        uint8_t _osize = 0;         // original code size

    public:
        HookHandler handler = nullptr;
        uint8_t code[1];

        int sizeofcode() { return _osize + SIZEOFCALL; }
        int sizeoforigin() { return _osize; }
        int totalsize() { return offsetof(HookData, code) + sizeofcode(); }

        /* using namespace asmjit; */
        static HookData *New(void *addr)
        {
            CX86Disasm86 dis;

            if (dis.GetError()) puts("cs_open failure");
            //set how deep should capstone reverse instruction
            dis.SetDetail(cs_opt_value::CS_OPT_ON);
            //set syntax for output disasembly string
            dis.SetSyntax(cs_opt_value::CS_OPT_SYNTAX_INTEL);

            // find out enough memory to contain the 'call' codes
            auto CODE = readBytes((uint8_t*)addr, 16);
            //int count = cs_disasm(handle, (const uint8_t *)CODE.c_str(), CODE.size(), 0x1000, 0, &insn);
            auto insn = dis.Disasm(CODE.c_str(), CODE.size(), reinterpret_cast<size_t>(addr));
            int codesize = 0;
            for (int j = 0; j < insn->Count && codesize < SIZEOFCALL; j++)
                codesize += insn->Instructions(j)->size;

            if (codesize < SIZEOFCALL) return nullptr;

            // write the 'call' codes
            auto fullsize = codesize + SIZEOFCALL;
            auto self = (HookData *)VirtualAlloc(nullptr,
                    offsetof(HookData, code) + fullsize,
                    MEM_COMMIT | MEM_RESERVE,
                    PAGE_EXECUTE_READWRITE);
            self->_osize = codesize;
            memcpy(self->code, CODE.c_str(), self->sizeoforigin());
            auto pjmp = self->code + codesize;
            pjmp[0] = 0xE9;
            auto nextaddr = (uint8_t *)addr + codesize;
            *(size_t *)&pjmp[1] = nextaddr - pjmp - SIZEOFCALL;
            return self;
        }

        static void Delete(HookData *hd)
        {
            VirtualAlloc(hd, hd->totalsize(), MEM_FREE, 0);
        }
    };

    extern "C" void fasm_hook_handler();

    extern "C"
    void * __stdcall nbox_hook_handler(void *addr, const HookContext *context)
    {
        auto hookdata = g_hookmap[addr];
        assert(hookdata);

        if (hookdata->handler)
            hookdata->handler(addr, context);
        else;

        return (void *)hookdata->code;
    }

    int sethook(void *addr, HookHandler f)
    {
        if (g_hookmap.count(addr))
            return -1;
        auto hd = HookData::New(addr);
        if (!hd) return 1;
        hd->handler = f;

        suspendThreads();

        uint8_t codes[SIZEOFCALL];
        codes[0] = 0xE8;
        *(size_t*)&codes[1] = (uint8_t*)&fasm_hook_handler - (uint8_t*)addr - SIZEOFCALL;
        auto success = writeBytes(addr, codes, SIZEOFCALL);

        resumeThreads();
        if (success)
            g_hookmap[addr] = hd;
        else
            return HookData::Delete(hd), 1;
        return 0;
    }

    int delhook(void *addr)
    {
        auto hd = g_hookmap[addr];
        if (!hd) return 1;

        suspendThreads();
        // write origin code
        writeBytes(addr, hd->code, hd->sizeoforigin());

        resumeThreads();
        HookData::Delete(hd);
        g_hookmap.erase(addr);
        return 0;
    }
}
