
#include <Windows.h>
#include <thread>
#include <X86Disasm.hh>

#include "nbox.h"
#include "nspy-dbg.h"
#include "nspy-lua.h"
#include "nbrpc.h"
#include "dbg-data.h"

using namespace nbox;
using namespace std;
using namespace nrpc;
using namespace xval;

NBoxSession g_ses;

recursive_mutex g_veh_mutex;
DebugState g_debugstate;

HANDLE g_evwaitui = CreateEvent(nullptr, FALSE, FALSE, nullptr);

void updateRegsView(CONTEXT *ct)
{
    auto d = NewDict(
        "EAX", (uint64_t)ct->Eax,
        "EBX", (uint64_t)ct->Ebx,
        "ECX", (uint64_t)ct->Ecx,
        "EDX", (uint64_t)ct->Edx,
        "EBP", (uint64_t)ct->Ebp,
        "ESP", (uint64_t)ct->Esp,
        "EDI", (uint64_t)ct->Edi,
        "ESI", (uint64_t)ct->Esi,
        "EIP", (uint64_t)ct->Eip,
        "EFLAGS", (uint64_t)ct->EFlags
    );
    g_ses.fnotify("ndbg#asm#on_regs", d);
}

void updateStackView(CONTEXT *context)
{
    auto st = backtrace(context);
    auto l = List::New(st.size());
    for (auto frame : st)
    {
        auto d = NewDict("retaddr", (uint64_t)frame.retaddr);
        if (frame.symbol.size())
            d.set("symbol", frame.symbol).set("sym_dis", frame.sym_dis);
        if (frame.file.size())
            d.set("file", frame.file).set("line", (int)frame.line);
        l.list().push_back(d);
    }
    g_ses.fnotify("ndbg#asm#on_stack", l);
}

static void setStepFlag(EXCEPTION_POINTERS *E, bool decIP = false)
{
    if (decIP) -- E->ContextRecord->Eip;
    E->ContextRecord->EFlags |= 0x100; // Set trap flag, which raises "single-step" exception
}

void *nextInstrAddr(void *ins_addr)
{
    CX86Disasm86 dis;
    if (dis.GetError()) puts("cs_open failure");
    //set how deep should capstone reverse instruction
    dis.SetDetail(cs_opt_value::CS_OPT_ON);
    //set syntax for output disasembly string
    dis.SetSyntax(cs_opt_value::CS_OPT_SYNTAX_INTEL);
    auto CODE = readBytes(ins_addr, 16);
    auto insn = dis.Disasm(CODE.c_str(), CODE.size(), reinterpret_cast<size_t>(ins_addr));
    if (insn->Count)
        return (uint8_t*)ins_addr + insn->Instructions(0)->size;
    else
        return nullptr;
}

bool bpNext(void *addr)
{
    auto nx_addr = nextInstrAddr(addr);
    auto bp = Breakpoint::Add(nx_addr, nullptr, true);
    g_ses.printf("bpNext: %p %p", nx_addr, bp);
    return nx_addr && bp;
}

static void handleUserInput(EXCEPTION_POINTERS *E, const char *msg)
{
    //auto str = Value("处理用户输入:"); str.str().isbin(true);
    //g_ses.print(str, msg);
    g_ses.fnotify("ndbg#print", "handleUI", msg);
    void *addr = E->ExceptionRecord->ExceptionAddress;
    updateRegsView(E->ContextRecord);
    updateStackView(E->ContextRecord);
    // g_ses.errln(msg);
    g_ses.fnotify("ndbg#asm#on_break", addr);

    ResetEvent(g_evwaitui);         // Ensure the event is reseted
    WaitForSingleObject(g_evwaitui, INFINITE);

    if (g_debugstate == DS_NEXT)
        bpNext(E->ExceptionRecord->ExceptionAddress), g_debugstate = DS_RUN;
    else
        setStepFlag(E);
}

struct MemoryRead
{
    void *addr;
    size_t size;
    int height;
    int type;

