#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include<iostream>
#include "framework.h"
#include "DMMain.h"
#pragma comment(lib, "Kernel32.lib")
namespace fs = std::filesystem;
int msgboxflag = 0;

std::vector<HANDLE> lockedFileHandles;

//RootPath定义为向上2级
fs::path RootPath = []() {
    fs::path p = std::filesystem::current_path();
    for (int i = 0; i < 2; ++i)
    {
        p = p.parent_path();
    }
    return p;
    }();


int CheckLcfgExecuted()
{
    const wchar_t* subkey = L"Software\\LCFG";
    const wchar_t* valueName = L"LCFGHasRun";
    DWORD expectedValue = 1;
    DWORD existingValue = 0;
    DWORD dataSize = sizeof(DWORD);
    DWORD type = 0;

    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        subkey,
        0,
        KEY_QUERY_VALUE,
        &hKey);

    if (result != ERROR_SUCCESS)
    {
        return 2; // 读取失败
    }

    result = RegQueryValueExW(
        hKey, valueName, nullptr, &type,
        reinterpret_cast<BYTE*>(&existingValue),
        &dataSize);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || type != REG_DWORD)
    {
        return 2; // 读取失败
    }

    return (existingValue == expectedValue) ? 0 : 1;
}
/*
void DisableSmartActiveCmdFiles()
{
    msgboxflag = 0;

    const fs::path baseMapsPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features" / L"Modules" / L"SmartActive" / L"actions" / L"maps";
    const fs::path baseUtilitiesPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features" / L"Modules" / L"SmartActive" / L"actions" / L"utilities";

    // 白名单路径，要排除的目录（大小写敏感，根据实际情况调整）
    const fs::path whiteListPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features" / L"Modules" / L"SmartActive" / L"actions" / L"utilities" / L"fastvip";

    // 匹配cmd_数字.cfg的正则表达式
    std::wregex cmdFileRegex(LR"(cmd_(\d+)\.cfg)");

    auto processFile = [](const fs::path& filePath) {
        // 重命名并隐藏文件
        fs::path newPath = filePath;
        newPath.replace_extension(L".lockfile");

        int suffix = 1;
        while (fs::exists(newPath))
        {
            std::wstring suffixStr = L".lockfile" + std::to_wstring(suffix);
            newPath = filePath;
            newPath.replace_extension(suffixStr);
            suffix++;
            if (suffix > 1000) break;
        }

        std::error_code ec;
        fs::rename(filePath, newPath, ec);
        if (ec)
        {
            std::wstringstream ss;
            ss << L"重命名失败:\n" << filePath.wstring() << L"\n错误: " << ec.message().c_str();
            MessageBoxW(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        // 设置隐藏属性
        DWORD attr = GetFileAttributesW(newPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            SetFileAttributesW(newPath.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
        }

        return true;
        };

    std::function<void(const fs::path&)> recursiveScanMaps = [&](const fs::path& currentPath)
        {
            if (!fs::exists(currentPath) || !fs::is_directory(currentPath))
                return;

            for (const auto& entry : fs::directory_iterator(currentPath))
            {
                if (entry.is_directory())
                {
                    recursiveScanMaps(entry.path());
                }
                else if (entry.is_regular_file())
                {
                    auto filename = entry.path().filename().wstring();

                    if (filename == L"_init_.cfg" || std::regex_match(filename, cmdFileRegex))
                    {
                        if (processFile(entry.path()) && msgboxflag != 1)
                        {
                            MessageBoxW(nullptr, L"你的电脑未运行过LCFG但是存在自动身法文件，已禁用相关功能，请遵守用户协议。", L"警告", MB_OK | MB_ICONWARNING);
                            msgboxflag = 1;
                        }
                    }
                }
            }
        };

    std::function<void(const fs::path&)> recursiveScanUtilities = [&](const fs::path& currentPath)
        {
            if (!fs::exists(currentPath) || !fs::is_directory(currentPath))
                return;

            // 排除白名单路径及其子目录
            if (currentPath.native().find(whiteListPath.native()) == 0)
            {
                // 当前路径在白名单路径下，跳过
                return;
            }

            for (const auto& entry : fs::directory_iterator(currentPath))
            {
                if (entry.is_directory())
                {
                    recursiveScanUtilities(entry.path());
                }
                else if (entry.is_regular_file())
                {
                    auto filename = entry.path().filename().wstring();

                    if (filename == L"_init_.cfg" || std::regex_match(filename, cmdFileRegex))
                    {
                        if (processFile(entry.path()) && msgboxflag != 1)
                        {
                            MessageBoxW(nullptr, L"你的电脑未运行过LCFG但是存在自动身法文件，已禁用相关功能，请遵守用户协议。", L"警告", MB_OK | MB_ICONWARNING);
                            msgboxflag = 1;
                        }
                    }
                }
            }
        };

    // 执行扫描
    recursiveScanMaps(baseMapsPath);
    recursiveScanUtilities(baseUtilitiesPath);
}



void CleanLockFiles()
{
    fs::path basePath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features" / L"Modules" / L"SmartActive";

    if (!fs::exists(basePath) || !fs::is_directory(basePath)) {
        std::wstringstream ss;
        ss << L"[错误] SmartActive 目录不存在:\n" << basePath.wstring();
        MessageBoxW(nullptr, ss.str().c_str(), L"清理失败", MB_OK | MB_ICONERROR);
        return;
    }

    int deleted_count = 0;
    std::wstringstream deleted_files;

    for (const auto& file : fs::recursive_directory_iterator(basePath)) {
        if (!file.is_regular_file()) continue;

        fs::path filePath = file.path();
        std::wstring ext = filePath.extension().wstring();

        // 如果扩展名以 .lockfile 开头
        if (ext.find(L".lockfile") == 0) {
            std::error_code ec;
            fs::remove(filePath, ec);
            if (ec) {
                std::wstringstream err;
                err << L"删除失败:\n" << filePath.wstring()
                    << L"\n错误: " << ec.message().c_str();
                MessageBoxW(nullptr, err.str().c_str(), L"删除错误", MB_OK | MB_ICONERROR);
            }
            else {
                deleted_count++;
                deleted_files << L"删除: " << filePath.wstring() << L"\n";
            }
        }
    }

    std::wstringstream result;
    result << L"[完成] 已删除 " << deleted_count << L" 个 .lockfile 文件。\n";
    if (deleted_count > 0)
        result << L"\n删除文件列表:\n" << deleted_files.str();
    else
        result << L"\n没有发现任何 .lockfile 文件。";

    if (debug == 1)
    {
        MessageBoxW(nullptr, result.str().c_str(), L"清理完成", MB_OK | MB_ICONINFORMATION);
    }
}

*/

