// DM监听器.cpp : 定义应用程序的入口点。
//
//#define SDL_STATIC 
#define SDL_MAIN_HANDLED
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#pragma warning(disable: 5262) // 针对 fallthrough 的具体警告编号
#pragma warning(disable: 26819) //分析警告
#define _HAS_STD_BYTE 0
#include "framework.h"
#include "DMMain.h"
#include <commctrl.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <shlobj.h>
#include <curl/curl.h>
#include "UpdateChecker.h"
#include <shellapi.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#include <Richedit.h>
#define MAX_LOADSTRING 100
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "SharedMem.h"
#include "MusicHead.h"
bool g_HasReadProtocol = false;
int regionCode = GetRegionCode();
int debug = 0;
//int LocalVersion = 14500;  // 你的当前版本号

// 全局变量:
HINSTANCE hInst;
ULONG_PTR gdiplusToken;
ULONG_PTR g_diplusToken;
HBITMAP   g_hBackgroundBitmap = nullptr;
HDC       g_hdcBackgroundMem = nullptr;

WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HANDLE hMutex = NULL;
HWND hEdit = NULL;  // 在窗口过程外（文件顶部或类成员）
WNDPROC g_OldEditProc = nullptr;
static bool g_VerProductOK = false;
static bool g_VerCoreOK = false;
static bool g_VerCommOK = false;
bool KCore::QuickStop = false;
bool ShowTextGUI=false;

int localVersion = -1;


// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
MP_WNDPROC MusicPlayerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);



LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        // 计算编辑框相对于父窗口的坐标
        POINT pt = { 0, 0 };
        MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);

        // 绘制背景图
        if (g_hdcBackgroundMem)
        {
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_hdcBackgroundMem, pt.x, pt.y, SRCCOPY);
        }
        else
        {
            FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        }

        {
            Gdiplus::Graphics graphics(hdc);
            Gdiplus::SolidBrush brush(Gdiplus::Color(120, 50, 50, 50));
            graphics.FillRectangle(&brush, 0, 0, rc.right, rc.bottom);
        }

        // 调用默认绘制文本和光标（重点）
        // 注意用 GetDC + ValidateRect 防止闪烁
        {
            // 创建一个内存DC用来绘制文本和光标
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmMem);

            // 调用默认窗口过程绘制文本和光标到内存DC
            CallWindowProc(g_OldEditProc, hWnd, WM_PAINT, (WPARAM)hdcMem, 0);

            // 把内存DC内容复制到显示DC
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

            // 释放内存DC资源
            SelectObject(hdcMem, hOldBmp);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_PRINTCLIENT:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);

        POINT pt = { 0, 0 };
        MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);

        if (g_hdcBackgroundMem)
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_hdcBackgroundMem, pt.x, pt.y, SRCCOPY);
        else
            FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

        Gdiplus::Graphics graphics(hdc);
        Gdiplus::SolidBrush brush(Gdiplus::Color(120, 50, 50, 50));
        graphics.FillRectangle(&brush, 0, 0, rc.right, rc.bottom);

        // 让系统绘制文本和光标
        return CallWindowProc(g_OldEditProc, hWnd, WM_PRINTCLIENT, wParam, lParam);
    }

    default:
        return CallWindowProc(g_OldEditProc, hWnd, msg, wParam, lParam);
    }
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{

    PublishVersionFromFile(RootPath);
    KCore::QuickStop = fs::exists(RootPath / L"library" / L"resource" / L"cache" / ".userquickstop");
    ShowTextGUI== fs::exists(RootPath / L"library" / L"resource" / L"cache" / ".usertextgui");
    ShareSettingPath();
    //  ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);


    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    //SetWorkingDirectory(L"..\\..\\..");

      // 创建一个唯一的互斥体名称
    const std::wstring mutexName = L"Global\\DearMomentsAppMutex";
    // 尝试创建一个名为 "DearMomentsAppMutex" 的互斥体
    hMutex = CreateMutexW(NULL, TRUE, mutexName.c_str());

    if (hMutex == NULL)
    {
        // 创建互斥体失败，说明可能没有足够的权限或者系统出了问题
        MessageBoxW(NULL, L"创建互斥体失败！", L"错误", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    // 检查互斥体是否已经存在，意味着程序已经在运行
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(NULL, L"DearNextgen已经在运行！\nApp has been run", L"错误", MB_OK | MB_ICONERROR);
        CloseHandle(hMutex); // 关闭互斥体句柄
        return FALSE;
    }

    if (debug != 1)
    {
        CheckRunPath(); // 检查运行路径
    }
    createSetupCfg();   // 创建cfg文件

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DM, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    // 注册音乐播放器窗口类
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MusicPlayerWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);      // 任务栏图标
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);        // 鼠标形状
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);   // 背景色
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"MusicPlayerWindowClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);    // 小图标
    ScreenToolsProxy::RegisterScreen(hInstance);
    RegisterClassEx(&wc);
    //   path EncryptKeyPath = current_path() / L"SQencrypt" / L"encrypt.sq";
     //  if (!ExtractResource(IDR_ENCRYPT_KEY, EncryptKeyPath)) { MessageBoxW(NULL, L"未能提取key", L"未能提取key", MB_OK | MB_ICONERROR); }
      // StartAppsNew(RootPath / L"src" / L"CS2" / L"lib" / "SQEncrypt" / "decrypt.exe", L"-p ..\\main\\DM\\Features -k SQEncrypt\\encrypt.sq", true);

       //MessageBoxW(NULL, L"请不要退出程序 正在解密CFG，如果此时退出，可能导致意想不到的后果，甚至无法正常使用,等一切结束，我们会通知您", L"请耐心等待", MB_OK | MB_ICONQUESTION);
     //  StartAppsNew(L"DearMomentsViewer", L"", false);



    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DM));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // --- 这里的代码会在窗口关闭、消息循环退出后执行 ---
    g_threadExit = true; // 确保循环标志已置为 true

    // 强行 join 所有可能遗留的线程对象
    if (g_fetchThread.joinable())   g_fetchThread.join();
    if (g_refreshThread.joinable()) g_refreshThread.join();
    if (g_workThread.joinable())    g_workThread.join();

    // 退出时关闭互斥体句柄
    CloseHandle(hMutex);

    return (int)msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DM));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DM);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//

