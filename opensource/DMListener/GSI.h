#pragma once
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <SDL_mixer.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <chrono>
#include <fstream>
void PlayKillSound(int kills_count, bool match, float vol);
void ReportKill(int kills_count);
bool LoadKillingSoundConfig(bool& match, float& vol);
void ReceiveData(httplib::Server& svr, bool match, float vol);
void RunGSI();