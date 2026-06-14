#pragma once
#include <string>
#include <vector>
#include <SDL.h>
#include <SDL_mixer.h>
// 音乐结构体


// 全局音乐列表
//extern std::vector<Music> musicList;


struct Music {
    int id;
    std::string name;      // UTF-8
    std::wstring wname;    // 转换后的宽字符串
    std::string url;
    enum Status { NORMAL, HIDE, DEV } status = NORMAL;

};

