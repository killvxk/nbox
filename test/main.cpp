
#include <stdio.h>
#include <assert.h>
#include <iostream>

#include "nbox.h"
#include "VonJack-Client.hpp"

namespace offset
{
	enum {
		GameManager = 0x4DBBA68,
			EntityList = 0xB8,
				Entity = 0x0008,								// Entity Index
					EntityRef = 0x20,
						FeetPosition = 0x1C0,		// diff
						HeadPosition = 0x1E0,		// diff
						EntityInfo = 0x18,
							MainComponent = 0xA0,				// diff
								WeaponComp = 0x40,				// diff
									WeaponProcessor = 0x90,
										Weapon = 0x0,			// Weapon Index
											WeaponInfo = 0x110,
												BulletNum = 0x170,
												Spread = 0x2A0,
												Recoil = 0x2E8,
												Recoil2 = 0x354,
												Recoil3 = 0x304,
												AdsRecoil = 0x330,
								ChildComponent = 0x8,
									Health = 0x118,
						PlayerInfo = 0x2A0,
							PlayerInfoDeref = 0x0,
								PlayerTeamId = 0x148,
								PlayerName = 0x170,
		Renderer = 0x4D602D0,
			GameRenderer = 0x0,
				EngineLink = 0x120,
					Engine = 0x218,
						Camera = 0x38,
							ViewTranslastion = 0x1A0,
							ViewRight = 0x170,
							ViewUp = 0x180,
							ViewForward = 0x190,
							FOVX = 0x1B0,
							FOVY = 0x1C4,
	};
}

using namespace nbox;

PBYTE RSBase = (PBYTE)0x7FF7D5A70000;
PBYTE pGameManager;

PBYTE getEntityRef(int i = 0)
{
	return readPtr(RSBase, {
		offset::GameManager,
		offset::EntityList,
		(int)sizeof(void*) * i,
		offset::EntityRef,
	});
}

void printOffsets(const vector<int>& offsets)
{
	for (auto v : offsets)
		cout << ' ' << hex << v;
	cout << endl;
}

#include "gethandle.hpp"

//#define NOBYPASS
int fuck_rs6()
{
	// enableDebugPrivilege();

#ifdef NOBYPASS
	auto PID = getPid("RainbowSix.exe");
	nbox::hProcess = openProcess(PID);
#else
	static VonJack g_sj;
	assert(g_sj.Init());
	nbox::hProcess = g_sj.GetHandle(L"RainbowSix.exe");
	nbox::ReadMemory = [](HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T * lpNumberOfBytesRead)
	{
		//return g_sj.RVM(hProcess, (void*)lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead) == -1 ? TRUE : FALSE;
		return g_sj.RWVM(hProcess, (void*)lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead), TRUE;
	};
	nbox::WriteMemory = [](HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T * lpNumberOfBytesWrite)
	{
		return g_sj.RWVM(hProcess, lpBaseAddress, (void*)lpBuffer, nSize, lpNumberOfBytesWrite, false) == -1 ? TRUE : FALSE;
	};
#endif
	assert(nbox::hProcess);

	//enumModule(PID, [&](MODULEENTRY32 *me) -> int { RSBase = me->modBaseAddr; return 0; });
	//enumModule(hProcess, [&](HMODULE hMod, const char *name) -> int {
	//	if (!_stricmp(name, "RainbowSix.exe")) return RSBase = (PBYTE)hMod, 0;
	//	return 1;
	//});

	pGameManager = readPtr(RSBase, { offset::GameManager });
	printf("RSBase: %p pGameManager: %p\n", RSBase, pGameManager);

	for (int i = 0; i < 3; ++i)
	{
		auto pEntityRef = getEntityRef(i);
		auto pWeaponInfo = readPtr(pEntityRef, {
			offset::EntityInfo,
			offset::MainComponent,
			offset::WeaponComp,
			offset::WeaponProcessor,
			offset::Weapon,
			offset::WeaponInfo
		});
		auto pPlayerInfoDeref = readPtr(pEntityRef, {
			offset::PlayerInfo,
			offset::PlayerInfoDeref,
		});
		auto pCamera = readPtr(RSBase, {
			offset::Renderer,
			offset::GameRenderer,
			offset::EngineLink,
			offset::Engine,
			offset::Camera,
		});

		int bullet;
		readValue(pWeaponInfo + offset::BulletNum, bullet);
		printf("pWeaponInfo: %p\n", pWeaponInfo); printf("BulletNum: %d\n", bullet);
		printf("pEntityRef: %p\n", pEntityRef);
		printf("pCamera: %p\n", pCamera);

		cout << "Player Name: "
			 << readString(readPtr(pPlayerInfoDeref + offset::PlayerName)) << endl;

		// 子弹数量
		// bullet = 100; writeValue(pWeaponInfo + offset::BulletNum, bullet);
		// 修改后坐力
		// float recoil = 0.0; writeValue(pWeaponInfo + offset::Recoil, recoil);

		//float x, y, z;
		//readValue(pEntityRef + offset::FeetPosition, x);
		//printf("x: %g\n", x);
	}
	return 0;
}



int fuck_codol()
{
	nbox::hProcess = openProcess(0x11E4);
	inject("C:\\Users\\xavier\\source\\repos\\Dll2\\Debug\\Dll2.dll");
	return 0;
}

int main()
{
	//char buf[] = { 1,2,3 };
	//printf("%d\n", WriteProcessMemory(GetCurrentProcess(), (LPVOID)&main, buf, sizeof(buf), nullptr));
	//return fuck_rs6();
	return fuck_codol();
}