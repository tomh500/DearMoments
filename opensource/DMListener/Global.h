#pragma once
#include <filesystem>

namespace fs = std::filesystem;
extern bool isCS2Running;
extern int debug;
extern int localVersion;
extern HINSTANCE hInst;                              
extern fs::path RootPath;
extern fs::path cachePath;