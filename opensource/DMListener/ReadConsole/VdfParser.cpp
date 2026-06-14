#include "VdfParser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <stdexcept>

using namespace std;
namespace fs = std::filesystem;

namespace CS2ConsoleWatcher {
    wstring VdfParser::FindCSLogPath(const wstring& steamPath) {
        wstring vdfPath = steamPath + L"\\steamapps\\libraryfolders.vdf";
        if (!fs::exists(vdfPath)) {
            throw runtime_error("libraryfolders.vdf not found.");
        }

        ifstream file(vdfPath); 
        if (!file.is_open()) {
            throw runtime_error("Failed to open libraryfolders.vdf.");
        }

        string line, currentPath;
        bool found730 = false;

        while (getline(file, line)) {
            smatch match;

         
            if (regex_search(line, match, regex("\"path\"\\s*\"([^\"]+)\""))) {
                currentPath = match[1].str();
            }

  
            if (line.find("\"730\"") != string::npos) {
                found730 = true;
                break;
            }
        }

        if (!found730 || currentPath.empty()) {
            throw runtime_error("CS2 (730) not found in libraryfolders.vdf.");
        }

        // string -> wstring
        wstring ws(currentPath.begin(), currentPath.end());
        wstring csPath = ws + L"\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\console.log";
        return csPath;
    }
}