void createSetupCfg()
{
    // 1. 定义路径，建议基于你刚才找出来的 RootPath，或者用相对路径转绝对路径
    fs::path targetDir = fs::current_path().parent_path().parent_path() / "scripts";
    fs::path filePath = targetDir / ".listener.lty";

    try {
        // 2. 检查并创建目录（原代码如果 code 文件夹不存在会失败，这里帮它建好）
        if (!fs::exists(targetDir)) {
            fs::create_directories(targetDir);
        }

        // 3. 如果文件已存在，先删掉（保持原逻辑）
        if (fs::exists(filePath)) {
            fs::remove(filePath);
        }

        // 4. 使用 wofstream 写入 UTF-8 内容
        std::wofstream file(filePath, std::ios::out | std::ios::binary);
        if (file.is_open()) {
            // 设置编码为 UTF-8
            file.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));

            // 写入那一串验证字符
            file << L"dweiqjw4jitwjniofwe4houiw4hjtf4hjw3\n";

            file.close();
        }
    }
    catch (const fs::filesystem_error& e) {
        // 防止权限不足或路径非法导致崩溃
        std::cerr << "文件操作失败: " << e.what() << std::endl;
    }
}


void HandleKillSound(HWND hWnd) {
    // 1. 启动两个核心程序
  
        if (!KCore::QuickStop)
        {
            StartAppsNew(L"DM-GSI.exe", L"", debug);
            StartAppsNew(L"DearMouseHook.exe", L"", debug); 
        }
        else {
            StartAppsNew(L"DM-GSI.exe", L"", debug);
            StartAppsNew(L"DearMouseHook.exe", L"-quickstop", debug); 
        }
        StartAppsNew(L"DearMomentsViewer.exe", L"", debug);
    
  

    std::thread([=]() {
        // 给程序一点启动时间
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 2. 检查是否都运行成功（或者根据你需求只查一个）
        bool gsiRunning = IsProcessRunning(L"DM-GSI.exe");
        bool hookRunning = IsProcessRunning(L"DearMouseHook.exe");

        if (gsiRunning && hookRunning) {
            // 发送消息通知 UI 更新按钮状态（比如变成“关闭服务”）
            PostMessageW(hWnd, WM_USER + 1, 0, 0);
        }
        }).detach();
}


