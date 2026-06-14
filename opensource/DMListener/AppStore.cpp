#include "AppStoreFetcher.h"
#include <windows.h>
#include <curl/curl.h>
#include <sstream>
#include <fstream>
#include <algorithm>

bool debug = false;
std::vector<AppStore> AppStoreList;

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* data = static_cast<std::string*>(userp);
    data->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

static std::string trim(const std::string& s) {
    auto l = s.find_first_not_of(" \t\r\n");
    if (l == std::string::npos) return "";
    auto r = s.find_last_not_of(" \t\r\n");
    return s.substr(l, r - l + 1);
}

bool fetchAppStoreList(const std::string& url) {
    if (debug) {
        MessageBoxA(NULL, ("开始获取歌曲列表: " + url).c_str(), "调试信息", MB_OK);
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (debug) MessageBoxA(NULL, "curl 初始化失败", "错误", MB_OK);
        return false;
    }

    std::string rawData;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rawData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (curl_easy_perform(curl) != CURLE_OK) {
        if (debug) MessageBoxA(NULL, "获取音乐列表失败", "错误", MB_OK);
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_cleanup(curl);

    AppStoreList.clear();
    std::istringstream stream(rawData);
    std::string line;
    AppStore current;
    int step = 0;

    while (getline(stream, line)) {
        auto t = trim(line);
        if (t.empty()) continue;

        switch (step) {
        case 0:
            if (isdigit(t[0]) && t.back() == ':') {
                current = AppStore();
                current.id = stoi(t.substr(0, t.size() - 1));
                step = 1;
            }
            break;
        case 1:
            if (t.rfind("- ", 0) == 0) {
                current.name = t.substr(2);
                step = 2;
            }
            break;
        case 2:
            if (t.rfind("- ", 0) == 0) {
                current.url = t.substr(2);
                step = 3;
            }
            break;
        case 3:
            if (t.rfind("- ", 0) == 0) {
                std::string flag = t.substr(2);
                if (flag == "hide") current.status = AppStore::HIDE;
                else if (flag == "dev") current.status = AppStore::DEV;
                else current.status = AppStore::NORMAL;
            }
            else {
                current.status = AppStore::NORMAL;
            }
            AppStoreList.push_back(current);
            step = 0;
            break;
        }
    }

    if (step == 3) {
        current.status = AppStore::NORMAL;
        AppStoreList.push_back(current);
    }

    std::sort(AppStoreList.begin(), AppStoreList.end(), [](const AppStore& a, const AppStore& b) {
        return a.id < b.id;
        });

    if (debug) {
        MessageBoxA(NULL, ("成功解析歌曲列表，共 " + std::to_string(AppStoreList.size()) + " 首歌曲").c_str(), "调试信息", MB_OK);
    }

    return true;
}
