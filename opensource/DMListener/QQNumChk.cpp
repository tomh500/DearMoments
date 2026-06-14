// check_cloud_and_local_no_regex.cpp
// MSVC x64, C++20
// Compile: cl /std:c++20 /EHsc check_cloud_and_local_no_regex.cpp Everything64.lib wininet.lib


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <cctype>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "SharedMem.h"
#include "Hardinfo.h"

#pragma comment(lib, "winhttp.lib")

#include "Everything.h" // Everything SDK header

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace QQNumChk {
    // ---------- helpers ----------

    // 被插入到 helpers 区域（例如放在 Utf8ToWstring 之后）：
    static std::wstring DecimalToOctalW(const std::wstring& dec) {
        if (dec.empty()) return L"";
        unsigned long long v = 0;
        for (wchar_t c : dec) {
            if (c < L'0' || c > L'9') return L"";
            v = v * 10ull + (unsigned long long)(c - L'0');
        }
        if (v == 0) return L"0";
        std::wstring out;
        while (v > 0) {
            wchar_t d = L'0' + (wchar_t)(v % 8ull);
            out.push_back(d);
            v /= 8ull;
        }
        std::reverse(out.begin(), out.end());
        return out;
    }

    static std::wstring OctalToDecimalW(const std::wstring& oct) {
        if (oct.empty()) return L"";
        unsigned long long v = 0;
        for (wchar_t c : oct) {
            if (c < L'0' || c > L'7') return L"";
            v = v * 8ull + (unsigned long long)(c - L'0');
        }
        return std::to_wstring(v);
    }

    static std::wstring ToBinaryW(uint64_t v) {
        if (v == 0) return L"0";
        std::wstring out;
        while (v > 0) {
            out.push_back((v & 1) ? L'1' : L'0');
            v >>= 1;
        }
        std::reverse(out.begin(), out.end());
        return out;
    }



    static std::string WstringToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
            NULL, 0, NULL, NULL);
        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
            &result[0], size_needed, NULL, NULL);
        return result;
    }

    static std::string DownloadUrlToStringA_WinHTTP(const char* url, int debug = 0)
    {
        std::string result;
        // Crack URL
        URL_COMPONENTS uc{};
        uc.dwStructSize = sizeof(uc);
        uc.dwSchemeLength = uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = 1; // request lengths

        wchar_t wurl[4096];
        int wlen = MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, (int)std::size(wurl));
        if (wlen == 0) {
            if (debug) std::wcerr << L"[DEBUG] MultiByteToWideChar failed for URL\n";
            return result;
        }

        if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpCrackUrl failed\n";
            return result;
        }

        // extract host, path
        std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
        std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
        std::wstring extra(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        std::wstring fullPath = path + extra;

        if (debug) {
            std::wcout << L"[DEBUG] WinHTTP host: " << host << L", path: " << fullPath << L"\n";
        }

        // Initialize WinHTTP session
        HINTERNET hSession = WinHttpOpen(L"EverythingChecker/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpOpen failed\n";
            return result;
        }

        // connect
        INTERNET_PORT port = uc.nPort;
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
        if (!hConnect) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpConnect failed\n";
            WinHttpCloseHandle(hSession);
            return result;
        }

        // open request
        DWORD flags = WINHTTP_FLAG_ESCAPE_PERCENT | WINHTTP_FLAG_BYPASS_PROXY_CACHE | WINHTTP_FLAG_SECURE;

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", fullPath.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpOpenRequest failed\n";
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Send request
        BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpSendRequest failed\n";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            if (debug) std::wcerr << L"[DEBUG] WinHttpReceiveResponse failed\n";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // check HTTP status
        DWORD status = 0;
        DWORD statusLen = sizeof(status);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) {
            if (debug) std::wcout << L"[DEBUG] HTTP status: " << status << L"\n";
            if (status != 200) {
                if (debug) std::wcout << L"[DEBUG] HTTP not OK, status != 200\n";
                // still attempt to read body for debugging
            }
        }
        else {
            if (debug) std::wcout << L"[DEBUG] WinHttpQueryHeaders(status) failed\n";
        }

        // Read response body
        const DWORD chunkSize = 8192;
        std::vector<char> buffer(chunkSize);
        DWORD bytesRead = 0;
        while (true) {
            if (!WinHttpQueryDataAvailable(hRequest, &bytesRead)) break;
            if (bytesRead == 0) break;
            if (bytesRead > chunkSize) buffer.resize(bytesRead);
            DWORD actuallyRead = 0;
            if (!WinHttpReadData(hRequest, buffer.data(), bytesRead, &actuallyRead)) break;
            if (actuallyRead == 0) break;
            result.append(buffer.data(), actuallyRead);
            // reset buffer size
            if (buffer.size() != chunkSize) buffer.resize(chunkSize);
        }

        // cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (debug) std::wcout << L"[DEBUG] WinHTTP read " << result.size() << L" bytes\n";
        return result;
    }

    static std::string DownloadUrlToStringA(const char* url)
    {
        std::string result;
        HINTERNET hInternet = InternetOpenA("MyAgent", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return result;

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
        if (!hUrl) { InternetCloseHandle(hInternet); return result; }

        const DWORD bufSize = 4096;
        char buffer[bufSize];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, bufSize, &bytesRead) && bytesRead != 0) {
            result.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return result;
    }

    static std::wstring Utf8ToWstring(const std::string& utf8)
    {
        if (utf8.empty()) return L"";
        size_t start = 0;
        if (utf8.size() >= 3 && (unsigned char)utf8[0] == 0xEF && (unsigned char)utf8[1] == 0xBB && (unsigned char)utf8[2] == 0xBF)
            start = 3;
        int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data() + start, (int)(utf8.size() - start), NULL, 0);
        if (needed <= 0) return L"";
        std::wstring out(needed, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data() + start, (int)(utf8.size() - start), out.data(), needed);
        return out;
    }

    static bool IsAllDigitsW(const std::wstring& s) {
        if (s.empty()) return false;
        for (wchar_t c : s) if (!iswdigit(c)) return false;
        return true;
    }

    // case-insensitive find for ASCII (good enough for "Enable=")
    static int ParseEnableFlagFromUtf8(const std::string& s)
    {
        // search for "enable=" case-insensitive, then return next 0/1 if present
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        const std::string key = "enable=";
        size_t p = lower.find(key);
        if (p == std::string::npos) return -1;
        size_t i = p + key.size();
        while (i < lower.size() && (lower[i] == ' ' || lower[i] == '\t' || lower[i] == '\r' || lower[i] == '\n')) ++i;
        if (i < lower.size()) {
            if (lower[i] == '0') return 0;
            if (lower[i] == '1') return 1;
        }
        return -1;
    }

    // Parse entries directly from UTF-8 string (no regex).
    // For each '[', find id (digits), status (alnum/_), and msg in double quotes.
    // Appends tuples of wstrings (id,status,msg) to out.
 // 替换为下面这个实现（注意模板参数为四元组）
    static void ParseEntriesFromUtf8(const std::string& s, std::vector<std::tuple<std::wstring, std::wstring, std::wstring, std::wstring>>& out, int debug = 0)
    {
        size_t pos = 0;
        while (pos < s.size()) {
            size_t lb = s.find('[', pos);
            if (lb == std::string::npos) break;
            size_t rb = s.find(']', lb + 1);
            if (rb == std::string::npos) break;

            std::string inner = s.substr(lb + 1, rb - lb - 1);
            size_t idx = 0;
            auto skip_ws = [&](void) {
                while (idx < inner.size() && (inner[idx] == ' ' || inner[idx] == '\t' || inner[idx] == '\r' || inner[idx] == '\n')) ++idx;
                };

            skip_ws();
            // id
            size_t id_start = idx;
            while (idx < inner.size() && std::isdigit((unsigned char)inner[idx])) ++idx;
            std::string id = inner.substr(id_start, idx - id_start);
            if (id.empty()) { pos = rb + 1; continue; }

            skip_ws();
            if (idx >= inner.size() || inner[idx] != ',') { pos = rb + 1; continue; }
            ++idx;

            skip_ws();
            // status
            size_t st_start = idx;
            while (idx < inner.size() && (std::isalnum((unsigned char)inner[idx]) || inner[idx] == '_')) ++idx;
            std::string status = inner.substr(st_start, idx - st_start);
            if (status.empty()) { pos = rb + 1; continue; }

            skip_ws();
            if (idx >= inner.size() || inner[idx] != ',') { pos = rb + 1; continue; }
            ++idx;

            skip_ws();
            // msg (quoted or until comma/end)
            std::string msgutf8;
            if (idx < inner.size() && inner[idx] == '"') {
                ++idx;
                while (idx < inner.size()) {
                    char c = inner[idx++];
                    if (c == '\\') {
                        if (idx < inner.size()) { char next = inner[idx++]; msgutf8.push_back(next); continue; }
                        else break;
                    }
                    if (c == '"') break;
                    msgutf8.push_back(c);
                }
            }
            else {
                size_t mstart = idx;
                while (idx < inner.size() && inner[idx] != ',') ++idx;
                msgutf8 = inner.substr(mstart, idx - mstart);
                while (!msgutf8.empty() && isspace((unsigned char)msgutf8.back())) msgutf8.pop_back();
            }

            // parse optional 4th field (hardinfo) — 允许缺省
            std::string hardutf8;
            skip_ws();
            if (idx < inner.size() && inner[idx] == ',') {
                ++idx;
                skip_ws();
                if (idx < inner.size() && inner[idx] == '"') {
                    ++idx;
                    while (idx < inner.size()) {
                        char c = inner[idx++];
                        if (c == '\\') {
                            if (idx < inner.size()) { char next = inner[idx++]; hardutf8.push_back(next); continue; }
                            else break;
                        }
                        if (c == '"') break;
                        hardutf8.push_back(c);
                    }
                }
                else {
                    size_t hstart = idx;
                    while (idx < inner.size()) ++idx;
                    hardutf8 = inner.substr(hstart, idx - hstart);
                    while (!hardutf8.empty() && isspace((unsigned char)hardutf8.back())) hardutf8.pop_back();
                }
            }

            std::wstring wid = Utf8ToWstring(id);
            std::wstring wstatus = Utf8ToWstring(status);
            std::wstring wmsg = Utf8ToWstring(msgutf8);
            std::wstring whard = Utf8ToWstring(hardutf8);

            if (debug) {
                std::wstring wdebug = L"[DEBUG parse] id=" + wid + L" status=" + wstatus + L" msg=" + wmsg + L" hard=" + whard + L"\n";
                std::wcout << wdebug;
            }

            out.emplace_back(wid, wstatus, wmsg, whard);

            pos = rb + 1;
        }
    }


    static int ParseWlistRequireVersionFromUtf8(const std::string& s)
    {
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        const std::string key = "wlistrequireversion=";
        size_t p = lower.find(key);
        if (p == std::string::npos)
            return -1;

        size_t i = p + key.size();
        while (i < lower.size() && std::isspace((unsigned char)lower[i])) ++i;

        size_t start = i;
        while (i < lower.size() && std::isdigit((unsigned char)lower[i])) ++i;

        if (start == i)
            return -1;

        return std::stoi(lower.substr(start, i - start));
    }



    // ---------- main function ----------

    void CheckCloudAndLocalAndAct(int debug, HWND hWnd)
    {
        const char* url = "";
        if (debug) std::wcout << L"[DEBUG] Downloading URL: " << Utf8ToWstring(std::string(url)) << L"\n";

        std::string remoteRaw = DownloadUrlToStringA_WinHTTP(url, debug);

        if (debug) std::wcout << L"[DEBUG] Downloaded " << remoteRaw.size() << L" bytes\n";
        if (remoteRaw.empty()) {
            if (debug) std::wcout << L"[DEBUG] Download failed\n";
            return;
        }

        // parse Enable flag
        int enableFlag = ParseEnableFlagFromUtf8(remoteRaw);
        if (debug) std::wcout << L"[DEBUG] Parsed Enable flag (utf8): " << enableFlag << L"\n";
        if (enableFlag == 0) {
            if (debug) std::wcout << L"[DEBUG] Cloud rules disabled (Enable=0), returning\n";
            return;
        }

        int wlistRequireVer = ParseWlistRequireVersionFromUtf8(remoteRaw);

        if (debug) {
            std::wcout << L"[DEBUG] WlistRequireVersion: "
                << wlistRequireVer
                << L", LocalVersion: "
                << g_LocalVersion
                << L"\n";
        }


        // parse entries
        std::vector<std::tuple<std::wstring, std::wstring, std::wstring, std::wstring>> cloudEntries;

        ParseEntriesFromUtf8(remoteRaw, cloudEntries, debug);
        if (debug) std::wcout << L"[DEBUG] Parsed cloud entries count: " << cloudEntries.size() << L"\n";
        if (cloudEntries.empty()) return;

        // ---------- 本地 QQ号检查 + 缓存 ----------
        // localIds 存储为 八进制 字符串（从 Everything 获取后转换为八进制并写入缓存）
        std::unordered_set<std::wstring> localIds;
        fs::path cacheFile = fs::path(L"cache") / L"qq_ids.json";
        bool needRescan = true;


        if (fs::exists(cacheFile)) {
            try {
                std::ifstream in(cacheFile);
                nlohmann::json j; in >> j;
                std::string lastUpdateStr = j.value("last_update", "");
                if (!lastUpdateStr.empty()) {
                    std::tm tm{}; std::istringstream ss(lastUpdateStr);
                    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                    if (!ss.fail()) {
                        auto lastUpdate = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                        auto now = std::chrono::system_clock::now();
                        auto diff = std::chrono::duration_cast<std::chrono::hours>(now - lastUpdate).count();
                        if (diff < 72) needRescan = false; // 小于72小时
                    }
                }
                if (!needRescan) {
                    for (auto& id : j["qq_ids"]) {
                        localIds.insert(Utf8ToWstring(id.get<std::string>()));
                    }
                    if (debug) std::wcout << L"[DEBUG] Loaded " << localIds.size() << L" QQ IDs from cache\n";
                }
            }
            catch (...) {
                if (debug) std::wcout << L"[DEBUG] Failed to read cache, rescan\n";
            }
        }
        else {
            if (debug) std::wcout << L"[DEBUG] No cache file, rescan\n";
        }

        if (needRescan) {
            const std::vector<std::wstring> checkPaths = {
                L"QQ\\miniapp\\auth",
                L"QQ\\Crashpad\\log",
                L"Tencent Files"
            };

            for (auto& basePath : checkPaths) {
                Everything_SetSearchW(basePath.c_str());
                Everything_SetMatchPath(TRUE);
                Everything_SetMatchCase(FALSE);
                Everything_SetMatchWholeWord(FALSE);
                Everything_SetMax(10000);

                if (!Everything_QueryW(TRUE)) {
                    if (debug) std::wcout << L"[DEBUG] Everything_QueryW failed for " << basePath << L"\n";
                    continue;
                }

                DWORD numResults = Everything_GetNumResults();
                auto ToLowerW = [](const std::wstring& s) {
                    std::wstring r(s);
                    std::transform(r.begin(), r.end(), r.begin(), ::towlower);
                    return r;
                    };

                for (DWORD i = 0; i < numResults; ++i) {
                    LPCWSTR resPath = Everything_GetResultPathW(i);
                    if (!resPath) continue;
                    try {
                        std::wstring res = resPath;
                        std::wstring lowRes = ToLowerW(res);

                        // basePath 来自 checkPaths 的元素
                        std::wstring lowPattern = L"\\" + ToLowerW(basePath) + L"\\";

                        size_t pos = lowRes.find(lowPattern);
                        if (pos == std::wstring::npos) continue;

                        size_t start = pos + lowPattern.size();
                        if (start >= res.size()) continue;

                        size_t end = res.find(L'\\', start);
                        std::wstring child;
                        if (end == std::wstring::npos) child = res.substr(start);
                        else child = res.substr(start, end - start);

                        if (child.empty()) continue;

                        // QQ号条件：全是数字，长度 >=5 && <=12，且不能有前导0
                        if (IsAllDigitsW(child) && child.size() >= 5 && child.size() <= 12) {
                            if (!(child.size() > 1 && child[0] == L'0')) {
                                // 把从 Everything 得到的十进制 QQ 号转换为 八进制 后存入 localIds（只写入缓存的八进制）
                                std::wstring oct = DecimalToOctalW(child);
                                if (!oct.empty()) localIds.insert(oct);
                            }
                        }

                    }
                    catch (...) {}
                }

            }

            // 写缓存
            nlohmann::json j;
            for (auto& id : localIds) j["qq_ids"].push_back(WstringToUtf8(id));
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm; localtime_s(&tm, &t);
            std::ostringstream oss; oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            j["last_update"] = oss.str();
            try {
                std::ofstream out(cacheFile);
                out << j.dump(2);
                if (debug) std::wcout << L"[DEBUG] Cache updated\n";
            }
            catch (...) {}
        }


        if (localIds.empty()) return;

        // 写缓存后：确保后续比对始终来自缓存文件（八进制），而不是直接使用 Everything 得到的数据
        localIds.clear();
        try {
            std::ifstream inReload(cacheFile);
            nlohmann::json jReload; inReload >> jReload;
            for (auto& id : jReload["qq_ids"]) {
                localIds.insert(Utf8ToWstring(id.get<std::string>())); // 插入八进制字符串
            }
            if (debug) std::wcout << L"[DEBUG] Reloaded " << localIds.size() << L" QQ IDs from cache (octal)\n";
        }
        catch (...) {
            if (debug) std::wcout << L"[DEBUG] Failed to reload cache after write\n";
        }


        bool whitelistMode = false;

        // 先判断云端是否声明了 whitelist
        for (auto& t : cloudEntries) {
            if (_wcsicmp(std::get<1>(t).c_str(), L"whitelist") == 0) {
                whitelistMode = true;
                break;
            }
        }

        // 再用版本门槛强制约束
        if (whitelistMode && wlistRequireVer >= 0) {
            if (static_cast<int>(g_LocalVersion) < wlistRequireVer) {
                {
                    if (debug) {
                        std::wcout << L"[DEBUG] Local version too low, whitelist disabled\n";
                    }
                    whitelistMode = false; // ❗ 强制降级
                }
            }


            if (debug)
                std::wcout << L"[DEBUG] Whitelist mode: "
                << (whitelistMode ? L"ON" : L"OFF") << L"\n";
            // key: binary id
              // value: (status, msg, hardinfo)
            std::unordered_map<std::wstring, std::tuple<std::wstring, std::wstring, std::wstring>> cloudMap;
            for (auto& t : cloudEntries) {
                cloudMap.emplace(
                    std::get<0>(t), // id
                    std::make_tuple(std::get<1>(t), std::get<2>(t), std::get<3>(t))
                );
            }



            // ---------- 对比云端 ----------
    // 由缓存中的八进制 ID 构建十进制查找表（不直接使用 Everything 得到的数据）
            // ---------- 对比云端 ----------
    // 本地：八进制 → 十进制 → 二进制
            std::unordered_set<std::wstring> localBinaryIds;

            for (const auto& oct : localIds) {
                std::wstring dec = OctalToDecimalW(oct);
                if (dec.empty()) continue;

                uint64_t v = _wcstoui64(dec.c_str(), nullptr, 10);
                std::wstring bin = ToBinaryW(v);
                localBinaryIds.insert(bin);
            }

            if (debug)
                std::wcout << L"[DEBUG] Built " << localBinaryIds.size()
                << L" binary IDs from octal cache\n";


            bool matchedAny = false;
            bool matchedWhitelist = false;
            for (const auto& localBin : localBinaryIds) {
                auto it = cloudMap.find(localBin);
                if (it == cloudMap.end())
                    continue;

                const std::wstring& st = std::get<0>(it->second);
                const std::wstring& msg = std::get<1>(it->second);
                const std::wstring& hwid = std::get<2>(it->second); // 云端的 hardinfo（可为空）

                if (_wcsicmp(st.c_str(), L"whitelist") == 0) {
                    if (whitelistMode) {
                        // 若云端提供了机器码，则必须匹配本地机器码
                        if (_wcsicmp(st.c_str(), L"whitelist") == 0) {
                            if (whitelistMode) {

                                // ★ 1. 云端没写机器码 → 直接拒绝
                                if (hwid.empty()) {
                                    MessageBoxW(
                                        hWnd,
                                        L"没有令牌导致的权限不足，请联系管理员处理",
                                        L"Permission Denied",
                                        MB_OK | MB_ICONERROR
                                    );
                                    exit(211);
                                }

                                // ★ 2. 校验机器码
                                std::wstring localHardW = Utf8ToWstring(g_LocalHardInfo);
                                if (hwid != localHardW) {
                                    MessageBoxW(
                                        hWnd,
                                        L"提供的令牌与身份不一致，请联系管理员处理",
                                        L"Permission Denied",
                                        MB_OK | MB_ICONERROR
                                    );
                                    exit(211);
                                }

                                // ★ 3. QQ + 机器码同时通过 → 放行
                                matchedWhitelist = true;
                                MessageBoxW(hWnd, msg.c_str(), L"Welcome", MB_OK | MB_ICONINFORMATION);
                            }
                            continue;
                        }

                    }
                    continue;
                }

                // 黑名单 / err / war：立刻拒绝（与机器码无关）
                if (_wcsicmp(st.c_str(), L"war") == 0)
                    MessageBoxW(hWnd, msg.c_str(), L"Warning", MB_OK | MB_ICONWARNING);
                else if (_wcsicmp(st.c_str(), L"err") == 0)
                    MessageBoxW(hWnd, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
                else
                    MessageBoxW(hWnd, msg.c_str(), L"Info", MB_OK | MB_ICONINFORMATION);

                MessageBoxW(hWnd, L"没有权限使用", L"Permission Denied", MB_OK | MB_ICONERROR);
                exit(211);
            }


            // ★ 白名单模式：版本达标，但一个 whitelist 都没命中 → 拒绝
            if (whitelistMode && !matchedWhitelist) {
                if (debug) {
                    std::wcout << L"[DEBUG] Whitelist enabled, version OK, but no whitelist matched → exit\n";
                }
                MessageBoxW(hWnd, L"没有权限使用",
                    L"Permission Denied", MB_OK | MB_ICONERROR);
                exit(211);
            }



        }
    }
}