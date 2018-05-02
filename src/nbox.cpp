
#include "nbox.h"

namespace nbox
{
	using namespace std;

	HANDLE hProcess = GetCurrentProcess();
	READ_MEMORY ReadMemory = (READ_MEMORY)::ReadProcessMemory;
	WRITE_MEMORY WriteMemory = (WRITE_MEMORY)::WriteProcessMemory;

	//int8_t readByte(uint8_t *addr) { return readValue<int8_t>(addr); }
	//int16_t readShort(uint8_t *addr) { return readValue<int16_t>(addr); }
	//int32_t readInt(uint8_t *addr) { return readValue<int32_t>(addr); }
	//int64_t readLong(uint8_t *addr) { return readValue<int64_t>(addr); }
	uint8_t *readPtr(uint8_t *addr)
	{
		uint8_t *ptr;
		return readValue(addr, ptr) ? ptr : nullptr;
	}

	string readString(uint8_t *addr)
	{
		const int BUFSIZE = 128;
		char buf[BUFSIZE + 1] = { 0 };
		size_t readed;
		string ret;

		for (int l = ret.size(); ; l = ret.size())
		{
			if (!ReadMemory(hProcess, addr, buf, BUFSIZE, &readed))
				break;
			if (ret.append(buf).size() - l < readed)
				break;
		}
		return ret;
	}

	string readBytes(uint8_t *addr, size_t len)
	{
		string buf(len, 0); size_t nReaded;
		return ReadMemory(hProcess, addr, (LPVOID)buf.c_str(), len, &nReaded) && nReaded == len ? buf : "";
	}
	bool readBytes(uint8_t *addr, void *pdata, size_t len)
	{
		size_t nReaded;
		return ReadMemory(hProcess, addr, pdata, len, &nReaded) && nReaded == len;
	}

	uint8_t *readPtr(uint8_t *addr, const initializer_list<int>& offsets)
	{
		auto ptr = addr;
		for (auto offset : offsets)
		{
			if (!ptr) break;
			ptr = readPtr(ptr + offset);
		}
		return ptr;
	}

	bool writeBytes(uint8_t *addr, const char *bytes, size_t len)
	{
		size_t nWritten;
		return WriteMemory(hProcess, addr, bytes, len, &nWritten) && nWritten == len;
	}

	bool writeBytes(uint8_t *addr, const string& bytes)
	{
		return writeBytes(addr, bytes.c_str(), bytes.size());
	}

