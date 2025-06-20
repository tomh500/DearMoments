#if defined(_WIN32) || defined(_WIN64)
#include <SDL.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <SDL_syswm.h>
#include <windows.h> 
#include "Global.h"

namespace fs = std::filesystem;

std::atomic<bool> flash_running{ false };
std::thread flash_thread;



// 共享资源指针
SDL_Window* flash_window = nullptr;
SDL_Renderer* flash_renderer = nullptr;
SDL_Texture* flash_texture = nullptr;

void gsi_playerflash() {
    if (flash_running) return;
    flash_running = true;

    flash_thread = std::thread([]() {
        fs::path image_path = "Userspace/gsi/images/flash.bmp";

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "[FLASH] SDL_Init failed: " << SDL_GetError() << "\n";
            flash_running = false;
            return;
        }

        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);
        int screen_w = dm.w;
        int screen_h = dm.h;

        flash_window = SDL_CreateWindow("FlashOverlay",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            screen_w, screen_h, SDL_WINDOW_BORDERLESS);

        if (!flash_window) {
            std::cerr << "[FLASH] Failed to create window: " << SDL_GetError() << "\n";
            flash_running = false;
            return;
        }

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(flash_window, &wmInfo)) {
            HWND hwnd = wmInfo.info.win.window;

            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        }

        SDL_SetWindowAlwaysOnTop(flash_window, SDL_TRUE);
        SDL_SetWindowFullscreen(flash_window, SDL_WINDOW_FULLSCREEN_DESKTOP);

        flash_renderer = SDL_CreateRenderer(flash_window, -1, SDL_RENDERER_ACCELERATED);
        if (!flash_renderer) {
            std::cerr << "[FLASH] Failed to create renderer: " << SDL_GetError() << "\n";
            return;
        }

        SDL_Surface* bmp = nullptr;

        if (!low_memory) {
            if (!fs::exists(image_path)) {
                std::cerr << "[FLASH] Image not found: " << image_path << "\n";
                return;
            }

            std::ifstream file(image_path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "[FLASH] Failed to open image file.\n";
                return;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size)) {
                std::cerr << "[FLASH] Failed to read image file.\n";
                return;
            }

            SDL_RWops* rw = SDL_RWFromMem(buffer.data(), static_cast<int>(buffer.size()));
            if (!rw) {
                std::cerr << "[FLASH] Failed to create RWops: " << SDL_GetError() << "\n";
                return;
            }

            bmp = SDL_LoadBMP_RW(rw, 1);
            if (!bmp) {
                std::cerr << "[FLASH] Failed to load BMP from memory: " << SDL_GetError() << "\n";
                return;
            }
        }
        else {
            bmp = SDL_LoadBMP(image_path.string().c_str());
            if (!bmp) {
                std::cerr << "[FLASH] Failed to load image from disk: " << SDL_GetError() << "\n";
                return;
            }
        }

        flash_texture = SDL_CreateTextureFromSurface(flash_renderer, bmp);
        SDL_FreeSurface(bmp);
        if (!flash_texture) {
            std::cerr << "[FLASH] Failed to create texture: " << SDL_GetError() << "\n";
            return;
        }

        SDL_SetTextureBlendMode(flash_texture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawBlendMode(flash_renderer, SDL_BLENDMODE_BLEND);
        SDL_ShowCursor(SDL_DISABLE);

        const int fade_duration_ms = 3000;
        const int step_ms = 30;
        int alpha = 255;

        while (flash_running && alpha > 0) {
            alpha = std::max(0, alpha - (255 * step_ms / fade_duration_ms));
            SDL_SetTextureAlphaMod(flash_texture, alpha);

            SDL_SetRenderDrawColor(flash_renderer, 0, 0, 0, 0);
            SDL_RenderClear(flash_renderer);
            SDL_RenderCopy(flash_renderer, flash_texture, nullptr, nullptr);
            SDL_RenderPresent(flash_renderer);
            SDL_Delay(step_ms);
        }

        // 注意：SDL 清理已移到 gsi_endplayerflash()
        });
}

void gsi_endplayerflash() {
    if (!flash_running) return;

    flash_running = false;
    if (flash_thread.joinable())
        flash_thread.join();

    // 销毁 SDL 资源
    if (flash_texture) {
        SDL_DestroyTexture(flash_texture);
        flash_texture = nullptr;
    }
    if (flash_renderer) {
        SDL_DestroyRenderer(flash_renderer);
        flash_renderer = nullptr;
    }
    if (flash_window) {
        SDL_DestroyWindow(flash_window);
        flash_window = nullptr;
    }

    SDL_Quit();
}

#endif
