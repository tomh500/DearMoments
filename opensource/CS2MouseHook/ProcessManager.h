#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>


namespace ProcessManager {
	bool KillProcess(const std::wstring& processName);
}