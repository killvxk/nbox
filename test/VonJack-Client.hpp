#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include "GetHandle.hpp"

#include <iostream>
 
//#define NOBYPASS
#ifdef NOBYPASS
#pragma comment (lib, "ntdll.lib")
#endif
 
#define WITHBENCHMARK
 
#define SHARED_MEM_SIZE 4096

 
using namespace std;
 
extern "C" void SpinLockByte(volatile void* byteAddr, volatile BYTE valueExit);
 
#ifdef NOBYPASS
EXTERN_C NTSTATUS NTAPI NtReadVirtualMemory(HANDLE, PVOID, PVOID, ULONG, PULONG);
EXTERN_C NTSTATUS NTAPI NtWriteVirtualMemory(HANDLE, PVOID, PVOID, ULONG, PULONG);
#endif
 
struct SJORDER {
	DWORD64 exec = 1; // Least significant byte used to release the spinlock, 0 release spinlock in abused process overwritten to 1 after execution
	DWORD order = 0; // 0: Read, 1: Write
	NTSTATUS status = 0xFFFFFFFF; // TODO: Remove
	HANDLE hProcess = NULL;
	DWORD64 lpBaseAddress = NULL;
	SIZE_T nSize = 0;
	SIZE_T* nBytesReadOrWritten = 0; // TODO: Remove
}; // Important: Must be 8 bytes aligned, otherwise garbage data is added in the structure
 
struct SJCFG {
	SIZE_T remoteExecMemSize = NULL;
	void* remoteExecMem = nullptr;
	SIZE_T sharedMemSize = NULL;
	void* ptrRemoteSharedMem = nullptr;
};
 
class VonJack {
public:
	VonJack();
	~VonJack();
 
	// Function to call before use
	bool Init();
 
	// Functions of interest
	HANDLE GetHandle(wstring gameProcessName = L"", bool setAsDefault = true);
	void UseHandle(HANDLE handleID);
	NTSTATUS qRVM(uintptr_t lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead = NULL);
	NTSTATUS qWVM(uintptr_t lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten = NULL);
	NTSTATUS RVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead = NULL);
	NTSTATUS WVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten = NULL);
	uintptr_t Deref(uintptr_t addr, HANDLE hProcess = NULL);
	uintptr_t DerefChain(vector<uintptr_t> ptrChain, bool derefLast = false);
	// Benchmark
	unsigned int countingRVMs = 0, countingWVMs = 0, RVMs = 0, WVMs = 0;
	unsigned int countingRVMc = 0, countingWVMc = 0, RVMc = 0, WVMc = 0;
	void ResetSecond();
	void ResetCycle();
 
	// Static functions
	static vector<DWORD> GetPIDs(wstring targetProcessName);
 
	NTSTATUS RWVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* nBytesReadOrWritten, bool read = true);

protected:
	// Install
	bool Reconnect(HANDLE hLocalSharedMem = NULL);
	bool Disconnect();
 
	// Configuration
	HANDLE m_hMutex = NULL;
	DWORD m_pivotPID = NULL;
	HANDLE m_hHiJack = NULL;
	// Shared memory
	SIZE_T m_usableSharedMemSize = NULL;
	void* m_ptrLocalSharedMem = nullptr;
};
 
VonJack::VonJack() {
 
}
 
VonJack::~VonJack() {
	Disconnect();
}
 
bool VonJack::Disconnect() {
	if (m_ptrLocalSharedMem)
		UnmapViewOfFile(m_ptrLocalSharedMem);
#ifdef NOBYPASS
	if (m_hHiJack)
		CloseHandle(m_hHiJack);
#endif
	return true;
}
 
bool VonJack::Init() {
#ifdef NOBYPASS
	m_usableSharedMemSize = INFINITE;
	return true;
#endif
 
	string e = ""; // TODO Overoptimisation: Randomise names instead of offuscation (need to be unique per each system reboot)
	string mutexNoStr = e + 'G' + 'l' + 'o' + 'b' + 'a' + 'l' + '\\' + 'V' + 'O' + 'N' + 'J' + 'A' + 'C' + 'K' + 'X';
	m_hMutex = CreateMutexA(NULL, TRUE, mutexNoStr.c_str());
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		exit(EXIT_FAILURE); // Security: An instance is already running, terminate now
 
	wstring we = L"";
	wstring lsassNoStr = we + L'l' + L's' + L'a' + L's' + L's' + L'.' + L'e' + L'x' + L'e';
	vector<DWORD> pidsLsass = GetPIDs(lsassNoStr);
	if (pidsLsass.empty())
		return false;
	sort(pidsLsass.begin(), pidsLsass.end()); // In case there is several lsass.exe running (?) take the first one (based on PID)
	m_pivotPID = pidsLsass[0];
	if (!m_pivotPID)
		return false;
 
	// Test if bypass is installed with gatekeeper
	string sharedMemNameNoStr = e+ 'G' + 'l' + 'o' + 'b' + 'a' + 'l' + '\\' + 'V' + 'O' + 'N' + 'J' + 'A' + 'C' + 'K' + 'M';
	HANDLE hLocalSharedMem = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, sharedMemNameNoStr.c_str());
	if (!hLocalSharedMem)
		return false; // Not installed

	return Reconnect(hLocalSharedMem);
}
 
