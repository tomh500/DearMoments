#pragma once
#include <string>
#include <vector>
#include <tuple>

namespace QQNumChk{

    // Download URL using WinHTTP
    std::string DownloadUrlToStringA_WinHTTP(const char* url, int debug = 0);

    // Download URL using WinInet
    std::string DownloadUrlToStringA(const char* url);

    // UTF-8 string to wstring
    std::wstring Utf8ToWstring(const std::string& utf8);

    // Check if wstring is all digits
    bool IsAllDigitsW(const std::wstring& s);

    // Parse Enable flag from UTF-8 content (-1 if not found, 0 or 1 if present)
    int ParseEnableFlagFromUtf8(const std::string& s);

    // Parse entries from UTF-8 content
    void ParseEntriesFromUtf8(
        const std::string& s,
        std::vector<std::tuple<std::wstring, std::wstring, std::wstring>>& out,
        int debug = 0
    );

    // Main function to check cloud and local data
    void CheckCloudAndLocalAndAct(int debug,HWND hWnd);

} 
