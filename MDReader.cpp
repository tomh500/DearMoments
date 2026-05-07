#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#define CMARK_STATIC
#include <cmark.h>

#include "resource.h"
namespace fs = std::filesystem;

// UTF-8 / Wide 转换
std::wstring Utf8ToWide(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string WideToUtf8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

// 读取 Markdown 文件
std::string ReadFileUtf8(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    // 替换 Windows CRLF 为 LF
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

    return content;
}


// 提取 RCDATA PNG 到临时文件
fs::path ExtractBackgroundTempFile() {
    std::wcout << L"[DEBUG] ExtractBackgroundTempFile start\n";

    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDB_BACKGROUND), RT_RCDATA);
    if (!hRes) {
        std::wcerr << L"[ERROR] Resource not found!\n";
        return fs::path();
    }

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        std::wcerr << L"[ERROR] LoadResource failed!\n";
        return fs::path();
    }

    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hData);
    if (!pData || size == 0) {
        std::wcerr << L"[ERROR] LockResource failed or size=0\n";
        return fs::path();
    }

    WCHAR tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        std::wcerr << L"[ERROR] GetTempPathW failed\n";
        return fs::path();
    }
    std::wcout << L"[DEBUG] Temp path: " << tempPath << L"\n";

    WCHAR tempFile[MAX_PATH];
    if (GetTempFileNameW(tempPath, L"bg", 0, tempFile) == 0) {
        std::wcerr << L"[ERROR] GetTempFileNameW failed\n";
        return fs::path();
    }
    std::wcout << L"[DEBUG] Temp file: " << tempFile << L"\n";

    fs::path finalFile = fs::path(tempFile).replace_extension(L".png");

    std::ofstream out(finalFile, std::ios::binary);
    if (!out) {
        std::wcerr << L"[ERROR] Cannot open temp file for writing!\n";
        return fs::path();
    }

    out.write(reinterpret_cast<const char*>(pData), size);
    out.close();

    if (!fs::exists(finalFile)) {
        std::wcerr << L"[ERROR] File not actually created!\n";
    }
    else {
        std::wcout << L"[DEBUG] Successfully wrote: " << finalFile << L"\n";
    }

    return finalFile;
}


// 写入 HTML 临时文件
fs::path WriteTempHtml(const std::string& htmlContent) {
    fs::path tempFile = fs::temp_directory_path() / "mdreader_temp.html";
    std::ofstream out(tempFile, std::ios::binary);
    out << htmlContent;
    out.close();
    return tempFile;
}

// 文件选择对话框
fs::path PickFile() {
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr)) return fs::path();

    FILEOPENDIALOGOPTIONS options;
    pFileOpen->GetOptions(&options);
    pFileOpen->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);

    COMDLG_FILTERSPEC rgSpec[] = { { L"Markdown File", L"*.md" } };
    pFileOpen->SetFileTypes(1, rgSpec);

    hr = pFileOpen->Show(NULL);
    if (FAILED(hr)) {
        pFileOpen->Release();
        return fs::path();
    }

    IShellItem* pItem = nullptr;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) {
        pFileOpen->Release();
        return fs::path();
    }

    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    fs::path result;
    if (SUCCEEDED(hr)) result = pszFilePath;
    CoTaskMemFree(pszFilePath);
    pItem->Release();
    pFileOpen->Release();
    return result;
}

// 尝试用 Edge 打开
bool OpenInEdge(const fs::path& path) {
    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = L"msedge";
    std::wstring param = path.wstring();
    sei.lpParameters = param.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

// 系统默认浏览器打开
void OpenInDefaultBrowser(const fs::path& path) {
    ShellExecuteW(NULL, L"open", path.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

int wmain(int argc, wchar_t* argv[]) {
    (void)CoInitialize(NULL);

    fs::path mdFile;
    if (argc >= 2) mdFile = argv[1];
    else mdFile = PickFile();

    if (mdFile.empty()) {
        std::wcout << L"未选择文件，退出\n";
        return 0;
    }

    std::string mdContent = ReadFileUtf8(mdFile);
    char* htmlContent = cmark_markdown_to_html(mdContent.c_str(), mdContent.size(), CMARK_OPT_DEFAULT);
       //char* htmlContent = cmark_markdown_to_html(mdContent.c_str(), mdContent.size(), CMARK_OPT_UNSAFE);
    fs::path bgFile = ExtractBackgroundTempFile();
    if (!fs::exists(bgFile)) {
        std::wcerr << L"背景文件不存在，继续使用白色背景\n";
    }

    std::string bgUrl;
    if (!bgFile.empty()) {
        bgUrl = WideToUtf8(bgFile.wstring());
        std::replace(bgUrl.begin(), bgUrl.end(), '\\', '/');
        bgUrl = "file:///" + bgUrl;
    }
    else {
        bgUrl = "#fff"; // fallback 白色
    }

    std::ostringstream fullHtml;
    fullHtml << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
        // body 背景 + 字体
        << "body{margin:0;padding:0;background:url('" << bgUrl
        << "') no-repeat center center fixed;background-size:cover;font-family:Segoe UI,Helvetica,Arial,sans-serif;}"
        // 内容容器
        << "#content{margin:20px auto;padding:20px;max-width:900px;background:rgba(255,255,255,0.4);"
        << "backdrop-filter:blur(4px);-webkit-backdrop-filter:blur(4px);border-radius:12px;"
        << "box-shadow:0 4px 30px rgba(0,0,0,0.1);overflow-x:auto;}"
        // 代码块样式
        << "pre{background:rgba(255,255,255,0.7);padding:10px;border-radius:8px;overflow-x:auto;white-space:pre-wrap;word-wrap:break-word;}"
        << "code{background:rgba(255,255,255,0.7);padding:2px 4px;border-radius:4px;}"
        // 表格样式
        << "table{border-collapse:collapse;margin:20px 0;width:100%;table-layout:fixed;}"
        << "th,td{border:1px solid #ddd;padding:12px;text-align:left;vertical-align:top;word-wrap:break-word;}"
        << "th{background:rgba(0,0,0,0.05);}"
        << "p, ul, ol, blockquote{margin:0;}"
        // 标题样式
        << "h1,h2,h3,h4,h5,h6{border-bottom:1px solid #ddd;padding-bottom:4px;}"
        << "</style></head><body><div id='content'>"
        << htmlContent
        << "</div></body></html>";
    free(htmlContent);


    fs::path htmlFile = WriteTempHtml(fullHtml.str());

    if (!OpenInEdge(htmlFile)) OpenInDefaultBrowser(htmlFile);

    return 0;
}
