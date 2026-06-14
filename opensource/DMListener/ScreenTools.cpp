#include "ScreenTools.h"
#include "Head.h"
#include <gdiplus.h>
#include "Global.h"
#pragma comment(lib, "gdiplus.lib")

// 添加控件ID定义
#define IDC_RES_LIST    5001
#define IDC_RATE_COMBO  5002
#define IDC_APPLY_BTN   5003

using namespace std;
using namespace Gdiplus;

// 全局状态
static HINSTANCE                       g_hInst = nullptr;
static ULONG_PTR                       g_gdiplusToken = 0;
static const wchar_t                   g_szChildClass[] = L"ChildResWndClass";
static map<pair<int, int>, vector<int>> g_modes;

// 枚举所有显示模式并按刷新率降序排序
static void EnumAllDisplayModes() {
    DEVMODE dm = {};
    dm.dmSize = sizeof(dm);
    for (int i = 0; EnumDisplaySettings(nullptr, i, &dm); i++) {
        auto key = make_pair(dm.dmPelsWidth, dm.dmPelsHeight);
        auto& vec = g_modes[key];
        if (find(vec.begin(), vec.end(), dm.dmDisplayFrequency) == vec.end())
            vec.push_back(dm.dmDisplayFrequency);
    }
    for (auto& kv : g_modes)
        sort(kv.second.begin(), kv.second.end(), greater<int>());
}

// 填充分辨率列表（按面积降序）
static void FillResList(HWND hList) {
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    vector<pair<int, int>> all;
    for (auto& kv : g_modes) all.push_back(kv.first);
    sort(all.begin(), all.end(), [](auto& a, auto& b) {
        return (long)a.first * a.second > (long)b.first * b.second;
        });
    for (auto& r : all) {
        wostringstream ss;
        ss << r.first << L"x" << r.second;
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)ss.str().c_str());
    }
}

// 填充刷新率下拉列表
static void FillRateCombo(HWND hCombo, const pair<int, int>& res) {
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    for (int hz : g_modes[res]) {
        wostringstream ss;
        ss << hz << L" Hz";
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)ss.str().c_str());
    }
    if (!g_modes[res].empty())
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

// 默认选中当前分辨率与刷新率
static void SelectCurrentMode(HWND hResList, HWND hRateCombo) {
    DEVMODE cur = {};
    cur.dmSize = sizeof(cur);
    if (!EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &cur))
        return;

    // 1. 选分辨率
    wchar_t resTxt[32] = {};
    swprintf_s(resTxt, L"%dx%d", cur.dmPelsWidth, cur.dmPelsHeight);
    int cnt = SendMessage(hResList, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < cnt; i++) {
        wchar_t item[32] = {};
        SendMessage(hResList, LB_GETTEXT, i, (LPARAM)item);
        if (wcscmp(item, resTxt) == 0) {
            SendMessage(hResList, LB_SETCURSEL, i, 0);
            FillRateCombo(hRateCombo, { cur.dmPelsWidth,cur.dmPelsHeight });
            break;
        }
    }

    // 2. 选刷新率
    wchar_t rateTxt[32] = {};
    swprintf_s(rateTxt, L"%d Hz", cur.dmDisplayFrequency);
    int rc = SendMessage(hRateCombo, CB_GETCOUNT, 0, 0);
    for (int j = 0; j < rc; j++) {
        wchar_t item[32] = {};
        SendMessage(hRateCombo, CB_GETLBTEXT, j, (LPARAM)item);
        if (wcscmp(item, rateTxt) == 0) {
            SendMessage(hRateCombo, CB_SETCURSEL, j, 0);
            break;
        }
    }
}

