#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <iostream>
extern bool isInsertLocked;

void CheckForCheatProcesses();
bool LockInsertKey(HWND hwnd);
void UnlockInsertKey(HWND hwnd);

void ToggleInsertLock(HWND hwnd);