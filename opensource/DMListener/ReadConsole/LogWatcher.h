#pragma once
#include <string>

namespace CS2ConsoleWatcher {
    class LogWatcher {
    public:
        LogWatcher(const std::wstring& logPath, bool rawMode);
        void Watch();
    private:
        std::wstring logPath;
        std::streampos lastSize;
        bool rawMode;
    };
}
