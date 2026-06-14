#pragma once
#define MP_WNDPROC LRESULT CALLBACK
#include <string>
#include <vector>

// 音乐结构体


// 全局音乐列表
//extern std::vector<Music> musicList;

struct Music {
    int id;
    std::string name;      // UTF-8
    std::wstring wname;    // 转换后的宽字符串
    std::string url;
    bool isLocal = false;
    enum Status { NORMAL, HIDE, DEV } status = NORMAL;

};

// 获取音乐列表
bool fetchMusicList(const std::string& url);

// 下载文件
bool downloadFile(const std::string& url, const std::string& filename);

// 检查是否支持的音乐格式
bool isSupportedFormat(const std::string& url);

// 获取临时文件路径
std::string getTempFilePath(const std::string& extension);

// 应用系统代理设置
void applyProxySettings(void* curl);

extern bool BypassProxy;

extern bool isBusy ;
MP_WNDPROC MusicPlayerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);