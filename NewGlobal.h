#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <SDL_mixer.h>  // 假设Mix_Chunk定义在这里

extern bool low_memory;