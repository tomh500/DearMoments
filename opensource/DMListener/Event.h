#pragma once
#include<iostream>
#include <filesystem>
#include "framework.h"
#include "DMMain.h"
namespace fs = std::filesystem;
void quitDM(HWND hWnd);
void createSetupCfg();
bool CheckRunPath();
bool FolderExists(const std::wstring& folderPath);
bool IsProcessRunning(const std::wstring& processName);
void KillProcess(const std::wstring& processName);
void HandleKillSound(HWND hWnd);
void CloseKillSound(HWND hWnd);
void ClearAndResetBindings(HWND hWnd);
void MusicPlayerExit(HWND hWnd);
//void DisableSmartActiveCmdFiles();
int CheckLcfgExecuted();
//void CleanLockFiles();
void LockSmartActiveFolder();
//void UnLockSmartActiveFolder();
void LockSmartActiveFolderNTFS();
//void UnLockSmartActiveFolderNTFS();
namespace LoadDLLGuard

{
void LoadDLL();
}
	