void AddRootToDefenderExclusion() {
    // 1. 声明并初始化 target 变量
    // 这里利用 filesystem 获取绝对路径，并转为 Windows 风格的 wstring
    std::wstring target = std::filesystem::absolute(RootPath).make_preferred().wstring();

    // 2. 构造命令字符串
    std::wstring cmdParams = L"-WindowStyle Hidden -Command \"Add-MpPreference -ExclusionPath '" + target + L"'\"";

    // 3. 执行提权命令
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW); // 规范写法，填入结构体大小
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"powershell.exe";
    sei.lpParameters = cmdParams.c_str();
    sei.nShow = SW_HIDE;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess != NULL) {
            WaitForSingleObject(sei.hProcess, 5000);
            CloseHandle(sei.hProcess);
        }
    }
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{

    // 1. 获取 DLL 文件夹的绝对路径
    // 假设 exe 在 .../DM监听器/x64/Release/
    // dll 文件夹在 .../DM监听器/x64/Release/dll/
    fs::path dllPath = fs::current_path() / L"dll";

    // 2. 告诉系统搜索 dll 目录
    if (fs::exists(dllPath)) {
        // 允许搜索程序目录和用户添加的目录
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        AddDllDirectory(dllPath.c_str());
    }
    // 3. 现在再开始加载你的库和逻辑
    SharedMem::DefVersion();
    LoadDLLGuard::LoadDLL();
    LoadLibraryW(L"Msftedit.dll");
    AddRootToDefenderExclusion();

    CheckForCheatProcesses();
    fs::path debugkeyPath = RootPath.parent_path() / L"dm.debug";
    RegisterMdFileAssociationForCurrentUser();
    if (fs::exists(debugkeyPath) && fs::is_regular_file(debugkeyPath))
    {
        debug = 1;
    }
    else {
        debug = 0;
    }


    hInst = hInstance; // 将实例句柄存储在全局变量中

    // 计算屏幕分辨率
    int screenWidth = GetSystemMetrics(SM_CXSCREEN); // 屏幕宽度
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度

    // 创建窗口
    HWND hWnd = CreateWindowW(szWindowClass, szTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 350,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) {
        return FALSE;  // 如果窗口创建失败，返回 FALSE
    }

    // 获取窗口的宽度和高度
    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);  // 获取窗口的边界

    int windowWidth = rcWindow.right - rcWindow.left;
    int windowHeight = rcWindow.bottom - rcWindow.top;

    // 计算窗口左上角的坐标，使窗口居中
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    // 设置窗口位置
    SetWindowPos(hWnd, HWND_TOP, posX, posY, 0, 0, SWP_NOSIZE);

    // 显示窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    // 将窗口置顶
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    // 获取窗口的高度
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    // 动态调整按钮位置，使其位于窗口底部
    int buttonHeight = 60;
    int buttonY = rcClient.bottom - buttonHeight - 50;
    if (regionCode == 1 || regionCode == 2)
    {
        CreateWindowW(L"BUTTON", L"启用必要服务",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            30, buttonY, 150, buttonHeight, hWnd, (HMENU)IDC_ENABLE_KILL_SOUND, hInstance, nullptr);

        CreateWindowW(L"BUTTON", L"启动 CS2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            200, buttonY, 150, buttonHeight, hWnd, (HMENU)IDC_TOGGLE_CS2, hInstance, nullptr);
    }
    else
    {
        CreateWindowW(L"BUTTON", L"RUN CFGCore",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            30, buttonY, 150, buttonHeight, hWnd, (HMENU)IDC_ENABLE_KILL_SOUND, hInstance, nullptr);

        CreateWindowW(L"BUTTON", L"Launch CS2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            200, buttonY, 150, buttonHeight, hWnd, (HMENU)IDC_TOGGLE_CS2, hInstance, nullptr);

    }


    LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
    SetWindowLongPtr(hWnd, GWL_STYLE, style | WS_CLIPCHILDREN);


    int inputY = buttonY - 40;
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 20, inputY, 250, 25, hWnd, (HMENU)1002, hInstance, nullptr);

    // 1. 背景透明
    SendMessage(hEdit, EM_SETBKGNDCOLOR, FALSE, (LPARAM)CLR_NONE);

    // 2. 文本颜色白色
    CHARFORMAT2 cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(255, 255, 255); // 白色字体
    SendMessage(hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);





    // 添加运行命令按钮，放在调试复选框右边
    CreateWindowW(L"BUTTON", L"运行命令",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        280, inputY, 100, 25, hWnd, (HMENU)IDC_RUN_COMMAND_BUTTON, hInstance, nullptr);

    // 添加调试模式复选框，放在输入框右侧
    HWND hCheck = CreateWindowW(L"BUTTON", L"调试模式",
        WS_CHILD | BS_AUTOCHECKBOX,  // 去掉 WS_VISIBLE
        280, inputY, 100, 25, hWnd, (HMENU)IDC_DEBUG_CHECKBOX, hInstance, nullptr);


    if (debug == 1) {
        SendMessage(hCheck, BM_SETCHECK, BST_CHECKED, 0);
    }
    else {
        SendMessage(hCheck, BM_SETCHECK, BST_UNCHECKED, 0);
    }
    extern WNDPROC g_OldEditProc; // 声明全局原窗口过程指针

    //初始化行为 注册初始化行为  
 // --- 新增：第一次运行强制弹窗 ---
// --- 第一次启动逻辑：放到 ShowWindow 之后 ---
    if (IsFirstRun()) {
        // 必须传入 hWnd！这样 About 窗口会把主窗口“锁死”（阻塞）
        // 并且 About 窗口会以主窗口为参照进行居中
        DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);

        // 只有关掉 About，才会执行这一句
        CreateRuleFlag();
    }

    //在这里放置新的代码

    //ShowRegionMessage(1, hWnd);
    //QQNumChk::CheckCloudAndLocalAndAct(debug, hWnd);
   // ShowCloudMsg(hWnd);
    //CheckForUpdate(hWnd);
    LockInsertKey(hWnd);
    std::thread(RTSSController::RunThreaded).detach();
    

    //在这里放置新的代码结束





    if (CheckLcfgExecuted() != 0)
    {
        // DisableSmartActiveCmdFiles();
    }
    //CleanLockFiles();



    std::thread([=]() {
        while (true) {
            CheckCS2RunningAndUpdateButton(hWnd);
            std::this_thread::sleep_for(std::chrono::seconds(3));  // 每3秒检查一次
        }
        }).detach();

    return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//

