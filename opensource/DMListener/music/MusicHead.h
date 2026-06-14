

#ifndef MUSIC_HEAD_H
#define MUSIC_HEAD_H

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <iostream>
#include <algorithm>
#include <sstream>

// 第三方库
#include "miniaudio.h"
#include <curl/curl.h>
#include <mmdeviceapi.h>
#include <AudioClient.h>
#include "MusicPlayer.h"
#include <SDL.h>
#include <SDL_mixer.h>


enum EngineType { LATEST, LEGACY };
extern EngineType MNG; // 这里是声明

namespace MA_Audio {
    // --- 宏定义 ---
#define WM_APP_MUSIC_DONE (WM_APP + 2)
#define ID_PAUSE_BUTTON    3401
#define ID_VOLUME_SLIDER   3105
#define ID_PROGRESS_SLIDER 3343

// --- 枚举与结构体 ---
// --- 结构体定义 (全工程只准在这里定义一次) ---



// 在全局变量区增加
    struct DeviceItem {
        ma_device_id id;
        wstring name;
    };




    // --- 枚举与宏 ---
   enum PlayMode { MODE_SINGLE, MODE_LOOP, MODE_SEQUENCE };
#define WM_APP_MUSIC_DONE (WM_APP + 2)

    // --- 全局变量声明 (extern) ---
    // 音频核心
    extern ma_device g_device;
    extern ma_decoder g_decoder;
    extern ma_context g_context;
    extern std::mutex g_audioMutex;
    extern std::atomic<bool> g_isDecoderValid;
    extern std::atomic<bool> g_isDecoderInited;
    extern bool g_contextInited;

    // 播放控制
    extern atomic<bool> isPaused;
    extern atomic<int> currentVolume;
    extern atomic<bool> isDownloading;
    extern PlayMode playMode;
    extern string currentTempFile;

    // UI 句柄与数据
    extern HWND globalHwnd;
    extern HWND hListBox;
    extern HWND hSearchEdit;
    extern HWND hDeviceCombo;
    extern HWND hProgressSlider;
    extern HWND hShowHiddenCheckbox;
    extern vector<Music> musicList;
    extern vector<Music> visibleMusicList;
    extern vector<DeviceItem> g_realDeviceList;
    // --- 线程句柄声明 ---
    extern std::thread g_fetchThread;   // 歌曲列表下载线程
    extern std::thread g_refreshThread; // UI 进度刷新线程
    extern std::thread g_workThread;    // 歌曲文件下载线程（playTrackByIndex 里的）

    // 其他
    extern bool BypassProxy;
    extern atomic<bool> g_threadExit;
    extern HANDLE MusicMutex;

    // --- 函数原型 ---
    string getExtFromUrl(const string& url);
    void DebugMsg(const wstring& msg);
    void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void playTrackByIndex(int index);
    int GetSystemSampleRate();
    void AudioLog(const char* format, ...);

    // 网络
    bool downloadFile(const string& url, const string& filename);
    bool fetchMusicList(const string& url);
    void applyProxySettings(CURL* curl);
    size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // 工具
    wstring utf8ToWide(const string& str);
    string getTempFilePath(const string& extension);
    void buildMusicListUI();
    vector<wstring> GetAudioDevices();
    void UpdateDeviceComboState(bool isBusy);


}
struct Mix_Music;
namespace Legacy_Audio
{
    // --- 变量声明 ---
    extern HWND hListBox;
    extern HWND hSearchEdit;
    extern HWND hProgressSlider;
    extern HWND hDeviceCombo;
    extern HWND hShowHiddenCheckbox;

    // 统一声明方式，不要写两遍
    extern Mix_Music* currentTrack;
    extern std::mutex g_audioMutex;
    extern std::atomic<bool> g_threadExit;
    extern HANDLE MusicMutex;

    // --- 函数声明 ---
    LRESULT CALLBACK MusicPlayerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}



#endif
void CleanupMusicPlayer();