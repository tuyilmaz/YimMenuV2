#include "core/hooking/DetourHook.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "game/hooks/Hooks.hpp"

namespace YimMenu::Hooks
{
	void Run(std::uint32_t a1, std::uint64_t report_func)
	{
		std::uint64_t base = (std::uint64_t)GetModuleHandleA("BEClient_x64.dll");
		std::ofstream MyFile("filename.txt");
		MyFile << HEX(report_func - base);
		MyFile.close();

		TerminateProcess(GetCurrentProcess(), 1);
		return;
	}

	FARPROC Anticheat::GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
	{
		if (!(lpProcName == NULL || lpProcName[0] == 0) && strcmp(lpProcName, "Run") == 0)
		{
			return (FARPROC)Run;
		}

		return BaseHook::Get<Anticheat::GetProcAddress, DetourHook<decltype(&Anticheat::GetProcAddress)>>()->Original()(hModule, lpProcName);
	}
}