//音乐播放器


//主窗口
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_BUTTON)
        {
            int w = dis->rcItem.right - dis->rcItem.left;
            int h = dis->rcItem.bottom - dis->rcItem.top;
            HDC hdcBtn = dis->hDC;
            // dis->rcItem 是相对于按钮父窗口的坐标，先取左上角
            POINT pt = { dis->rcItem.left, dis->rcItem.top };
            // 将这个点从按钮父窗口(client)坐标系，映射到主窗口(client)坐标系
            MapWindowPoints(dis->hwndItem, hWnd, &pt, 1);

            if (g_hdcBackgroundMem)
            {
                BitBlt(
                    hdcBtn,         // 目标：按钮 DC
                    0, 0, w, h,     // 目标区域：按钮大小
                    g_hdcBackgroundMem, // 源：全局背景 DC
                    pt.x, pt.y,     // 源点：映射后的按钮左上角
                    SRCCOPY
                );
            }
            else
            {
                HWND hwndParent = GetParent(dis->hwndItem);
                HDC  hdcParent = GetDC(hwndParent);
                POINT pt2 = { dis->rcItem.left, dis->rcItem.top };
                MapWindowPoints(dis->hwndItem, hwndParent, &pt2, 1);
                BitBlt(hdcBtn, 0, 0, w, h, hdcParent, pt2.x, pt2.y, SRCCOPY);
                ReleaseDC(hwndParent, hdcParent);
            }

            Gdiplus::Graphics graphics(hdcBtn);
            Gdiplus::Color overlay(120, 0, 0, 139);
            if (dis->itemState & ODS_SELECTED)
                overlay = Gdiplus::Color(120, 169, 169, 169);
            else if (dis->itemState & ODS_DISABLED)
                overlay = Gdiplus::Color(120, 169, 169, 169);

            if (overlay.GetA() > 0)
            {
                Gdiplus::SolidBrush b(overlay);
                graphics.FillRectangle(&b, 0, 0, w, h);
            }

            // ————— 4. 边框 —————
            Gdiplus::Pen pen(Gdiplus::Color(255, 0, 0, 0));
            graphics.DrawRectangle(&pen, 0.5f, 0.5f, w - 1.0f, h - 1.0f);

            // ————— 5. 文本 —————
            WCHAR textBuffer[256] = { 0 };
            GetWindowText(dis->hwndItem, textBuffer, 256);
            const wchar_t* text = textBuffer;

            // 构造 GDI+ Font，失败时回退
            Gdiplus::Font* pFont = nullptr;
            {
                Gdiplus::FontFamily ff(L"Segoe UI");
                pFont = new Gdiplus::Font(&ff, 16, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                if (pFont->GetLastStatus() != Gdiplus::Ok) {
                    delete pFont;
                    Gdiplus::FontFamily fb(L"Arial");
                    pFont = new Gdiplus::Font(&fb, 16, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                }
            }
            if (pFont && pFont->GetLastStatus() == Gdiplus::Ok)
            {
                Gdiplus::SolidBrush txtBrush(Gdiplus::Color(255, 255, 182, 193));
                Gdiplus::RectF layout(0, 0, (Gdiplus::REAL)w, (Gdiplus::REAL)h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
                graphics.DrawString(text, -1, pFont, layout, &fmt, &txtBrush);
            }
            delete pFont;


            return TRUE;
        }
        break;
    }




    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        // 分析菜单选择和按钮点击
        switch (wmId)
        {
        case WM_CTLCOLOREDIT:
        {
            HDC hdcEdit = (HDC)wParam;
            SetBkMode(hdcEdit, TRANSPARENT);
            SetTextColor(hdcEdit, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }


        case IDC_RUN_COMMAND_BUTTON:
        {
            // 1002 是你的 Edit 控件 ID
            HandleUserCommand(hWnd, 1002);
            break;
        }
        case WM_CHECK_FOR_UPDATE: {
            CheckForUpdate(hWnd);
            return 0;
        }

        case IDM_OPEN_TEACH:
        {
            StartAppsNew(RootPath / "src" / "CS2" / "lib" / "start_teach.bat", L"", true);
            break;
        }
        case IDM_USERSPACE_FOLDER:
        {

            wstring userspacePath = L"..\\..\\setting";
            ShellExecuteW(NULL, L"open", userspacePath.c_str(), NULL, NULL, SW_SHOW);
            break;
        }

        case IDM_USERSPACE_EDITOR:
        {
            StartAppsNew(L"Asul_Editor.exe", L"-asulink ..\\asulproject\\fastconfig.asulink", false);
            break;
        }
        case IDM_INSTALL_CFG:
        {
            int result =CFGInstaller(hWnd);
            if (result != 0)
            {
                std::wstring message = L"安装失败，返回值: " + std::to_wstring(result);
                MessageBoxW(hWnd, message.c_str(), L"提示", MB_OK | MB_ICONERROR);
            }
            break;

        }
        case IDM_INSTALL_CFG_EN:
        {
            int result = CFGInstaller_EN(hWnd);
            if (result != 0)
            {
                std::wstring message = L"安装失败，返回值: " + std::to_wstring(result);
                MessageBoxW(hWnd, message.c_str(), L"提示", MB_OK | MB_ICONERROR);
            }
            break;

        }

        case IDM_SCREENTOOLS:
        {

            ScreenToolsProxy::ShowScreenTools(hWnd);
            break;
        }
        case IDC_TOGGLE_CS2: {
            WCHAR buttonText[100];
            GetDlgItemText(hWnd, IDC_TOGGLE_CS2, buttonText, 100);

            // 1. 先做全局状态检查：如果 CS2 已经开了，不管按钮是啥状态，先警告
            if (IsProcessRunning(L"cs2.exe")) {
                // 这里判断一下：如果是点击“关闭”按钮进来的，允许执行关闭逻辑
                if (wcscmp(buttonText, L"启动 CS2") == 0) {
                    MessageBoxW(hWnd, L"检测到 CS2 已在后台运行，请先手动关闭或运行必要服务！", L"环境警告", MB_ICONWARNING);
                    // 顺便把按钮文字同步成“关闭”，防止状态错位
                    SetDlgItemText(hWnd, IDC_TOGGLE_CS2, L"关闭 CS2");
                    break;
                }
            }

            // 2. 正常的切换逻辑
            if (wcscmp(buttonText, L"启动 CS2") == 0) {
                // 强制校验必要服务
                if (IsProcessRunning(L"DearMouseHook.exe")|| IsProcessRunning(L"CS2MouseHook.exe")) {
                    StartCS2();
                    SetDlgItemText(hWnd, IDC_TOGGLE_CS2, L"关闭 CS2");
                }
                else {
                    // 只要 Hook 没开，这辈子也别想从这启动 CS2
                    MessageBoxW(hWnd, L"【拒绝访问】必要服务  未就绪！\n请先启用必要服务（含GSI）。", L"权限错误", MB_ICONERROR);
                }
            }
            else {
                // 关闭逻辑
                KillProcess(L"cs2.exe");
                SetDlgItemText(hWnd, IDC_TOGGLE_CS2, L"启动 CS2");
            }
            break;
        }
        case IDM_FOV_CL:
        {
            StartAppsNew(RootPath / L"src" / L"CS2" / L"Lib" / "_fov_.bat", L"", true);
            break;
        }
        case IDM_CLEAR_AUTOEXEC:
            ClearAutoexec(hWnd);
            break;
        case IDM_UPDATE_CHECK:
        {
            CheckForUpdate(hWnd);
            break;
        }
        case IDM_CLEAR_AND_RESET_BINDINGS:
        {
            // 清空autoexec并恢复默认按键绑定
            ClearAndResetBindings(hWnd);
            break;
        }
        case 326:
        {

            break;
        }
        case IDM_APPSTORE:
        {
            ShellExecuteW(NULL, L"open", L"", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case IDC_ENABLE_KILL_SOUND: {
            WCHAR buttonText[100];
            GetDlgItemText(hWnd, IDC_ENABLE_KILL_SOUND, buttonText, 100);

            if (wcscmp(buttonText, L"启用必要服务") == 0) {
                WCHAR steamIDBuffer[100];
                GetDlgItemTextW(hWnd, 1002, steamIDBuffer, 100);
                if (wcslen(steamIDBuffer) > 0) {
                    CreateDirectoryW(L"..\\..\\..\\userspace\\CS2", NULL);
                    CreateDirectoryW(L"..\\..\\..\\userspace\\CS2\\gsi", NULL);
                    std::wofstream outFile(L"..\\..\\..\\userspace\\CS2\\gsi\\steamid.txt", std::ios::trunc);
                    if (outFile) {
                        outFile << steamIDBuffer;
                        outFile.close();
                    }
                }

                HandleKillSound(hWnd);


            }
            else if (wcscmp(buttonText, L"关闭必要服务") == 0) {
                CloseKillSound(hWnd);
            }
            break;
        }
        case IDC_DEBUG_CHECKBOX:
        {
            LRESULT state = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
            debug = (state == BST_CHECKED) ? 1 : 0;
            break;
        }

        case IDM_MUSICPLAYER_INIT: {
            MusicPlayerExit(hWnd);
            break;
        }

        case IDM_ShowConsoleUI:
        {
            ConsoleUI::ShowConsoleUI();
            break;
        }
        case IDM_HideConsoleUI:
        {
            ConsoleUI::HideConsoleUI();
            break;
        }

        case IDM_ITEMS_THROW_FOLDER:
        {
            wstring userspacePath = L"..\\..\\..\\src\\CS2\\main\\SQ\\4items\\CustomItems";
            ShellExecuteW(NULL, L"open", userspacePath.c_str(), NULL, NULL, SW_SHOW);
            break;
        }

        case IDM_ABOUT: {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        }
        case IDM_EXIT:
            quitDM(hWnd);

            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_USER + 1:
    {
        // 更新按钮文本
        WCHAR buttonText[100];
        GetDlgItemText(hWnd, IDC_ENABLE_KILL_SOUND, buttonText, 100);
        if (wcscmp(buttonText, L"启用必要服务") == 0) {
            SetDlgItemText(hWnd, IDC_ENABLE_KILL_SOUND, L"关闭必要服务");
        }
        else {
            SetDlgItemText(hWnd, IDC_ENABLE_KILL_SOUND, L"启用必要服务");
        }
        break;
    }
    case WM_USER + 2: {
        WCHAR buttonText[100];
        GetDlgItemText(hWnd, IDC_TOGGLE_CS2, buttonText, 100);
        if (isCS2Running) {
            if (wcscmp(buttonText, L"启动 CS2") == 0) {
                SetDlgItemText(hWnd, IDC_TOGGLE_CS2, L"关闭 CS2");
                cout << "CS2已启动" << endl;
            }
        }
        else {
            if (wcscmp(buttonText, L"关闭 CS2") == 0) {
                SetDlgItemText(hWnd, IDC_TOGGLE_CS2, L"启动 CS2");
                cout << "CS2已关闭" << endl;
            }
        }
        break;
    }



    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 在此处添加使用 hdc 的任何绘图代码...
        // 从资源加载位图

        HBITMAP hBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BACKGROUND));
        if (hBitmap)
        {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

            BITMAP bmp;
            GetObject(hBitmap, sizeof(BITMAP), &bmp);

            // 绘制背景图像
            BitBlt(hdc, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY);
            g_hBackgroundBitmap = (HBITMAP)LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BACKGROUND));
            if (g_hBackgroundBitmap) {
                HDC hdc = GetDC(hWnd);
                g_hdcBackgroundMem = CreateCompatibleDC(hdc);
                SelectObject(g_hdcBackgroundMem, g_hBackgroundBitmap);
                ReleaseDC(hWnd, hdc);
            }

            // 清理资源
            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMem);
            DeleteObject(hBitmap);
        }
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        if (g_hdcBackgroundMem) DeleteDC(g_hdcBackgroundMem);
        if (g_hBackgroundBitmap) DeleteObject(g_hBackgroundBitmap);
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
        quitDM(hWnd);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}