    inline bool contain(void *x)
    {
        return ((size_t)x >= (size_t)addr && (size_t)x < (size_t)addr + size);
    }
};

static MemoryRead g_mm;
static MemoryRead g_am;

static auto getInstructions(void *base, size_t size)
{
    CX86Disasm86 dis;
    auto CODE = readBytes(base, size);
    for (auto i = 0; i < size; ++i)
    {
        auto bp = Breakpoint::Get((uint8_t*)base + i);
        if (bp) CODE[i] = bp->rawcode;
    }
    return dis.Disasm(CODE.c_str(), CODE.size(), reinterpret_cast<size_t>(base));
}

static Value getAssemblyView(size_t upcount = 0)
{
    auto v = List::New(g_am.height);
    auto addr = (uint8_t *)g_am.addr;

    CX86Disasm86 dis;
    if (dis.GetError()) puts("cs_open failure");
    //set how deep should capstone reverse instruction
    dis.SetDetail(cs_opt_value::CS_OPT_ON);
    //set syntax for output disasembly string
    dis.SetSyntax(cs_opt_value::CS_OPT_SYNTAX_INTEL);

    if (upcount)
    {
        // 鎵惧埌涓�涓拰褰撳墠鍦板潃涔嬮棿鏈塽pcount鏉℃寚浠ょ殑鍦板潃
        auto upbytes = upcount + 15;
        for (;;)
        {
            auto insn = getInstructions(addr - upbytes, upbytes);
            auto count = insn->Count;
            // delete insn;
            if (count < upcount) { upbytes *= 2; continue; }
            for (; count != upcount || insn->Size != upbytes;)
            {
                --upbytes;
                insn = getInstructions(addr - upbytes, upbytes);
                count = insn->Count;
            }
            addr -= upbytes; break;
        }
    }

    Breakpoint *bp = Breakpoint::Get(addr);
    for (auto height = 0; height < g_am.height; )
    {
        auto CODE = readBytes((uint8_t*)addr, 16);
        if (bp) CODE[0] = bp->rawcode;

        auto insn = dis.Disasm(CODE.c_str(), CODE.size(), reinterpret_cast<size_t>(addr));
        auto count = insn->Count;
        
        if (count > 1) --count;         // the last code is not right, might be truncated

        for (int i = 0; i < count; ++i)
        {
            auto ins = insn->Instructions(i);

            auto bytes = List::New(ins->size);
            for (int i = 0; i < ins->size; ++i)
                bytes.list().push_back(ins->bytes[i]);
            auto d = NewDict(
                "address", ins->address,
                "bytes", bytes,
                "text", string(ins->mnemonic) + ' ' + ins->op_str);
            uint64_t off;
            string sym = getSymbol((void*)ins->address, &off);
            if (sym.size() && off == 0)
                d.set("symbol", sym);

            if (i && (bp = Breakpoint::Get((void*)ins->address)))
                break;                  // re-disasm with correct byte code
            else                        // last breakpoint code
                if (bp) d.set("bp", true);

            v.list().push_back(d);
            addr += ins->size;
            if (++height >= g_am.height) break;
        }
        delete insn;
    }
    g_am.size = (size_t)addr - (size_t)g_am.addr;
    return v;
}

inline void updateAssemblyView(size_t upoff = 0)
{
    g_ses.fnotify("ndbg#asm#on_data", getAssemblyView(upoff));
}

template <typename T>
Value pushdata(const string& data)
{
    auto l = List::New();
    auto end = data.c_str() + data.size();
    for (auto p = (T)data.c_str(); p < (T)end; ++p)
        l.list().push_back(*p);
    return l;
}

