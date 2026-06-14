#include "AboutDialog.h"
#include "AutoStartViaBatManager.h"
#include "resource.h"
#include "Global.h"
#include <fstream>     
#include <filesystem> 
namespace fs = std::filesystem;
fs::path cachePath = RootPath / L"library" / L"resource" / L"cache";

AboutDialog::AboutDialog(HINSTANCE hInstance, HWND parentWindow, const std::string& appName)
    : m_hInstance(hInstance), m_parentWindow(parentWindow), m_appName(appName) {
}

void AboutDialog::Show() {
    DialogBoxParamA(
        m_hInstance,
        MAKEINTRESOURCEA(IDD_ABOUTBOX),
        m_parentWindow,
        AboutDialog::DialogProc,
        reinterpret_cast<LPARAM>(this)
    );
}

INT_PTR CALLBACK AboutDialog::DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);

        // 直接调用HandleMessage，执行初始化逻辑
        auto* pThis = reinterpret_cast<AboutDialog*>(lParam);
        if (pThis) {
            return pThis->HandleMessage(hDlg, msg, wParam, lParam);
        }
        return TRUE;  // 或FALSE，取决于是否设置焦点
    }

    auto* pThis = reinterpret_cast<AboutDialog*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
    return pThis ? pThis->HandleMessage(hDlg, msg, wParam, lParam) : FALSE;
}

INT_PTR AboutDialog::HandleMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        AutoStartViaBatManager autoStart(m_appName);
        BOOL enabled = autoStart.IsEnabled();

        // 弹窗显示路径和检测结果
        auto batPath = autoStart.GetBatFilePath();
        if (debug)
        {
            wchar_t buf[512];
            swprintf(buf, 512, L"检测启动文件:\n%s\n存在: %d", batPath.wstring().c_str(), enabled);
            MessageBoxW(hDlg, buf, L"调试", MB_OK);
        }
        CheckDlgButton(hDlg, IDC_AUTOSTART_CHECK, enabled ? BST_CHECKED : BST_UNCHECKED);

        HICON hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_DM));
        if (hIcon) {
            SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }


        fs::create_directories(cachePath);

        // 如果文件存在，CheckDlgButton 就会勾选
        CheckDlgButton(hDlg, IDC_QUICKSTOP_CHECK, fs::exists(cachePath / L".userquickstop") ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_TEXTGUI_CHECK, fs::exists(cachePath / L".usertextgui") ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }


    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == IDC_AUTOSTART_CHECK && wmEvent == BN_CLICKED) {
            BOOL checked = IsDlgButtonChecked(hDlg, IDC_AUTOSTART_CHECK);
            AutoStartViaBatManager autoStart(m_appName);
            autoStart.SetEnabled(checked == BST_CHECKED);

            if (debug) {
                char buf[128];
                sprintf_s(buf, "AutoStart set to: %d", checked == BST_CHECKED);
                MessageBoxA(hDlg, buf, "Debug", MB_OK);
            }
            return TRUE;
        }

        // 处理 QuickStop
        if (wmId == IDC_QUICKSTOP_CHECK) {
            bool isChecked = (IsDlgButtonChecked(hDlg, IDC_QUICKSTOP_CHECK) == BST_CHECKED);
            auto qfilePath = cachePath / L".userquickstop";

            if (isChecked) {
                std::ofstream(qfilePath).close(); // 创建文件
            }
            else {
                fs::remove(qfilePath);            // 删除文件
            }
            return TRUE;
        }

        // 3. 专门处理“TextGUI”
        if (wmId == IDC_TEXTGUI_CHECK && wmEvent == BN_CLICKED) {
            bool isChecked = (IsDlgButtonChecked(hDlg, IDC_TEXTGUI_CHECK) == BST_CHECKED);
            auto tfilePath = cachePath / L".usertextgui";
            if (isChecked) {
                std::ofstream(tfilePath).close();
            }
            else
            { fs::remove(tfilePath); }
            return TRUE;
        }

        switch (wmId) {
        case IDOK:
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }


    return FALSE;
}
