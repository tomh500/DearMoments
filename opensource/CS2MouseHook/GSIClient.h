#pragma once
#include <functional>
#include <string>

// 启动 GSI HTTP 监听
void StartGSIListener(std::function<void(const std::string&)> onJson);