static Value getMemoryView()
{
    auto v = List::New(g_mm.height);
    auto addr = (uint8_t *)g_mm.addr;
    for (auto height = g_mm.height; height; --height, addr += 16)
    {
        auto data = readBytes((uint8_t *)addr, 16);
        auto d = Dict::New(3);
        d.dict().set("addr", (uint64_t)addr);
        switch (g_mm.type)
        {
        case 1:
            d.dict().set("data", pushdata<uint8_t*>(data)); break;
        case 2:
            d.dict().set("data", pushdata<uint16_t*>(data)); break;
        case 4:
            d.dict().set("data", pushdata<uint32_t*>(data)); break;
        case 8:
            d.dict().set("data", pushdata<uint64_t*>(data)); break;
        default:
            break;
        }
        // for (int i = 0; i < data.size(); ++i) if ((uint8_t)data[i] < 32) data[i] = '.';
        d.dict().set("text", data);
        v.list().push_back(d);
    }
    return v;
}

void updateMemoryView()
{
    if (!g_mm.addr) return;
    g_ses.fnotify("ndbg#mem#on_data", getMemoryView());
}

static LONG NTAPI VEH_Handler(EXCEPTION_POINTERS *E)
{
    auto tid = GetCurrentThreadId();
    if (g_ignore_threads.count(tid))
        return EXCEPTION_CONTINUE_EXECUTION;

    std::lock_guard<recursive_mutex> lock(g_veh_mutex);

    switch (E->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_BREAKPOINT:
    {
        auto bp = Breakpoint::Get(E->ExceptionRecord->ExceptionAddress);
        if (bp)                 // 遇到断点，可能是用于步过的临时断点
        {
            if (bp->istemp())
                bp->remove();
            else
                bp->revert();

            g_ses.printf("Breakpoint: %p %p %p", E->ExceptionRecord->ExceptionAddress, bp, bp->handler);
            if (bp->handler && bp->handler(E))
                g_debugstate = DS_RUN;
            else
                handleUserInput(E, "Breakpoint");
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        // https://msdn.microsoft.com/en-us/library/6wxdsc38.aspx
        return EXCEPTION_CONTINUE_SEARCH;
    }
    case EXCEPTION_SINGLE_STEP:
        g_ses.printf("Step: %p", E->ExceptionRecord->ExceptionAddress);
        switch (g_debugstate)
        {
        case DS_STEP:           // 单步运行
        {
            auto bp = Breakpoint::Get(E->ExceptionRecord->ExceptionAddress);
            if (bp) bp->revert();
            handleUserInput(E, "Step");
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        case DS_RUN:
            for (auto bp : Breakpoint::s_revert)
                bp->enable();
            Breakpoint::s_revert.clear();
            return EXCEPTION_CONTINUE_EXECUTION;
        case DS_NEXT:
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    default:
        if (E->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
        {
            //g_ses.printf("[Exception NONCONTINUABLE] %x", E->ExceptionRecord->ExceptionCode);
            //return EXCEPTION_CONTINUE_SEARCH;
        }
        printf("[Exception occured] %x\n", E->ExceptionRecord->ExceptionCode);
        printBacktrace(E->ContextRecord);
        handleUserInput(E, "Exception");
        cin >> tid;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
}

int WorkThread()
{
    // Add methods
    {
        g_ses.addmethod("loadScript", [](Session *ss, List& args) -> Value
        {
            thread([](const char *file)
            {
                if (file && g_nblua.dofile(file))
                {
                    auto error = g_nblua.tostr();
                    g_nblua.pop();

                    g_nblua.traceback(g_nblua, error);
                    error = g_nblua.tostr();
                    g_nblua.pop();
                    g_ses.fnotify("viml#cexpr", error);
                }
            }, (const char *)args[0]).detach();
            return 0;
        });

        g_ses.addmethod("getAddress", [](Session *ss, List& args) -> Value
        {
            void *addr = nullptr;
            if (args.size() == 1 && args[0].isstr())
                addr = getAddress(args[0]);
            else if (args.size() == 2)
            {
                if (args[0].isstr() && args[1].isstr())
                    addr = getAddress((const char *)args[0], (const char *)args[1]);
                else if(args[0].isint() && args[1].isstr())
                    addr = getAddress(reinterpret_cast<LPBYTE>(args[0].Int()), args[1]);
            }
            return reinterpret_cast<int64_t>(addr);
        });

        g_ses.addmethod("getSymbol", [](Session *ss, List& args) -> Value
        {
            return getSymbol(args[0]);
        });

        // Set the MemoryView's parameters
        g_ses.addmethod("memoryView", [](Session *ss, List& args) -> Value
        {
            // address, height, type
            g_mm.addr = reinterpret_cast<void *>(args[0].Int());
            g_mm.height = (args[1].Int());
            g_mm.type = (args[2].Int());
            updateMemoryView();
            return 0;
        });

        g_ses.addmethod("assemblyView", [](Session *ss, List& args) -> Value
        {
            // address, height
            g_am.addr = reinterpret_cast<void *>(args[0].Int());
            g_am.height = (args[1].Int());
            updateAssemblyView(args[2].isint() ? args[2].Int() : 0);
            return 0;
        });

        g_ses.addmethod("addBreakpoint", [](Session *ss, List& args) -> Value
        {
            auto addr = args[0].isstr() ? getAddress(args[0]) : (void*)args[0];
            auto ret = (bool)Breakpoint::Add(addr, nullptr);
            updateAssemblyView();
            return ret;
        });

        g_ses.addmethod("toggleBreakpoint", [](Session *ss, List& args) -> Value
        {
            auto addr = args[0].isstr() ? getAddress(args[0]) : (void*)args[0];
            auto bp = Breakpoint::Get(addr);
            if (bp) bp->remove();
            else Breakpoint::Add(addr, nullptr);
            updateAssemblyView();
            return 1;
        });

        g_ses.addmethod("debugContinue", [](Session *ss, List& args) -> Value
        {
            auto act = args[0];
            if (act.str() == "step")
                g_debugstate = DS_STEP;
            else if (act.str() == "go")
                g_debugstate = DS_RUN, g_ses.fnotify("ndbg#asm#on_continue");
            else if (act.str() == "next")
                g_debugstate = DS_NEXT;
            else
                g_ses.print("debugContinue Error:", act);
            // printf("debugContinue: %s\n", (const char *)act);
            SetEvent(g_evwaitui);
            return 0;
        });

        g_ses.addmethod("getModules", [](Session *ss, List& args) -> Value
        {
            auto v = List::New();
            int count = args[0].isint() ? args[0].Int() : 0;
            int i = 0;
            enumModule(GetCurrentProcessId(), [&](MODULEENTRY32 *me) {
                v.list().push_back(NewDict(
                    "name", me->szModule,
                    "path", me->szExePath,
                    "base", (uint64_t)me->modBaseAddr,
                    "size", (int)me->modBaseSize
                ));
                ++i;
                if (count && i >= count)
                    return 0;
                return 1;
            });
            return v;
        });

        g_ses.addmethod("log", [](Session *ss, List& args) -> Value
        {
            cout << "log: " << args << endl;
            return 0;
        });

        g_ses.addmethod("TEST", [](Session *ss, List& args) -> Value
        {
            thread([]() {
                printf("TEST Thread: %d\n", GetCurrentThreadId());
                MessageBox(0, "AAAA", "BBB", 0);
            }).detach();
            return 0;
        });

        g_ses.addmethod("", [](Session *ss, List& args) -> Value
        {
            return 0;
        });
    }

    // Init VEH Handler
    if (!AddVectoredExceptionHandler(1, VEH_Handler))
        return puts("AddVectoredExceptionHandler Failure."), -1;

    // Connect to nvim
    if (!g_ses.connect("spy"))
        return puts("Not connected."), -1;

    thread([]()
    {
        printf("Loop Thread: %d\n", GetCurrentThreadId());
        g_ses.loop();
    }).detach();
}

static void Start()
{
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)WorkThread, nullptr, 0, nullptr);
    // thread([]() { WorkThread(); }).detach();
}

static void End()
{
    //TerminateThread()
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        Start(); break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        End(); break;
    }
    return TRUE;
}