	DWORD getPid(const string& name)
	{
		DWORD pid = 0;
		enumProcess([&](PROCESSENTRY32 *pe) -> int {
			if (!_stricmp(pe->szExeFile, name.c_str()))
				return pid = pe->th32ProcessID, 0;
			return 1;
		});
		return pid;
	}
	DWORD getPid(HWND hWnd)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(hWnd, &pid);
		return pid;
	}

	HANDLE openProcess(const string& name, DWORD access)
	{
		return openProcess(getPid(name), access);
	}
	HANDLE openProcess(HWND hWnd, DWORD access)
	{
		return openProcess(getPid(hWnd), access);
	}
	HANDLE openProcess(DWORD pid, DWORD access) { return OpenProcess(access, FALSE, pid); }

	string errorString(int errcode)
	{
        HLOCAL str; string ret;
        if (FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, errcode, 0, (LPSTR)&str, 0, 0))
            ret += (char *)str;
        LocalFree(str);
        return ret;
	}
	string lastError() { return errorString(GetLastError()); }


	bool enablePrivilege(const char *priv, bool enable)
	{ 
		HANDLE hToken; TOKEN_PRIVILEGES tp; LUID luid;

		if(!::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
			return false;

		if(!::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
			return ::CloseHandle(hToken), false;

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

		if(!::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL))
			return ::CloseHandle(hToken), false;

		::CloseHandle(hToken);

		if(::GetLastError() == ERROR_NOT_ALL_ASSIGNED)
			return false;
		return true;
	}
	bool enableDebugPrivilege() { return enablePrivilege(SE_DEBUG_NAME); }


    int enumProcess(function<int (PROCESSENTRY32 *)> callback)
	{
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(pe32);

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return 0;

        for (BOOL b = Process32First(hSnap, &pe32);
			b && callback(&pe32); 
			b = Process32Next(hSnap, &pe32));
        CloseHandle(hSnap);
        return 1;
    }
	int enumModule(DWORD pid, function<int(MODULEENTRY32 *)> callback)
	{
        MODULEENTRY32 me32;
        me32.dwSize = sizeof(me32);

        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (hProcessSnap == INVALID_HANDLE_VALUE)
            return 0;

        for (BOOL bResult = Module32First(hProcessSnap, &me32); bResult && callback(&me32);
			bResult = Module32Next(hProcessSnap, &me32));
        CloseHandle(hProcessSnap);
        return 1;
	}

#include <Psapi.h>

	int enumModule(HANDLE hProc, function<int(HMODULE, const char*)> callback)
	{
		HMODULE hMods[1024]; DWORD cbNeeded;
		if (!EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded))
			return 0;
		for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			char szModName[MAX_PATH];
			if (!GetModuleFileNameEx(hProc, hMods[i], szModName, sizeof(szModName)))
				continue;
			if (!callback(hMods[i], szModName))
				break;
		}
		return 1;
	}

	typedef function<void(uint8_t*, int)> ENUM_PTRS;

	void enumPtrs(uint8_t *addr, const initializer_list<Range> ranges, function<void(uint8_t*, vector<int>&)> callback)
	{
		ENUM_PTRS EnumPointers;
		vector<Range> rgs(ranges);		// offset range of per level pointer
		vector<int> offsets(rgs.size());// midlle state of offsets

		EnumPointers = [&](uint8_t *addr, int il = 0) {
			auto& range = rgs[il];
			for (int offset = range.begin; offset <= range.end; offset += sizeof(void*))
			{
				offsets[il] = offset;
				auto ptr = readPtr(addr + offset);
				if (!ptr) continue;
				if (il + 1 >= rgs.size() && ptr)
					callback(ptr, offsets);
				else
					EnumPointers(ptr, il + 1);
			}
		};
		EnumPointers(addr, 0);
	}

	int inject(const string& szDllPath)
	{
		HANDLE hThread = NULL;
		HMODULE hMod = NULL;
		LPVOID pRemoteBuf = NULL;  // 存储在目标进程申请的内存地址  
		DWORD dwBufSize = szDllPath.size() + 1;  // 存储DLL文件路径所需的内存空间大小  
		LPTHREAD_START_ROUTINE pThreadProc;

		pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);  // 在目标进程空间中申请内存  

		WriteMemory(hProcess, pRemoteBuf, (LPVOID)szDllPath.c_str(), dwBufSize, NULL);  // 向在目标进程申请的内存空间中写入DLL文件的路径  

		hMod = GetModuleHandle("kernel32.dll");
		//pThreadProc = (LPTHREAD_START_ROUTINE)GetProcAddress(hMod, "LoadLibraryA");  // 获得LoadLibrary()函数的地址  
		pThreadProc = (LPTHREAD_START_ROUTINE)0x75945980;  // 获得LoadLibrary()函数的地址  

		hThread = CreateRemoteThread(hProcess, NULL, 0, pThreadProc, pRemoteBuf, 0, NULL);

		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		CloseHandle(hProcess);

		return TRUE;
	}

	LPBYTE getAddress(LPBYTE lpMod, const char *func)
	{
		LPBYTE result = nullptr;
		IMAGE_DOS_HEADER DOS;
		IMAGE_EXPORT_DIRECTORY EXP;
		LPBYTE lpExp, lpNames, lpFuncs, lpOrdianls;

		union
		{
			IMAGE_NT_HEADERS64 NT64;
			IMAGE_NT_HEADERS32 NT32;
		};
		if (!readBytes(lpMod, &DOS, sizeof(DOS)))
			goto RETURN;
		if (!readBytes(lpMod + DOS.e_lfanew, &NT64, sizeof(NT64)))
			goto RETURN;
		lpExp = lpMod + (NT64.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC ?
			NT32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress :
			NT64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

		if (!readBytes(lpExp, &EXP, sizeof(EXP)))
			goto RETURN;
		lpNames = lpMod + EXP.AddressOfNames,
			lpFuncs = lpMod + EXP.AddressOfFunctions,
			lpOrdianls = lpMod + EXP.AddressOfNameOrdinals;

		if ((DWORD)func & 0xFFFF0000)					// By Function name
		{
			int len = strlen(func), i = 0;
			char *buf = (char *)malloc(len + 1);
			DWORD va = 0; USHORT sn = 0;
			for (; i < EXP.NumberOfNames; i++)
			{
				if (!readBytes(lpNames + i * sizeof(va), &va, sizeof(va)))
					goto RETURN;
				if (!readBytes(lpMod + va, buf, len))
					goto RETURN;
				buf[len] = '\0';
				if (!strcmp(buf, func)) break;
			}
			free(buf);
			if (i == EXP.NumberOfFunctions) goto RETURN;
			if (!readBytes(lpOrdianls + i * sizeof(sn), &sn, sizeof(sn)))
				goto RETURN;
			if (readBytes(lpFuncs + sn * sizeof(va), &va, sizeof(va)))
				result = lpMod + va;

		}
		else											// By Numberic
		{
			DWORD va;
			if (readBytes(lpFuncs + (DWORD)func * sizeof(va), &va, sizeof(va)))
				result = lpMod + va;
		}

	RETURN:
		return result;
	}

	void *getAddress(const char *lib, const char *func)
	{
		HMODULE base = nullptr;
		enumModule(hProcess, [&](HMODULE hMod, const char *name) {
			if (_stricmp(name, func) == 0)
				return base = hMod, 0;
			return 1;
		});
		return base ? getAddress((LPBYTE)base, func) : nullptr;
	}
}
