#pragma once
#include <string>

namespace CS2ConsoleWatcher {
    class VdfParser {
    public:
        static std::wstring FindCSLogPath(const std::wstring& steamPath);
    };
}
