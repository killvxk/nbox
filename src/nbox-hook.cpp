
#include <capstone/capstone.h>
#include <asmjit/asmjit.h>
#include "nbox.h"

#define SIZEOFCALL 5

namespace nbox
{
	struct Trampoline
	{
	};

	extern void fasm_hook_handler();
	extern "C" void * __stdcall nbox_hook_handler(void *hookAddr)
	{
	}

	using namespace asmjit;
	void *genTramp(void *nextAddr)
	{
		JitRuntime rt;                          // Runtime specialized for JIT code execution.
		CodeHolder code;                        // Holds code and relocation information.
		code.init(rt.getCodeInfo());            // Initialize to the same arch as JIT runtime.
		X86Assembler a(&code);                  // Create and attach X86Assembler to `code`.
		a.mov(x86::eax, 1);                     // Move one to 'eax' register.
		a.ret();                                // Return from function.
		void *pTramp;
		Error err = rt.add(&pTramp, &code);         // Add the generated code to the runtime.
		if (err) return nullptr;                      // Handle a possible error returned by AsmJit.
		//rt.release(fn);
		return pTramp;
	}
	// hook any point
	int setHook(void *addr)
	{
		csh handle;
		cs_insn *insn;
		if (cs_open(CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK)
			return -1;
		// 找出足够多的指令来容纳call指令
		auto CODE = readBytes((uint8_t*)addr, 10);
		int count = cs_disasm(handle, (const uint8_t *)CODE.c_str(), CODE.size(), 0x1000, 0, &insn);
		if (count > 0)
		{
			size_t n = 0;
			for (int j = 0; j < count && n < SIZEOFCALL; j++)
				n += insn[j].size;
			cs_free(insn, count);
		}
		else
			return printf("ERROR: Failed to disassemble given code!\n"), -1;
		
		cs_close(&handle);
		return 0;
	}
	// reset the hook
	int resetHook(void *addr)
	{
	}
}
