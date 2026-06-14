#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include<iostream>
#include "framework.h"
#include "DMMain.h"
#include"Tools.h"

#pragma comment(lib, "Kernel32.lib")

using namespace std;
using namespace filesystem;

bool SetWorkingDirectory(LPCWSTR path) {
    wchar_t newDir[MAX_PATH] = { 0 };

    if (path == nullptr || wcslen(path) == 0) {
        // 获取程序自身所在目录
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
            return false;
        }

        // 去掉文件名，保留目录
        if (!PathRemoveFileSpecW(exePath)) {
            return false;
        }

        wcscpy_s(newDir, exePath); // 设置为程序所在目录
    }
    else {
        // 将相对路径转为绝对路径
        if (GetFullPathNameW(path, MAX_PATH, newDir, NULL) == 0) {
            return false;
        }
    }

    // 设置当前工作目录
    return SetCurrentDirectoryW(newDir) != 0;
}
bool CheckRunPath()
{
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    path exeDir = path(exePath).parent_path();
    wstring exeDirStr = exeDir.wstring();
    transform(exeDirStr.begin(), exeDirStr.end(), exeDirStr.begin(), ::towlower);
    wstring expectedSuffix = L"\\counter-strike global offensive\\game\\csgo\\cfg\\DearNextgen\\library\\execute";
    transform(expectedSuffix.begin(), expectedSuffix.end(), expectedSuffix.begin(), ::towlower);
    if (exeDirStr.size() >= expectedSuffix.size() &&
        exeDirStr.compare(exeDirStr.size() - expectedSuffix.size(), expectedSuffix.size(), expectedSuffix) == 0)
    {
        return true;
    }
    else
    {
        if (regionCode == 1 || regionCode == 2)
        {
            MessageBoxW(nullptr, L"路径错误，请阅读教程", L"路径错误", MB_ICONERROR | MB_OK);
        }
        else {
            MessageBoxW(nullptr, L"Path Wrong.Read docs Please", L"Path Error", MB_ICONERROR | MB_OK);
        }
        
        //exit(0);
        return false;
    }
}
string WString2String(const wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], size_needed, NULL, NULL);
    return str;
}

wstring String2WString(const string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], size_needed);
    return wstr;
}


bool FolderExists(const wstring& folderPath) {
    DWORD fileType = GetFileAttributesW(folderPath.c_str());
    return (fileType != INVALID_FILE_ATTRIBUTES) && (fileType & FILE_ATTRIBUTE_DIRECTORY);
}



