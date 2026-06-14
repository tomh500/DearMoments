#include "LogWatcher.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <ctime>
#include <regex>

using namespace std;
namespace fs = std::filesystem;

namespace CS2ConsoleWatcher {
    LogWatcher::LogWatcher(const wstring& logPath, bool rawMode)
        : logPath(logPath), lastSize(0), rawMode(rawMode) {
    }
    void LogWatcher::Watch() {
        static regex logPrefix(R"(^\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}\s+)");

        while (true) {
            if (!fs::exists(logPath)) {
                this_thread::sleep_for(chrono::milliseconds(500));
                continue;
            }

            auto fileSize = fs::file_size(logPath);

            // 文件被重置
            if (fileSize < lastSize) {
                lastSize = 0;
            }

            // 只有新增内容才处理
            if (fileSize > lastSize) {
                ifstream file(logPath, ios::binary);
                if (!file) {
                    lastSize = fileSize;
                    this_thread::sleep_for(chrono::milliseconds(500));
                    continue;
                }

                // 定位到上次读取位置
                file.seekg(lastSize, ios::beg);

                string line;
                while (getline(file, line)) {
                    // 跳过 BOM
                    if (!line.empty() &&
                        (unsigned char)line[0] == 0xEF &&
                        line.size() >= 3 &&
                        (unsigned char)line[1] == 0xBB &&
                        (unsigned char)line[2] == 0xBF) {
                        line = line.substr(3);
                    }

                    line = regex_replace(line, logPrefix, "");

                    if (rawMode) {
                        cout << line << endl;
                    }
                    else {
                        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
                        tm localTime{};
                        localtime_s(&localTime, &now);

                        cout << "[CS2] "
                            << put_time(&localTime, "%m/%d %H:%M:%S")
                            << " " << line << endl;
                    }
                }

                // 更新最后读取位置
                lastSize = file.tellg();
                if (lastSize == -1) lastSize = fileSize;
            }

            this_thread::sleep_for(chrono::milliseconds(250));
        }
    }


    /*
    void LogWatcher::Watch() {
        static regex logPrefix(R"(^\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}\s+)");
      
        while (true) {
            if (fs::exists(logPath)) {
                auto fileSize = fs::file_size(logPath);

                if (fileSize < lastSize) {
                    lastSize = 0; // 文件被重置，重新从头读
                }

                if (fileSize > lastSize) {
                    ifstream file(logPath, ios::binary);
                    if (file) {
                        file.seekg(lastSize);

                        string newLine;
                        while (getline(file, newLine)) {
                            // 跳过 BOM
                            if (!newLine.empty() &&
                                (unsigned char)newLine[0] == 0xEF &&
                                newLine.size() >= 3 &&
                                (unsigned char)newLine[1] == 0xBB &&
                                (unsigned char)newLine[2] == 0xBF) {
                                newLine = newLine.substr(3);
                            }

                            newLine = regex_replace(newLine, logPrefix, "");

                            if (rawMode) {
                                cout << newLine << endl;
                            }
                            else {
                                auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
                                tm localTime{};
                                localtime_s(&localTime, &now);

                                cout << "[CS2] "
                                    << put_time(&localTime, "%m/%d %H:%M:%S")
                                    << " " << newLine << endl;
                            }
                        }

                        lastSize = file.tellg();
                        if (lastSize == -1) {
                            lastSize = fileSize;
                        }
                    }
                }
            }

          

            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    */
}