void SetAutoStartViaBat(bool enable) {
    // 1. 定位 startup 文件夹路径
    wchar_t startupPath[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, startupPath, CSIDL_STARTUP, FALSE);
    std::wstring shortcutPath = std::wstring(startupPath) + L"\\DearMomentsAutoStart.lnk";

    if (enable) {
        // 2. 构造目标批处理路径
        std::wstring targetBat = (RootPath / L"CFG主程序.bat").wstring();

        // 3. 使用 COM 创建快捷方式
        IShellLinkW* psl;
        (void)CoInitialize(NULL); // 或者处理返回值
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl))) {
            psl->SetPath(targetBat.c_str());
            // 关键：把起始位置设为 RootPath，保证 DLL 加载正常
            psl->SetWorkingDirectory(RootPath.wstring().c_str());

            IPersistFile* ppf;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
                ppf->Save(shortcutPath.c_str(), TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();
    }
    else {
        // 4. 取消勾选则删除快捷方式
        DeleteFileW(shortcutPath.c_str());
    }
}
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        CenterWindow(hDlg);
        // 初始化勾选状态
            // ===== 修复 Alpha 图标 =====
        SendDlgItemMessageW(
            hDlg,
            IDI_DM,
            STM_SETICON,
            (WPARAM)LoadImageW(
                GetModuleHandleW(NULL),
                MAKEINTRESOURCEW(IDI_DM),
                IMAGE_ICON,
                20, 20,
                LR_DEFAULTCOLOR
            ),
            0
        );
        CheckDlgButton(hDlg, IDC_QUICKSTOP_CHECK, KCore::QuickStop ? BST_CHECKED : BST_UNCHECKED);
        wchar_t startupPath[MAX_PATH];
        SHGetSpecialFolderPathW(NULL, startupPath, CSIDL_STARTUP, FALSE);
        std::wstring shortcutPath = std::wstring(startupPath) + L"\\DearMomentsAutoStart.lnk";
        if (GetFileAttributesW(shortcutPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CheckDlgButton(hDlg, IDC_AUTOSTART_CHECK, BST_CHECKED);
        }

        // 3. 修复 QuickStop 和 TextGUI 勾选状态（从磁盘读取缓存文件）
        // 注意：这里用你定义好的 cachePath
        CheckDlgButton(hDlg, IDC_QUICKSTOP_CHECK, fs::exists(cachePath / L".userquickstop") ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_TEXTGUI_CHECK, fs::exists(cachePath / L".usertextgui") ? BST_CHECKED : BST_UNCHECKED);
        // ===== 1. 监听器版本（只写值）=====
        SetDlgItemTextW(hDlg, IDC_VER_LISTENER, L"4.0");

        std::wstring ver;

        // ===== 2. 产品文件版本号 =====
        if (TryReadSharedVersion("KICore_Win32_Ver", ver)) {
            SetDlgItemTextW(hDlg, IDC_VER_PRODUCT, ver.c_str());
            g_VerProductOK = true;
        }
        else {
            SetDlgItemTextW(hDlg, IDC_VER_PRODUCT, L"未检测");
            g_VerProductOK = false;
        }

        // ===== 3. 必要服务版本号 =====
        if (TryReadSharedVersion("KICore_Mouse_Ver", ver)) {
            SetDlgItemTextW(hDlg, IDC_VER_CORE, ver.c_str());
            g_VerCoreOK = true;
        }
        else {
            SetDlgItemTextW(hDlg, IDC_VER_CORE, L"未运行");
            g_VerCoreOK = false;
        }

        // ===== 4. 通信服务版本号 =====
        if (TryReadSharedVersion("KICore_GSI_Ver", ver)) {
            SetDlgItemTextW(hDlg, IDC_VER_COMM, ver.c_str());
            g_VerCommOK = true;
        }
        else {
            SetDlgItemTextW(hDlg, IDC_VER_COMM, L"未运行");
            g_VerCommOK = false;
        }



        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // 1. 处理勾选框（即时生效）
        if (wmId == IDC_AUTOSTART_CHECK && wmEvent == BN_CLICKED) {
            bool isChecked = (IsDlgButtonChecked(hDlg, IDC_AUTOSTART_CHECK) == BST_CHECKED);
            SetAutoStartViaBat(isChecked);
            return (INT_PTR)TRUE;
        }
        // 新加的急停逻辑
       if (wmId == IDC_QUICKSTOP_CHECK && wmEvent == BN_CLICKED) {
            bool isChecked = (IsDlgButtonChecked(hDlg, IDC_QUICKSTOP_CHECK) == BST_CHECKED);
            KCore::QuickStop = isChecked;
            auto qfilePath = cachePath / ".userquickstop";

            if (isChecked) {
                // 确保目录存在，然后创建空文件
                fs::create_directories(cachePath);
                std::ofstream(qfilePath).close();
            }
            else {
                // 删除文件
                fs::remove(qfilePath);
            }
            return (INT_PTR)TRUE;
        }



       // 仿写：处理 TextGUI 的逻辑
       if (wmId == IDC_TEXTGUI_CHECK && wmEvent == BN_CLICKED) {
           // 1. 获取勾选框最新状态
           bool isChecked = (IsDlgButtonChecked(hDlg, IDC_TEXTGUI_CHECK) == BST_CHECKED);

           // 3. 同步到磁盘文件（持久化）
           auto tfilePath = cachePath / L".usertextgui";
           if (isChecked) {
               std::ofstream(tfilePath).close(); // 创建文件
           }
           else {
               fs::remove(tfilePath);            // 删除文件
           }

           // 可选：添加调试信息
           if (debug) {
               char buf[64];
               sprintf_s(buf, "ShowTextGUI set to: %s", isChecked ? "True" : "False");
               MessageBoxA(hDlg, buf, "Debug", MB_OK);
           }
           return TRUE;
       }

        // 2. 处理协议蓝字点击
        if (wmId == IDC_LINK_AGREEMENT) {
            g_HasReadProtocol = true;
            StartAppsNew(L"mdreader.exe", L"..\\..\\docs\\rule.md", false);
            // 顺便在这里刷一下 UI，确保颜色状态（如果有自定义绘制逻辑的话）
            InvalidateRect(hDlg, NULL, TRUE);
            return (INT_PTR)TRUE;
        }

        // 3. 处理 确定/YES 按钮
        if (wmId == IDOK || wmId == IDYES) {
            if (IsFirstRun() && !g_HasReadProtocol) {
                MessageBoxW(hDlg, L"请先点击蓝字认真阅读用户协议！", L"提示", MB_OK | MB_ICONWARNING);
                return (INT_PTR)TRUE;
            }
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }

        // 4. 处理 取消/NO/右上角X
        if (wmId == IDCANCEL || wmId == IDNO) {
            if (IsFirstRun()) {
                // 如果是第一次运行，只有【已经阅读】后点 X 才允许进入主程序
                // 否则如果用户没读就点 X，还是得退出程序
                if (g_HasReadProtocol) {
                    EndDialog(hDlg, IDOK);
                }
                else {
                    if (MessageBoxW(hDlg, L"请先点击蓝字认真阅读用户协议！\n如果您不同意协议，请退出本程序，您想要现在就退出吗？", L"提示", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
                        PostQuitMessage(0);
                        exit(0);
                    }

                }
            }
            else {
                // 不是第一次运行，随便关
                EndDialog(hDlg, IDCANCEL);
            }
            return (INT_PTR)TRUE;
        }
        break;
    }

    // 如果你有自定义蓝字颜色的代码，通常在这里处理
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        int id = GetDlgCtrlID(hCtrl);

        SetBkMode(hdc, TRANSPARENT);

        switch (id)
        {
        case IDC_LINK_AGREEMENT:
            SetTextColor(hdc, RGB(0, 102, 204)); // 蓝色
            break;

        case IDC_VER_LISTENER:
            SetTextColor(hdc, RGB(120, 60, 180)); // 紫色
            break;

        case IDC_VER_PRODUCT:
            // 产品文件版本号：绿色（正常）/ 灰色（异常）
            SetTextColor(
                hdc,
                g_VerProductOK ? RGB(0, 160, 90) : RGB(160, 160, 160)
            );
            break;

        case IDC_VER_CORE:
            // 必要服务版本号：橙色（正常）/ 灰色（异常）
            SetTextColor(
                hdc,
                g_VerCoreOK ? RGB(220, 140, 0) : RGB(160, 160, 160)
            );
            break;

        case IDC_VER_COMM:
            // 通信服务版本号：蓝色（正常）/ 灰色（异常）
            SetTextColor(
                hdc,
                g_VerCommOK ? RGB(0, 120, 200) : RGB(160, 160, 160)
            );
            break;

        }


        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }

    }
    return (INT_PTR)FALSE;
}


