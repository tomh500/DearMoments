// OverlayKilling.h
#pragma once

#include <SDL.h>      // 对于 SDL_Window 和 SDL_Renderer，如果在头文件中需要可包含
#include <SDL2/SDL_image.h>

// 外部配置变量，由你的主程序或其他模块定义并设置
extern bool low_memory;
extern int round_kills;

// 可选：非 low_memory 模式下预加载所有 GIF
void InitOverlayKilling();

// 异步播放指定编号的 GIF 动画（1–5），动画循环完毕后自动关闭
// 返回 0 表示调用成功，-1 表示失败
int ShowKillingIcon(int kills);

// 立即停止并隐藏当前动画
void HideKillingIcon();
