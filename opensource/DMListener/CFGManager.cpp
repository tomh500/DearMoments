#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "Tools.h"
#include "Global.h"
#include "SteamHelper.h"
#include <regex>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;
using namespace filesystem;

namespace fs = std::filesystem;

#pragma comment(lib, "SteamHelper.lib")

/**
 * 返回值定义：
 * 0: 成功修改了至少一个账号
 * 1: 所有账号都已经有了 -condebug，无需操作
 * 2: 通用错误（保底）
 * 3: 注册表读取 Steam 路径失败
 * 4: Steam 用户列表为空
 * 5: 没找到任何账号的 CS2 (730) 配置文件路径
 * 6: 找到了 VDF 文件但无法打开
 * 7: 在 VDF 文件中没找到 "730" 节点
 * 8: 在 730 节点下没找到 "LaunchOptions" 键
 */

SteamHelper helper;
wstring steamPath = helper.CallRegister2Steam();

int AddCS2CondebugDebugVersion() {
    if (steamPath == L"Read Failed") return 3;

    const vector<string>& userIDs = helper.GetSteamUserIDs();
    if (userIDs.empty()) return 4;

    bool anyAddedTotal = false;
    bool allAlreadyHad = true;
    bool foundAnyVdf = false; // 新增：是否至少找到了一个物理文件
    int lastSpecificError = 5;

    for (const string& userID : userIDs) {
        path vdfPath = path(steamPath) / "userdata" / userID / "config" / "localconfig.vdf";

        if (!exists(vdfPath)) continue;

        foundAnyVdf = true; // 只要进到这里，说明文件物理存在

        string content;
        {
            ifstream inFile(vdfPath, ios::binary);
            if (!inFile.is_open()) {
                lastSpecificError = 6;
                continue;
            }
            stringstream buffer;
            buffer << inFile.rdbuf();
            content = buffer.str();
        }

        if (content.empty()) continue;

        // 查找 730 (CS2/CSGO的AppID)
        size_t pos730 = content.find("\"730\"");
        if (pos730 == string::npos) {
            if (lastSpecificError < 7) lastSpecificError = 7;
            continue;
        }

        // 走到这一步，错误码至少应该是 8 (没找到 LaunchOptions)
        if (lastSpecificError < 8) lastSpecificError = 8;

        bool modified = false;
        size_t posLaunch = content.find("\"LaunchOptions\"", pos730);

        if (posLaunch != string::npos) {
            regex launchRegex("\"LaunchOptions\"\\s+\"([^\"]*)\"");
            smatch match;

            size_t lineEnd = content.find('\n', posLaunch);
            if (lineEnd == string::npos) lineEnd = content.length();
            string line = content.substr(posLaunch, lineEnd - posLaunch);

            if (regex_search(line, match, launchRegex)) {
                string currentOptions = match[1].str();
                if (currentOptions.find("-condebug") != string::npos) {
                    continue;
                }

                allAlreadyHad = false;
                string newOptions = currentOptions;
                if (!newOptions.empty() && newOptions.back() != ' ') newOptions += " ";
                newOptions += "-condebug";

                string newLine = "\"LaunchOptions\"\t\t\"" + newOptions + "\"";
                content.replace(posLaunch, line.length(), newLine);
                modified = true;
            }
        }
        else {
            size_t posBrace = content.find('{', pos730);
            if (posBrace != string::npos) {
                string insertStr = "\n\t\t\t\t\t\t\"LaunchOptions\"\t\t\"-condebug\"";
                content.insert(posBrace + 1, insertStr);
                modified = true;
                allAlreadyHad = false;
            }
        }

        if (modified) {
            ofstream outFile(vdfPath, ios::trunc | ios::binary);
            if (outFile.is_open()) {
                outFile << content;
                outFile.close();
                anyAddedTotal = true;
            }
        }
    }

    if (anyAddedTotal) return 0;
    if (!foundAnyVdf) return 5; // 一个 VDF 路径都没对上
    if (allAlreadyHad && lastSpecificError >= 7) return 1;
    return lastSpecificError;
}