// 子窗口消息过程
LRESULT CALLBACK ScreenWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hResList = nullptr, hRateCombo = nullptr, hBtn = nullptr;
    switch (msg) {
    case WM_CREATE: {
        EnumAllDisplayModes();
        // 列表、下拉、按钮
        hResList = CreateWindow(
            L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY
            | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
            10, 40, 200, 200,
            hWnd, (HMENU)IDC_RES_LIST, g_hInst, nullptr
        );
        hRateCombo = CreateWindow(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            220, 40, 100, 200, hWnd, (HMENU)IDC_RATE_COMBO, g_hInst, nullptr);
        hBtn = CreateWindow(L"BUTTON", L"应用",
            WS_CHILD | WS_VISIBLE,
            220, 250, 100, 30, hWnd, (HMENU)IDC_APPLY_BTN, g_hInst, nullptr);

        FillResList(hResList);
        SelectCurrentMode(hResList, hRateCombo);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), evt = HIWORD(wp);
        if (id == IDC_RES_LIST && evt == LBN_SELCHANGE) {
            int idx = SendMessage(hResList, LB_GETCURSEL, 0, 0);
            if (idx >= 0) {
                wchar_t buf[32] = {};
                SendMessage(hResList, LB_GETTEXT, idx, (LPARAM)buf);
                int w, h; swscanf_s(buf, L"%dx%d", &w, &h);
                FillRateCombo(hRateCombo, { w,h });
            }
        }
        else if (id == IDC_APPLY_BTN) {
            int ridx = SendMessage(hResList, LB_GETCURSEL, 0, 0);
            int cidx = SendMessage(hRateCombo, CB_GETCURSEL, 0, 0);
            if (ridx < 0 || cidx < 0) {
                MessageBox(hWnd, L"请先选择", L"提示", MB_OK | MB_ICONINFORMATION);
                break;
            }
            // 解析
            wchar_t buf[32] = {};
            SendMessage(hResList, LB_GETTEXT, ridx, (LPARAM)buf);
            int newW, newH; swscanf_s(buf, L"%dx%d", &newW, &newH);
            SendMessage(hRateCombo, CB_GETLBTEXT, cidx, (LPARAM)buf);

            int newHz; swscanf_s(buf, L"%d Hz", &newHz);

            DEVMODE dm = {}; dm.dmSize = sizeof(dm);
            dm.dmPelsWidth = newW; dm.dmPelsHeight = newH;
            dm.dmDisplayFrequency = newHz;
            dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

            LONG ret = ChangeDisplaySettingsEx(
                nullptr, &dm, nullptr,
                CDS_UPDATEREGISTRY | CDS_FULLSCREEN,
                nullptr);
            if (debug) {
                if (ret == DISP_CHANGE_SUCCESSFUL)
                {
                    MessageBox(hWnd, L"切换成功", L"成功", MB_OK | MB_ICONINFORMATION);
                }
              
        else {
                    MessageBox(hWnd, L"切换失败", L"错误", MB_OK | MB_ICONERROR); }
            }
        }
        break;
    }
    case WM_NCHITTEST: {
        // 防止通过边框调整窗口大小
        LRESULT hit = DefWindowProc(hWnd, msg, wp, lp);
        if (hit == HTLEFT || hit == HTRIGHT || hit == HTTOP || hit == HTTOPLEFT ||
            hit == HTTOPRIGHT || hit == HTBOTTOM || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT) {
            return HTBORDER;  // 将调整大小的区域改为边框
        }
        return hit;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        Graphics g(hdc);

        RECT rc; GetClientRect(hWnd, &rc);
        SolidBrush bg(Color(255, 240, 240, 240));
        g.FillRectangle(&bg, Rect(0, 0, rc.right, rc.bottom));

        LinearGradientBrush hdr(
            PointF(0, 0), PointF((REAL)rc.right, 0),
            Color(255, 0, 120, 215), Color(255, 0, 80, 200));
        g.FillRectangle(&hdr, Rect(0, 0, rc.right, 30));

        FontFamily ff(L"Segoe UI");
        Font fnt(&ff, 12, FontStyleBold, UnitPixel);
        SolidBrush txt(Color(255, 255, 255, 255));
        g.DrawString(L"分辨率工具", -1, &fnt, PointF(8, 6), &txt);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
        DestroyWindow(hWnd);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wp, lp);
    }
    return 0;
}

// 对外接口
namespace ScreenToolsProxy {

    void RegisterScreen(HINSTANCE hInst) {
        g_hInst = hInst;
        WNDCLASSEX wc = { sizeof(wc) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ScreenWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = g_szChildClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
    }

    void ShowScreenTools(HWND /*ignored*/) {
        HWND h = FindWindow(g_szChildClass, nullptr);
        if (h) {
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
            return;
        }
        int w = 350, hh = 320;

        // 修改窗口样式：移除可调整大小的边框
        DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE;

        HWND hNew = CreateWindowEx(
            WS_EX_TOOLWINDOW,
            g_szChildClass, L"分辨率工具",
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            w, hh,
            nullptr, nullptr, g_hInst, nullptr);
        if (!hNew) return;

        HRGN rgn = CreateRoundRectRgn(0, 0, w, hh, 16, 16);
        SetWindowRgn(hNew, rgn, TRUE);

        ShowWindow(hNew, SW_SHOW);
        UpdateWindow(hNew);
    }

} // namespace