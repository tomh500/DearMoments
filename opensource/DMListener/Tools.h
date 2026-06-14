#pragma once
#include "framework.h"
#include "DMMain.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <locale>
#include <codecvt>
#include <thread>
#include <chrono>
#include <tlhelp32.h>
#include <sstream>
#include <algorithm> 
#include <cctype> 
#include <psapi.h>
#include "Global.h"
#include <Shlwapi.h>
#include <vector>
#pragma comment(lib, "Kernel32.lib")

using namespace std;
using namespace filesystem;

bool CheckRunPath();
bool FolderExists(const wstring& folderPath);
int StartApps(const wstring& exePath, const wstring& arguments, bool showWindow);
bool IsProcessRunning(const wstring& processName);
void KillProcess(const wstring& processName);
void ClearAutoexec(HWND hWnd);
bool SetWorkingDirectory(LPCWSTR path);
string WString2String(const wstring& wstr);
wstring String2WString(const std::string& str);
bool CopyFile(const wstring& src, const wstring& dst);
//extern "C" __declspec(dllexport) int __cdecl CFGInstaller_Asul();
int CFGInstaller(HWND hWnd);
int StartAppsNew(const path& relativeExePath, const wstring& arguments, bool showWindow);

void RegisterMdFileAssociationForCurrentUser();
void QuitTextGUI();
int CFGInstaller_EN(HWND hWnd);
int GetRegionCode();
void ShowRegionMessage(int regionCode, HWND hWnd);
void CheckForCheatProcesses();
bool IsFirstRun();
void CreateRuleFlag();
void CenterWindow(HWND hwnd);
string WideToUtf8(const wstring& wstr);
wstring Utf8ToWide(const char* str);