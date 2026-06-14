#include "RegistryReader.h"
#include <windows.h>
#include <stdexcept>

using namespace std;

namespace CS2ConsoleWatcher {
    wstring RegistryReader::GetSteamPath() {
        HKEY hKey;
        wstring steamPath;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            throw runtime_error("Failed to open Steam registry key.");
        }

        wchar_t buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer), &bufferSize) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            throw runtime_error("Failed to read SteamPath from registry.");
        }

        RegCloseKey(hKey);
        steamPath = buffer;
        return steamPath;
    }
}
