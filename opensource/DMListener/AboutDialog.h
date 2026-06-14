#pragma once
#include <windows.h>
#include <string>

class AboutDialog {
public:
    AboutDialog(HINSTANCE hInstance, HWND parentWindow, const std::string& appName);
    void Show();

private:
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hInstance;
    HWND m_parentWindow;
    std::string m_appName;
};