// 复制文件并自动创建路径
bool CopyFile(const wstring& src, const wstring& dst)
{
    try {
        path dstPath(dst);
        create_directories(dstPath.parent_path()); // 自动创建目标文件夹
        copy_file(src, dst, copy_options::overwrite_existing);

        wstring successMsg = L"成功复制文件：\n" + src + L"\n到\n" + dst;

          if(debug==1)  MessageBoxW(nullptr, successMsg.c_str(), L"复制成功", MB_OK | MB_ICONINFORMATION);
        
       
        return true;
    }
    catch (const filesystem_error& e) {
        wstring errorMsg = L"复制文件失败(copy failed)：\n" + src + L"\n到\n" + dst +
            L"\n错误信息(error code)：" + String2WString(e.what());
        if(debug==1)MessageBoxW(nullptr, errorMsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
        return false;
    }
}


int StartApps(const wstring& exePath, const wstring& arguments, bool showWindow) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    // 拼接命令行
    wstring cmdLine = L"\"" + exePath + L"\" " + arguments;
    LPWSTR cmdLineBuffer = &cmdLine[0];

    // 可选调试信息（如果需要调试输出，可以自行临时加上）
    if (debug == 1)
    {

    wstringstream ss;
    ss << L"即将执行的命令：\n" << cmdLine;
    MessageBoxW(nullptr, ss.str().c_str(), L"调试信息", MB_OK | MB_ICONINFORMATION);
    }


    if (!CreateProcessW(nullptr, cmdLineBuffer, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();

        wstringstream ss;
        if (regionCode == 1 || regionCode == 2)
        {
            ss << L"无法启动程序！错误代码：" << err;
        }
        else {
            ss << L"Could not launch new app , error code ：" << err;
        }
       
        MessageBoxW(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

// filesystem版本
#include <filesystem>
namespace fs = filesystem;

/*
int StartAppsNew(const fs::path& relativeExePath, const wstring& arguments, bool showWindow)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    // 拼接完整路径（现在 relativeExePath 已经是 fs::path）
    fs::path fullExePath = relativeExePath;
    wstring exePathStr = fullExePath.wstring();

    // 构造命令行
    wstring cmdLine = L"\"" + exePathStr + L"\" " + arguments;
    LPWSTR cmdLineBuffer = &cmdLine[0];

    extern int debug;
    if (debug == 1)
    {
        wstringstream ss;
        ss << L"即将执行的命令：\n" << cmdLine;
        MessageBoxW(nullptr, ss.str().c_str(), L"调试信息", MB_OK | MB_ICONINFORMATION);
    }

    if (!CreateProcessW(nullptr, cmdLineBuffer, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();

        wstringstream ss;
        if (regionCode == 1 || regionCode == 2)
        {
            ss << L"无法启动程序！错误代码：" << err;
        }
        else {
            ss << L"Could not launch new app , error code ：" << err;
        }
        MessageBoxW(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
*/
int StartAppsNew(const fs::path& relativeExePath, const wstring& arguments, bool showWindow)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    // 1. 获取 EXE 的绝对路径，并规范化（处理反斜杠）
    // 即使你传入的是 "Asul_Editor.exe"，它也会根据当前 RootPath 补全
    fs::path fullExePath = fs::absolute(relativeExePath).make_preferred();
    wstring exePathStr = fullExePath.wstring();

    // 2. 自动提取工作目录 (EXE 所在的文件夹)
    // 这一步解决了 "Asul_Editor.exe" 找不到同目录 DLL 或参数路径解析错误的问题
    wstring workDir = fullExePath.parent_path().wstring();

    // 3. 构造命令行 (程序路径建议带上引号，防止空格导致解析失败)
    wstring cmdLine = L"\"" + exePathStr + L"\" " + arguments;

    // CreateProcessW 的第二个参数需要可写的缓冲区，所以使用向量或 &str[0]
    // 这里使用 vector 保证内存安全
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    // 调试模式下弹窗确认路径
    if (debug == 1)
    {
        wstringstream ss;
        ss << L"执行路径: " << exePathStr << L"\n\n"
            << L"工作目录: " << workDir << L"\n\n"
            << L"完整命令: " << cmdLine;
        MessageBoxW(nullptr, ss.str().c_str(), L"Debug - 启动检查", MB_OK | MB_ICONINFORMATION);
    }

    // 4. 执行进程
    if (!CreateProcessW(
        nullptr,            // 不直接用 lpApplicationName，让它从 cmdLine 解析
        cmdBuffer.data(),   // 命令行参数
        nullptr,            // 进程安全属性
        nullptr,            // 线程安全属性
        FALSE,              // 不继承句柄
        0,                  // 创建标志
        nullptr,            // 环境变量
        workDir.c_str(),    // 核心改进：传入 EXE 所在的文件夹作为工作目录
        &si,
        &pi))
    {
        DWORD err = GetLastError();
        wstringstream ss;
        if (regionCode == 1 || regionCode == 2) {
            ss << L"无法启动程序！错误代码：" << err << L"\n路径：" << exePathStr;
        }
        else {
            ss << L"Could not launch app, error code: " << err << L"\nPath: " << exePathStr;
        }
        MessageBoxW(nullptr, ss.str().c_str(), L"启动失败", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 成功启动后关闭句柄，防止资源泄露
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

bool IsProcessRunning(const wstring& processName) {
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return false;
    }
    cProcesses = cbNeeded / sizeof(DWORD);
    for (unsigned int i = 0; i < cProcesses; i++) {
        if (aProcesses[i] == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, aProcesses[i]);
        if (hProcess != NULL) {
            wchar_t szProcessName[MAX_PATH] = L"<unknown>";
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, szProcessName, &size)) {
                wstring fullPath(szProcessName);
                size_t pos = fullPath.find_last_of(L"\\/");
                wstring exeName = (pos != wstring::npos) ? fullPath.substr(pos + 1) : fullPath;
                if (_wcsicmp(exeName.c_str(), processName.c_str()) == 0) {
                    CloseHandle(hProcess);
                    return true;
                }
            }
            CloseHandle(hProcess);
        }
    }
    return false;
}


void CenterWindow(HWND hwnd) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// 检查是否是第一次运行
bool IsFirstRun() {
    std::filesystem::path ruleFile = RootPath / L"scripts" / L".rule";
    return !std::filesystem::exists(ruleFile);
}

// 创建隐藏的标记文件
void CreateRuleFlag() {
    std::filesystem::path scriptsDir = RootPath / L"scripts";
    if (!std::filesystem::exists(scriptsDir)) {
        std::filesystem::create_directories(scriptsDir);
    }

    std::filesystem::path ruleFile = scriptsDir / L".rule";
    std::ofstream ofs(ruleFile);
    ofs << "User agreed to terms."; // 随便写点
    ofs.close();

    // 设置为隐藏文件
    SetFileAttributesW(ruleFile.c_str(), FILE_ATTRIBUTE_HIDDEN);
}

// 关闭进程
void KillProcess(const wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // 将传入的进程名转换为小写
    wstring lowerProcessName = processName;
    transform(lowerProcessName.begin(), lowerProcessName.end(), lowerProcessName.begin(), ::towlower);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // 将进程名转换为小写并进行比较
            wstring lowerExeName = pe32.szExeFile;
            transform(lowerExeName.begin(), lowerExeName.end(), lowerExeName.begin(), ::towlower);

            if (lowerProcessName == lowerExeName) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

void ClearAutoexec(HWND hWnd)
{
    // 弹出确认框，询问用户是否确认清空 autoexec.cfg

    int msgBoxResult = MessageBoxW(hWnd, L"确定要清空 autoexec.cfg 吗？\nAre you sure to clear autoexec?", L"确认操作", MB_YESNO | MB_ICONQUESTION);

    if (msgBoxResult == IDYES)  // 用户点击了“是”
    {
        // 获取autoexec.cfg的路径
        wstring filePath = L"..\\..\\..\\autoexec.cfg";

        // 打开文件并清空它
        ofstream file(filePath, ios::trunc);  // 以截断模式打开文件（即清空内容）
        if (file.is_open())
        {
            // 文件已清空
            MessageBoxW(hWnd, L"autoexec.cfg已清空", L"操作成功", MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            // 文件打开失败
            MessageBoxW(hWnd, L"无法打开autoexec.cfg文件\n open autoexec error", L"错误", MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        // 用户点击了“否”，不执行清空操作
        MessageBoxW(hWnd, L"操作已取消", L"取消操作", MB_OK | MB_ICONINFORMATION);
    }
}



void RegisterMdFileAssociationForCurrentUser()
{
    namespace fs = filesystem;

    // 获取 mdreader.exe 的完整路径
    fs::path exePath = fs::current_path() / L"MDReader.exe";
    if (!fs::exists(exePath)) {
        MessageBoxW(nullptr, L"找不到 MDReader.exe，无法注册 .md 关联。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 注册表根为当前用户
    HKEY root = HKEY_CURRENT_USER;
    HKEY hKey;

    // 设置 .md 的默认文件类型为 MarkdownFileUser
    if (RegCreateKeyExW(root, L"Software\\Classes\\.md", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        const wchar_t* fileType = L"MarkdownFileUser";
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(fileType),
            (DWORD)(wcslen(fileType) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // 设置 MarkdownFileUser 的打开命令
    wstring command = L"\"" + exePath.wstring() + L"\" \"%1\"";
    if (RegCreateKeyExW(root, L"Software\\Classes\\MarkdownFileUser\\shell\\open\\command", 0,
        nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            (DWORD)(command.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // 设置图标（可选）
    wstring iconPath = exePath.wstring() + L",0";
    if (RegCreateKeyExW(root, L"Software\\Classes\\MarkdownFileUser\\DefaultIcon", 0,
        nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(iconPath.c_str()),
            (DWORD)(iconPath.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    if (debug) {
        MessageBoxW(nullptr, L".md 文件已关联到 MDReader.exe（仅限当前用户）！请重启资源管理器或注销登录使其生效。",
            L"注册成功", MB_OK | MB_ICONINFORMATION);
    }
}


void QuitTextGUI() {
    HWND hWnd = FindWindow(L"ConfigWatermarkClass", nullptr); // 窗口类名
    if (hWnd) {
        PostMessage(hWnd, WM_CLOSE, 0, 0); // 安全退出
        wcout << L"已发送安全退出消息给渲染程序。" << endl;
    }
    else {
        wcerr << L"未找到渲染程序窗口。" << endl;
    }
}

int GetRegionCode() {
    char region[256];
    int len = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, region, sizeof(region));
    if (len > 0) {
        string regionStr(region);
        if (regionStr == "CN") {
            return 1;
        }
        else if (regionStr == "TW") {
            return 2;
        }
        else if (regionStr == "JP") {
            return 3;
        }
    }
    return 0; // 未知
}

void ShowRegionMessage(int regionCode,HWND hWnd) {
    wstring message;
    switch (regionCode) {
    case 1:
        message = L"Summer公开版，完全免费发布，您的地区：中国大陆";
        break;
    case 2:
        message = L"Summer公开版，完全免费发布，您的地区：台湾地区";
        break;
    case 3:
        message = L"Summer公开版，完全免费发布，您的地区：日本";
        break;
    default:
        message = L"Thanks For Use Summer Client , it's a good CFG for free";
        break;
    }
  //  MessageBoxW(hWnd, message.c_str(), L"地区信息", MB_OK | MB_ICONINFORMATION);
}





// 宽字符 (wstring) 转 UTF8 (string) -> SDL 用
string WideToUtf8(const wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// 窄字符 (const char*) 转 wstring -> Windows UI 用
wstring Utf8ToWide(const char* str) {
    if (!str) return L"";
    string s(str);
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wstr[0], len);
    wstr.resize(wcslen(wstr.c_str()));
    return wstr;
}