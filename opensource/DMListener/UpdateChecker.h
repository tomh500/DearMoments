#pragma once
#include "UpdateChecker.h"
#include <windows.h>

void CheckForUpdate(HWND hWnd);
int  ShowCloudMsg(HWND hWnd);
void PerformHotUpdate(HWND hWnd, bool needsUpdate);
extern bool HotUpdate_req;