#pragma once

#include <windows.h>
#include <string>
#include <sstream>
#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>

namespace ConsoleUI {

    inline HWND hWndLog = nullptr;
    inline HWND hEdit = nullptr;
    inline std::mutex logMutex;
    inline std::ostringstream logBuffer;
    inline std::wostringstream wlogBuffer;
    inline std::thread refreshThread;
    inline bool running = false;

    // 重定向 std::cout
    class CoutRedirect : public std::streambuf {
    protected:
        using base_type = std::streambuf;
        using int_type = base_type::int_type;
        virtual int_type overflow(int_type c) override;
    };

    // 重定向 std::wcout
    class WcoutRedirect : public std::wstreambuf {
    protected:
        using base_type = std::wstreambuf;
        using int_type = base_type::int_type;
        virtual int_type overflow(int_type c) override;
    };


    inline CoutRedirect coutRedirect;
    inline WcoutRedirect wcoutRedirect;
    inline std::streambuf* oldCoutBuf = nullptr;
    inline std::wstreambuf* oldWcoutBuf = nullptr;

    void RefreshThread();
    LRESULT CALLBACK LogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowConsoleUI();
    void HideConsoleUI();

} // namespace ConsoleUI