void AppendIfMissing(const path& filePath, const string& lineToAdd, HWND hWnd)
{
    string content;
    ifstream in(filePath, ios::binary);
    if (in) {
        content.assign((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
        in.close();
    }

    if (content.find(lineToAdd) == string::npos) {
        ofstream out(filePath, ios::app | ios::binary);
        if (!out) {
            MessageBoxW(hWnd, L"无法打开 autoexec.cfg 进行写入", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
        if (!content.empty() && content.back() != '\n') {
            out << "\n";
        }

        out << lineToAdd << "\n";
        MessageBoxW(hWnd, L"已成功添加 exec DearNextgen/Setup 到 autoexec.cfg", L"完成", MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(hWnd, L"autoexec.cfg 已包含 exec DearNextgen/Setup，无需添加", L"信息", MB_OK | MB_ICONINFORMATION);
    }
}

bool CopyFileEx(const fs::path& src, const fs::path& dest) {
    try {
        if (!fs::exists(src)) return false;
        fs::create_directories(dest.parent_path());
        return fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
    }
    catch (...) {
        return false;
    }
}

int CFGInstaller(HWND hWnd)
{
    fs::path cfgDir = RootPath.parent_path();
    fs::path csgoDir = cfgDir.parent_path();
    fs::path srcRoot = RootPath / L"library" / L"resource";

    auto ReportError = [&](const wstring& detail, int errorCode) {
        wstring msg = L"错误位置: " + detail + L"\n";
        msg += L"GetLastError: " + to_wstring(GetLastError()) + L"\n";
        msg += L"RootPath: " + RootPath.wstring();
        MessageBoxW(hWnd, msg.c_str(), L"安装调试信息", MB_OK | MB_ICONERROR);
        return errorCode;
        };

    KillProcess(L"cs2.exe");
    KillProcess(L"steam.exe");
    KillProcess(L"steamservice.exe");
    KillProcess(L"steamwebhelper.exe");
    Sleep(1000);
    int result = AddCS2CondebugDebugVersion();
    if (result != 0 && result != 1)
    {
        wstring errorMsg = L"写入失败 (错误码: " + to_wstring(result) + L")，请手动添加 -condebug";
        MessageBoxW(hWnd, errorMsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
    }
	StartAppsNew(path(steamPath) / L"steam.exe", L"", false);

    fs::path srcFile1 = srcRoot / L"DearMoments_Installed.cfg";
    fs::path destFile1 = cfgDir / L"DearMoments_Installed.cfg";

    if (!fs::exists(srcFile1)) return ReportError(L"源文件不存在: " + srcFile1.wstring(), 101);
    if (!CopyFileEx(srcFile1, destFile1)) return ReportError(L"复制 CFG 失败到: " + destFile1.wstring(), 1);

    fs::path resTargetDir = csgoDir / L"resource";
    const wchar_t* langFiles[] = {
        L"keybindings_schinese.txt", L"keybindings_english.txt",
        L"keybindings_tchinese.txt", L"keybindings_japanese.txt",
        L"keybindings_russian.txt"
    };

    for (const auto& file : langFiles) {
        fs::path s = srcRoot / file;
        fs::path d = resTargetDir / file;
        if (!fs::exists(s)) return ReportError(L"语言源文件不存在: " + s.wstring(), 102);
        if (!CopyFileEx(s, d)) return ReportError(L"复制语言文件失败: " + d.wstring(), 1);
    }

    fs::path srcGSI = srcRoot / L"gamestate_integration_square.cfg";
    fs::path destGSI = cfgDir / L"gamestate_integration_square.cfg";
    if (!fs::exists(srcGSI)) return ReportError(L"GSI源文件不存在", 103);
    if (!CopyFileEx(srcGSI, destGSI)) return ReportError(L"复制GSI失败", 1);

    fs::path soundsDir = csgoDir / L"sounds" / L"DM";
    try {
        fs::create_directories(soundsDir / L"musicmode");
        fs::create_directories(soundsDir / L"temp");
    }
    catch (const fs::filesystem_error& e) {
        return ReportError(L"创建目录异常: " + String2WString(e.what()), 3);
    }

    if (StartAppsNew(RootPath / L"library" / L"execute" / L"LinkListener.exe", L"", false) != 0) {
        return ReportError(L"LinkListener 启动失败", 4);
    }


    if (MessageBoxW(hWnd, L"文件复制成功，是否写入 autoexec？", L"确认", MB_YESNO) == IDYES) {
        try {
            AppendIfMissing(cfgDir / L"autoexec.cfg", "exec DearNextgen/Setup", hWnd);
        }
        catch (...) { return ReportError(L"autoexec 写入失败", 2); }
    }

    if (MessageBoxW(hWnd, L"是否编辑用户空间？", L"确认", MB_YESNO) == IDYES) {
        StartAppsNew(RootPath / L"library" / L"execute" / L"Asul_Editor.exe", L"-asulink ..\\asulproject\\fastconfig.asulink", false);
    }

    return 0;
}

int CFGInstaller_EN(HWND hWnd)
{/*
    fs::path cfgDir = RootPath.parent_path();
    fs::path csgoDir = cfgDir.parent_path();
    fs::path srcRoot = RootPath / L"library" / L"resource";

    auto ReportError = [&](const wstring& detail, int errorCode) {
        wstring msg = L"Error Location: " + detail + L"\n";
        msg += L"Win32 Error Code: " + to_wstring(GetLastError()) + L"\n";
        msg += L"RootPath: " + RootPath.wstring();
        MessageBoxW(hWnd, msg.c_str(), L"Installation Debug Info", MB_OK | MB_ICONERROR);
        return errorCode;
        };

    fs::path srcFile1 = srcRoot / L"DearMoments_Installed.cfg";
    fs::path destFile1 = cfgDir / L"DearMoments_Installed.cfg";

    if (!fs::exists(srcFile1)) return ReportError(L"Source file missing: " + srcFile1.wstring(), 101);
    if (!CopyFileEx(srcFile1, destFile1)) return ReportError(L"Failed to copy CFG to: " + destFile1.wstring(), 1);

    fs::path resTargetDir = csgoDir / L"resource";
    const wchar_t* langFiles[] = {
        L"keybindings_schinese.txt", L"keybindings_english.txt",
        L"keybindings_tchinese.txt", L"keybindings_japanese.txt",
        L"keybindings_russian.txt"
    };

    for (const auto& file : langFiles) {
        fs::path s = srcRoot / file;
        fs::path d = resTargetDir / file;
        if (!fs::exists(s)) return ReportError(L"Language source missing: " + s.wstring(), 102);
        if (!CopyFileEx(s, d)) return ReportError(L"Failed to copy language file: " + d.wstring(), 1);
    }

    fs::path srcGSI = srcRoot / L"gamestate_integration_square.cfg";
    fs::path destGSI = cfgDir / L"gamestate_integration_square.cfg";
    if (!fs::exists(srcGSI)) return ReportError(L"GSI source missing", 103);
    if (!CopyFileEx(srcGSI, destGSI)) return ReportError(L"Failed to copy GSI", 1);

    fs::path soundsDir = csgoDir / L"sounds" / L"DM";
    try {
        fs::create_directories(soundsDir / L"musicmode");
        fs::create_directories(soundsDir / L"temp");
    }
    catch (const fs::filesystem_error& e) {
        return ReportError(L"Directory creation failed: " + String2WString(e.what()), 3);
    }

    if (StartAppsNew(RootPath / L"library" / L"execute" / L"LinkListener.exe", L"", false) != 0) {
        MessageBoxW(hWnd, L"Unable to link desktop (LinkListener.exe failed)", L"Error", MB_OK | MB_ICONERROR);
        return 4;
    }

    int response = MessageBoxW(hWnd, L"Files copied successfully. Write autoexec.cfg?", L"Action Required", MB_YESNO | MB_ICONQUESTION);
    if (response == IDYES) {
        try {
            AppendIfMissing(cfgDir / L"autoexec.cfg", "exec DearNextgen/Setup", hWnd);
        }
        catch (...) { return ReportError(L"Failed to write autoexec.cfg", 2); }
    }

    int result = MessageBoxW(hWnd, L"Would you like to edit your UserSpace settings now?", L"Action Required", MB_YESNO | MB_ICONQUESTION);
    if (result == IDYES) {
        StartAppsNew(RootPath / L"library" / L"execute" / L"Asul_Editor.exe", L"-asulink ..\\asulproject\\fastconfig.asulink", false);
    }

    */
	MessageBoxW(hWnd, L"This English version installer is under development. Please use the Chinese version for now.", L"Info", MB_OK | MB_ICONINFORMATION);
    return 0;
}