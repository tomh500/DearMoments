#pragma once
#include <string>
#include <functional>

// 启动 GSI TCP 客户端
void StartGSIListener(std::function<void(const std::string&)> onJson);
