#pragma once
#include <string>
#include <vector>

struct AppStore {
    enum Status { NORMAL, HIDE, DEV } status = NORMAL;
    int id = 0;
    std::string name;
    std::string url;
};

// 全局列表，存储获取到的 AppStore 条目
extern std::vector<AppStore> AppStoreList;

// 调试开关
extern bool debug;

// 获取 AppStore 列表
// 返回 true 表示成功
bool fetchAppStoreList(const std::string& url);