bool VonJack::Reconnect(HANDLE hLocalSharedMem) {
	if (!hLocalSharedMem)
		return false;
	// Remapping shared memory
	m_ptrLocalSharedMem = MapViewOfFile(hLocalSharedMem, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
	CloseHandle(hLocalSharedMem);
	if (!m_ptrLocalSharedMem)
		return false;
 
	// Restoring variables from backup in shared memory
	SJCFG cfgBackup;
	void* endOfUsableLocalSharedMem = (void*)((DWORD64)m_ptrLocalSharedMem + SHARED_MEM_SIZE - sizeof(SJORDER));
	void* backupAddrInSharedMem = (void*)((DWORD64)endOfUsableLocalSharedMem - sizeof(SJCFG));
	CopyMemory(&cfgBackup, backupAddrInSharedMem, sizeof(cfgBackup));
 
	// Quick and dirty consistency check
	if (!cfgBackup.ptrRemoteSharedMem || !cfgBackup.sharedMemSize || !cfgBackup.remoteExecMem || !cfgBackup.remoteExecMemSize || cfgBackup.sharedMemSize != SHARED_MEM_SIZE)
		return false;
	m_usableSharedMemSize = cfgBackup.sharedMemSize - sizeof(SJCFG);
 
	return true;
}
 
vector<DWORD> VonJack::GetPIDs(wstring targetProcessName) {
	vector<DWORD> pids;
	if (targetProcessName == L"")
		return pids;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32W entry;
	entry.dwSize = sizeof entry;
	if (!Process32FirstW(snap, &entry))
		return pids;
	do {
		if (wstring(entry.szExeFile) == targetProcessName) {
			pids.emplace_back(entry.th32ProcessID);
		}
	} while (Process32NextW(snap, &entry));
	return pids;
}
 
NTSTATUS VonJack::RWVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* nBytesReadOrWritten, bool read) {
	if (!lpBuffer || !lpBaseAddress || !nSize || nSize >= m_usableSharedMemSize || !hProcess)
		return (NTSTATUS)0xFFFFFFFF;
	
#ifdef NOBYPASS
		if (read)
			return NtReadVirtualMemory(hProcess, lpBaseAddress, lpBuffer, nSize, (PULONG)nBytesReadOrWritten);
		else
			return NtWriteVirtualMemory(hProcess, lpBaseAddress, lpBuffer, nSize, (PULONG)nBytesReadOrWritten);
#endif
 
	// Preparing order structure
	SJORDER rpmOrder;
	rpmOrder.hProcess = hProcess;
	rpmOrder.lpBaseAddress = (DWORD64)lpBaseAddress;
	rpmOrder.nSize = nSize;
	rpmOrder.nBytesReadOrWritten = nBytesReadOrWritten;
 
	// For write operations, changing order and placing data to write in shared memory
	if (!read) {
		rpmOrder.order = 1;
		CopyMemory(m_ptrLocalSharedMem, lpBuffer, nSize);
	}
 
	// Pushing parameters
	void* controlLocalAddr = (void*)((DWORD64)m_ptrLocalSharedMem + SHARED_MEM_SIZE - sizeof(rpmOrder));
	CopyMemory(controlLocalAddr, &rpmOrder, sizeof(rpmOrder));
 
	// Triggering execution and waiting for completion with the configured synchronisation method
	BYTE exec = 0;
	CopyMemory(controlLocalAddr, &exec, sizeof(exec));
	SpinLockByte(controlLocalAddr, 1);
 
	// Moving from shared memory to lpBuffer and returning
	if (read)
		CopyMemory(lpBuffer, m_ptrLocalSharedMem, nSize);
 
#ifdef WITHBENCHMARK
	if (read) {
		++countingRVMs;
		++countingRVMc;
	} else {
		++countingWVMs;
		++countingWVMc;
	}
#endif
 
	return rpmOrder.status;
}
 
NTSTATUS VonJack::RVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead) {
	return RWVM(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, true);
}
 
NTSTATUS VonJack::WVM(HANDLE hProcess, void* lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten) {
	return RWVM(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten, false);
}
 
uintptr_t VonJack::Deref(uintptr_t addr, HANDLE hProcess) {
	uintptr_t addrPointed = NULL;
	if (!addr) // Invalid memory address given.
		return addrPointed;
	
	if (hProcess)
		RVM(hProcess, (void*)addr, &addrPointed, sizeof(void*));
	else
		RVM(m_hHiJack, (void*)addr, &addrPointed, sizeof(void*));
		
	return addrPointed;
}
 
uintptr_t VonJack::DerefChain(vector<uintptr_t> ptrChain, bool derefLast) {
	uintptr_t addr(0);
 
	for (int i(0); i < ptrChain.size(); ++i) {
		addr += ptrChain[i];
		if ((i + 1) < ptrChain.size() || ((i + 1) == ptrChain.size() && derefLast)) { // If we are asked to also dereference the last offset
			addr = Deref(addr);
		}
	}
	return addr;
}
 
// Quick mode
void VonJack::UseHandle(HANDLE handleID) { m_hHiJack = handleID; }
NTSTATUS VonJack::qRVM(uintptr_t lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead) {
	return RWVM(m_hHiJack, (void*)lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, true);
}
NTSTATUS VonJack::qWVM(uintptr_t lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten) {
	return RWVM(m_hHiJack, (void*)lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten, false);
}
 
HANDLE VonJack::GetHandle(wstring gameProcessName, bool setAsDefault) {
	HANDLE hGame = NULL;
 
#ifdef NOBYPASS
		vector<DWORD> pids = VonJack::GetPIDs(gameProcessName);
		if (pids.empty())
			return (HANDLE)0x0;
		 hGame = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pids[0]);
#else
		hGame = GetHandleIdTo(gameProcessName, m_pivotPID);
#endif
 
	if (setAsDefault)
		UseHandle(hGame);
 
	return hGame;
}
 
void VonJack::ResetSecond() {
	RVMs = countingRVMs;
	WVMs = countingWVMs;
	countingRVMs = 0;
	countingWVMs = 0;
}
 
void VonJack::ResetCycle() {
	RVMc = countingRVMc;
	WVMc = countingWVMc;
	countingRVMc = 0;
	countingWVMc = 0;
}