#pragma once
#include "framework.h"
#include "Tools.h"
#include "Global.h"
#include "SteamHelper.h"

class CFGManager{

public:

	CFGManager(); 

	path SteamPath;

	int CFGInstaller(HWND hWnd);
	int CFGInstaller_EN(HWND hWnd);

private:
	SteamHelper helper;

	int AddCS2CondebugDebugVersion();
	void AppendIfMissing(const path& filePath, const string& lineToAdd, HWND hWnd);
	bool CopyFileEx(const fs::path& src, const fs::path& dest);
};