void CloseKillSound(HWND hWnd) {
    KillProcess(L"DM-GSI.exe");
    KillProcess(L"DearMouseHook.exe");
	KillProcess(L"DearMomentsViewer.exe");
    PostMessage(hWnd, WM_USER + 1, 0, 0);
}




void ClearAndResetBindings(HWND hWnd)
{
    // 1. 定位路径：RootPath 的上层
    fs::path filePath = RootPath.parent_path() / "autoexec.cfg";

    // 2. 询问用户
    int result = MessageBoxW(hWnd, L"确定清空autoexec并恢复默认按键绑定吗？", L"确认操作", MB_YESNO | MB_ICONQUESTION);
    if (result == IDNO) return;

    try {
        // 3. 使用 filesystem 检查父目录是否存在（虽然 autoexec 通常在根部，但保险起见）
        if (!fs::exists(filePath.parent_path())) {
            MessageBoxW(hWnd, L"找不到指定的 CS2 配置目录", L"错误", MB_OK | MB_ICONERROR);
            return;
        }

        // 4. 写入文件（ios::trunc 确保清空内容）
        std::wofstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);

        // 设置 UTF-8 编码
        file.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));

        if (file.is_open())
        {
            // 写入默认绑定
            file << L"bind mouse_x yaw;\n"
                << L"bind mouse_y pitch;\n"
                << L"unbindall;\n"
                << L"binddefaults;\n"
                << L"binddefaults;\n"
                << L"joy_response_move 1;\n"
                << L"joy_side_sensitivity 1.000000;\n"
                << L"joy_forward_sensitivity 1.000000;\n"
                << L"cl_scoreboard_mouse_enable_binding +attack2;\n"
                << L"cl_quickinventory_filename radial_quickinventory.txt;\n"
                << L"host_writeconfig\n";

            file.close();

            MessageBoxW(hWnd, L"配置已重置！启动一次游戏后，再次点击清空即可恢复纯净环境。", L"操作提示", MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            // 失败可能是因为文件被游戏占用，或者是路径权限问题
            MessageBoxW(hWnd, (L"无法打开文件: " + filePath.wstring()).c_str(), L"错误", MB_OK | MB_ICONERROR);
        }
    }
    catch (const fs::filesystem_error& e) {
        std::string errorMsg = e.what();
        MessageBoxA(hWnd, errorMsg.c_str(), "文件系统异常", MB_OK | MB_ICONERROR);
    }
}
void MusicPlayerExit(HWND hWnd)
{
    HWND hMusicWnd = CreateWindowEx(0,
        L"MusicPlayerWindowClass",
        L"音乐播放器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        500,
        400,
        nullptr, 
        nullptr,
        hInst,
        nullptr);

    if (hMusicWnd) {
        ShowWindow(hMusicWnd, SW_SHOW);
        UpdateWindow(hMusicWnd);
    }
}

void LockSmartActiveFolder()
{
    if (debug != 1)
        return;

    fs::path folderPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features" / L"Modules" / L"SmartActive";

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
    {
        if (debug == 1)
            MessageBoxW(nullptr, L"[错误] SmartActive 目录不存在，无法锁定。", L"锁定失败", MB_OK | MB_ICONERROR);
        return;
    }

    int locked_count = 0;

    for (const auto& entry : fs::recursive_directory_iterator(folderPath))
    {
        if (!entry.is_regular_file()) continue;

        HANDLE hFile = CreateFileW(
            entry.path().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,  // 禁止写、删
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE)
        {
            lockedFileHandles.push_back(hFile);
            locked_count++;
        }
    }

    if (debug == 1)
    {
        std::wstringstream ss;
        ss << L"[锁定完成] 已锁定 " << locked_count << L" 个文件。\n"<< L"SmartActive 目录现已被本程序占用，防止外部修改。";
        MessageBoxW(nullptr, ss.str().c_str(), L"锁定成功", MB_OK | MB_ICONINFORMATION);
    }
}

