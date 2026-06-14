// CustomAboutDialog.cpp
// 实现一个带GDI绘制效果的 "关于" 对话框
#include <windows.h>
#include "resource.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;


// 对话框消息处理函数
INT_PTR CALLBACK CustomAboutDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 将按钮设置为 Owner-Draw
        HWND hBtn = GetDlgItem(hDlg, IDOK);
        LONG_PTR style = GetWindowLongPtr(hBtn, GWL_STYLE);
        SetWindowLongPtr(hBtn, GWL_STYLE, style | BS_OWNERDRAW);
        return TRUE;
    }
    case WM_ERASEBKGND: {
        // 绘制渐变背景
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hDlg, &rc);
        // 使用渐变填充
        TRIVERTEX vert[2];
        vert[0].x = rc.left;
        vert[0].y = rc.top;
        vert[0].Red = 0xFFF0;  // 白色起点
        vert[0].Green = 0xFFF0;
        vert[0].Blue = 0xFFF0;
        vert[0].Alpha = 0x0000;
        vert[1].x = rc.right;
        vert[1].y = rc.bottom;
        vert[1].Red = 0xA0C0;  // 浅蓝终点
        vert[1].Green = 0xC0E0;
        vert[1].Blue = 0xFFFF;
        vert[1].Alpha = 0x0000;
        GRADIENT_RECT gRect = { 0,1 };
        GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        SetTextColor(hdcStatic, RGB(50, 50, 50));
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlID == IDOK) {
            // 绘制自定义按钮
            RECT rc = pdis->rcItem;
            HDC hdc = pdis->hDC;
            // 选择颜色
            COLORREF bg = (pdis->itemState & ODS_SELECTED) ? RGB(0, 120, 215) : RGB(0, 153, 255);
            HBRUSH hBrush = CreateSolidBrush(bg);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // 边框
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 102, 204));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            // 文本居中
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            wchar_t text[16];
            GetWindowTextW(pdis->hwndItem, text, _countof(text));
            DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
