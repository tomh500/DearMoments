#include "ConsoleUI.h"

// 针对 cout
ConsoleUI::CoutRedirect::int_type ConsoleUI::CoutRedirect::overflow(int_type c) {
    if (c != traits_type::eof()) {
        std::lock_guard<std::mutex> lock(logMutex);
        logBuffer << traits_type::to_char_type(c);
    }
    return traits_type::not_eof(c);
}

// 针对 wcout
ConsoleUI::WcoutRedirect::int_type ConsoleUI::WcoutRedirect::overflow(int_type c) {
    if (c != traits_type::eof()) {
        std::lock_guard<std::mutex> lock(logMutex);
        wlogBuffer << traits_type::to_char_type(c);
    }
    return traits_type::not_eof(c);
}

void ConsoleUI::RefreshThread() {
    while (running) {
        std::string s;
        std::wstring ws;

        // 1. 快速取走数据，尽量减少锁的占用时间
        {
            std::lock_guard<std::mutex> lock(logMutex);
            s = logBuffer.str();
            ws = wlogBuffer.str();
            if (!s.empty()) { logBuffer.str(""); logBuffer.clear(); }
            if (!ws.empty()) { wlogBuffer.str(L""); wlogBuffer.clear(); }
        }

        if (hEdit && (!s.empty() || !ws.empty())) {
            // 2. 解决换行问题：将单 \n 转换为 \r\n
            // 顺便限制单次处理的长度，防止塞爆控件
            auto FixNewLine = [](const std::string& input) {
                std::string out;
                out.reserve(input.size() * 1.1); // 预留点空间
                for (char c : input) {
                    if (c == '\n') out += "\r\n";
                    else if (c != '\r') out += c;
                }
                return out;
                };

            // 3. 使用 SendMessage 的副作用：如果数据量太大，这里会阻塞 GUI
            // 我们通过逻辑处理，确保只有在有数据时才发消息
            if (!s.empty()) {
                std::string formatted = FixNewLine(s);
                int len = GetWindowTextLengthA(hEdit);
                // 限制控件最大长度，超过 30000 字符就清空，防止 Edit 控件崩溃
                if (len > 30000) SetWindowTextW(hEdit, L"--- Log Cleared to Prevent Lag ---\r\n");

                SendMessageA(hEdit, EM_SETSEL, -1, -1); // 选中末尾
                SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());
            }

            if (!ws.empty()) {
                // 宽字符同理处理...
                std::string formatted = FixNewLine(s);
                int len = GetWindowTextLengthW(hEdit);
                if (len > 30000) SetWindowTextW(hEdit, L"--- Log Cleared to Prevent Lag ---\r\n");

                SendMessageW(hEdit, EM_SETSEL, -1, -1);
                SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)ws.c_str());
            }

            // 4. 滚动到底部
            PostMessage(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
        }

        // 5. 稍微拉长一点时间，200ms 合理，给 GUI 喘息机会
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

LRESULT CALLBACK ConsoleUI::LogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_SIZE:
        if (hEdit) {
            MoveWindow(hEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        }
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void ConsoleUI::ShowConsoleUI() {
    if (hWndLog) return;

    WNDCLASS wc{};
    wc.lpfnWndProc = LogWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"ConsoleUIWindow";
    RegisterClass(&wc);
    std::cout << std::unitbuf;   // 开启单元缓冲区刷新模式
    std::wcout << std::unitbuf;  // 宽字符同理
    hWndLog = CreateWindowEx(
        0, L"ConsoleUIWindow", L"Console Logs",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // 确保 hEdit 成功拿到了句柄
    hEdit = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 600, 400,
        hWndLog, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (hEdit == NULL) {
        MessageBoxW(NULL, L"Edit Control 创建失败！", L"错误", MB_OK);
    }

    // 保存旧缓冲区并替换
    oldCoutBuf = std::cout.rdbuf(&coutRedirect);
    oldWcoutBuf = std::wcout.rdbuf(&wcoutRedirect);
    std::cerr.rdbuf(&coutRedirect);
    std::wcerr.rdbuf(&wcoutRedirect);

    running = true;
    refreshThread = std::thread(RefreshThread);
}

void ConsoleUI::HideConsoleUI() {
    running = false;
    if (refreshThread.joinable()) {
        refreshThread.join();
    }

    if (hWndLog) {
        DestroyWindow(hWndLog);
        hWndLog = nullptr;
        hEdit = nullptr;
    }

    // 恢复旧缓冲区（判断非空）
    if (oldCoutBuf) {
        std::cout.rdbuf(oldCoutBuf);
        std::cerr.rdbuf(oldCoutBuf);
        oldCoutBuf = nullptr;
    }
    if (oldWcoutBuf) {
        std::wcout.rdbuf(oldWcoutBuf);
        std::wcerr.rdbuf(oldWcoutBuf);
        oldWcoutBuf = nullptr;
    }
}
