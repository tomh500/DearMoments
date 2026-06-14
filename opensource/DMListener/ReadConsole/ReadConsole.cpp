#include "RegistryReader.h"
#include "VdfParser.h"
#include "LogWatcher.h"
#include <iostream>
#include <windows.h> 
using namespace std;
using namespace CS2ConsoleWatcher;

/*int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        bool rawMode = true; 
        for (int i = 1; i < argc; i++) {
            wstring arg = argv[i];
            if (arg == L"-raw") {
                rawMode = true;
            }
            else {
                rawMode = false;
            }
        }

        wstring steamPath = RegistryReader::GetSteamPath();
        wstring logPath = VdfParser::FindCSLogPath(steamPath);

        wcout << L"Found CS2 console log at: " << logPath << endl;

        LogWatcher watcher(logPath, rawMode);
        watcher.Watch();
    }
    catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
    }
    return 0;
}

*/