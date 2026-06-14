#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>

extern std::mutex queue_mutex;
extern std::condition_variable queue_cv;
extern std::queue<int> kills_queue;
extern bool g_running;