PSID GetCurrentUserSid()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return nullptr;

    DWORD length = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(hToken);
        return nullptr;
    }

    TOKEN_USER* tokenUser = (TOKEN_USER*)malloc(length);
    if (!GetTokenInformation(hToken, TokenUser, tokenUser, length, &length)) {
        CloseHandle(hToken);
        free(tokenUser);
        return nullptr;
    }

    DWORD sidLength = GetLengthSid(tokenUser->User.Sid);
    PSID pSid = (PSID)malloc(sidLength);
    CopySid(sidLength, pSid, tokenUser->User.Sid);

    CloseHandle(hToken);
    free(tokenUser);
    return pSid;
}


void UnlockSmartActiveFolder()
{
    for (HANDLE hFile : lockedFileHandles)
    {
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile);
    }

    lockedFileHandles.clear();

    if (debug == 1)
    {
        MessageBoxW(nullptr, L"[解锁完成] 所有 SmartActive 文件占用已解除。", L"解锁成功", MB_OK | MB_ICONINFORMATION);
    }
}

void LockSmartActiveFolderNTFS()
{
    if (debug != 1)
        return;

    fs::path folderPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features"  / L"SmartActive";

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        MessageBoxW(nullptr, L"[错误] SmartActive 目录不存在，无法锁定。", L"锁定失败", MB_OK | MB_ICONERROR);
        return;
    }

    PSID pSid = GetCurrentUserSid();
    if (!pSid) {
        MessageBoxW(nullptr, L"无法获取当前用户 SID。", L"权限设置错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 设置拒绝写入权限
    EXPLICIT_ACCESSW ea = {};
    ea.grfAccessPermissions = FILE_GENERIC_WRITE | DELETE;
    ea.grfAccessMode = DENY_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = (LPWSTR)pSid;

    PACL pNewDACL = nullptr;
    DWORD result = SetEntriesInAclW(1, &ea, nullptr, &pNewDACL);
    if (result != ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"无法创建访问控制列表 (ACL)。", L"权限设置错误", MB_OK | MB_ICONERROR);
        free(pSid);
        return;
    }

    result = SetNamedSecurityInfoW(
        (LPWSTR)folderPath.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr, nullptr,
        pNewDACL,
        nullptr
    );

    if (result == ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"SmartActive 文件夹已被锁定（禁止写入和删除）。", L"锁定成功", MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(nullptr, L"设置文件夹权限失败，可能需要管理员权限。", L"权限设置失败", MB_OK | MB_ICONERROR);
    }

    // 清理资源
    if (pNewDACL) LocalFree(pNewDACL);
    if (pSid) free(pSid);
}


void UnlockSmartActiveFolderNTFS()
{
    if (debug != 1)
        return;

    fs::path folderPath = RootPath / L"src" / L"CS2" / L"main"/ L"DM" / L"Features"  / L"SmartActive";

    DWORD result = SetNamedSecurityInfoW(
        (LPWSTR)folderPath.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr, nullptr,
        nullptr, nullptr // 清除 DACL → 恢复默认权限
    );

    if (result == ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"SmartActive 文件夹权限已恢复为默认状态。", L"解锁成功", MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(nullptr, L"解锁权限失败，可能需要管理员权限。", L"解锁失败", MB_OK | MB_ICONERROR);
    }
}
#ifdef UNICODE
#define SetDllDirectory_ SetDllDirectoryW
#else
#define SetDllDirectory_ SetDllDirectoryA
#endif


namespace  LoadDLLGuard {


    void LoadDLL()
    {
        wchar_t currentPathBuffer[MAX_PATH];
        GetModuleFileNameW(NULL, currentPathBuffer, MAX_PATH);
        fs::path currentPath = currentPathBuffer;

        // 2. 找到上一级目录
        fs::path parentPath = currentPath.parent_path().parent_path();

        // 3. 构建目标 DLL 目录的完整路径
        fs::path dllPath = parentPath / "lib";

        // 4. 使用 SetDllDirectory 将目标目录添加到 DLL 搜索路径中
        if (SetDllDirectory_(dllPath.c_str())) {
            std::wcout << L"DLL 搜索路径已成功设置为: " << dllPath.wstring() << std::flush <<std::endl;

            // 在这里调用你的 Autumn.exe 的主函数或执行其他代码
            // 例如: AutumnMain();
        }
        else {
            std::wcerr << L"设置 DLL 搜索路径失败。" << std::flush<< std::endl;
            return;
        }
    }
}