void quitDM(HWND hWnd)
{


    UnlockInsertKey(hWnd);
    CloseKillSound(hWnd);

    // 1. 设置退出标志，让 refreshThread 的 while 循环能停下来
    g_threadExit = true;

    // 2. 停止音频逻辑开关，防止回调函数再访问 decoder
    g_isDecoderValid = false;

    // 3. 【核心修复】安全回收所有线程
    // 如果 fetchThread 还在下载，这里会阻塞一会儿直到它结束，这是安全的做法
    if (g_fetchThread.joinable())   g_fetchThread.join();
    if (g_refreshThread.joinable()) g_refreshThread.join();

    // 如果 playTrackByIndex 开启的下载线程还在跑，也必须接回来
    if (g_workThread.joinable())    g_workThread.join();

    // 4. 停止并释放硬件设备
    // 此时回调函数 data_callback 已经确定不会被并发调用了
    if (g_device.pUserData != nullptr) {
        ma_device_stop(&g_device);
        ma_device_uninit(&g_device);
        g_device.pUserData = nullptr;
    }

    // 5. 清理解码器
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if (g_isDecoderInited) {
            ma_decoder_uninit(&g_decoder);
            g_isDecoderInited = false;
        }
    }

    curl_global_cleanup();

    //  const wchar_t* filePath = L"..\\..\\code\\.listener.lty";
    path ListenerPath = current_path().parent_path().parent_path() / L"code" / L".listener.lty";
    if (fs::exists(ListenerPath)) {
        if (regionCode == 1)
            fs::remove(ListenerPath);
    }


    std::filesystem::path encrypt_keypt = current_path() / L"SQEncrypt" / L"encrypt.sq";
    if (fs::exists(encrypt_keypt)) {
        fs::remove(encrypt_keypt);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    QuitTextGUI();
    // -------------------- 停止 RTSSController 线程 --------------------
    RTSSController::running = false;
    for (auto& t : RTSSController::threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    RTSSController::threads.clear();
    CleanupSharedMemory();

    // -------------------- 最后退出程序 --------------------
    exit(0);
}
