#ifndef __NBOX_H__
#define __NBOX_H__

#include <stdint.h>
#include <string>
#include <initializer_list>
#include <functional>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

#pragma comment(lib, "Advapi32.lib")

namespace nbox
{
	typedef BOOL (__stdcall *READ_MEMORY)(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, size_t *lpNumberOfBytesRead);
	typedef BOOL (__stdcall *WRITE_MEMORY)(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, size_t *lpNumberOfBytesWrite);
    
	extern HANDLE hProcess;
	extern READ_MEMORY ReadMemory;
	extern WRITE_MEMORY WriteMemory;

	using namespace std;

	struct Range
	{
		Range(int b) { begin = b, end = b; }
		Range(const initializer_list<int>& l)
		{
			auto it = l.begin();
			begin = *it++; end = *it;
		}
		int begin, end;
	};

	template<typename T>
	bool readValue(uint8_t *addr, T& ret)
	{
		size_t nReaded;
		//return ReadMemory(hProcess, addr, (LPVOID)&ret, sizeof(ret), &nReaded) && nReaded == sizeof(T);
		return ReadMemory(hProcess, addr, (LPVOID)&ret, sizeof(ret), &nReaded);
	}

	int8_t readByte(uint8_t *addr);
	int16_t readShort(uint8_t *addr);
	int32_t readInt(uint8_t *addr);
	int64_t readLong(uint8_t *addr);
	uint8_t *readPtr(uint8_t *addr);
	uint8_t *readPtr(uint8_t *addr, const initializer_list<int>& offsets);
	string readString(uint8_t *addr);
	string readBytes(uint8_t *addr, size_t size);
	void enumPtrs(uint8_t *addr, const initializer_list<Range> ranges, function<void(uint8_t*, vector<int>&)>);
	LPBYTE getAddress(LPBYTE lpMod, const char *func);
	void *getAddress(const char *lib, const char *func);

	template<typename T>
	bool writeValue(uint8_t *addr, T& val)
	{
		size_t nWritten;
		//return WriteMemory(hProcess, addr, &val, sizeof(T), &nWritten) && nWritten == sizeof(T);
		return WriteMemory(hProcess, addr, &val, sizeof(T), &nWritten);
	}
	bool writeBytes(uint8_t *addr, const char *bytes, size_t len);
	bool writeBytes(uint8_t *addr, const string& bytes);

	bool enablePrivilege(const char *priv, bool enable = true);
	bool enableDebugPrivilege();

	string errorString(int errcode);
	string lastError();

	DWORD getPid(const string& name);
	DWORD getPid(HWND hWnd);

	HANDLE openProcess(const string& name, DWORD access = PROCESS_ALL_ACCESS);
	HANDLE openProcess(HWND hWnd, DWORD access = PROCESS_ALL_ACCESS);
	HANDLE openProcess(DWORD pid, DWORD access = PROCESS_ALL_ACCESS);

	int enumProcess(function<int(PROCESSENTRY32 *)> callback);
	int enumModule(DWORD pid, function<int(MODULEENTRY32 *)> callback);
	int enumModule(HANDLE hProc, function<int(HMODULE, const char*)> callback);

	int inject(const string& szDllPath);
}

#endif /* __NBOX_H__ */
