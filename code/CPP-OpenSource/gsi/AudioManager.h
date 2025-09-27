#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <SDL_mixer.h>

#include "../core/SafeOperation.h"
#include "../core/Logger.h"
#include "../core/Constants.h"

namespace fs = std::filesystem;

/**
 * @brief Comprehensive audio management system for GSI
 * 
 * Extracts all audio-related functionality from GSI.cpp:
 * - Bomb sound management with RAII lifecycle
 * - Buy sound management with threading
 * - Live phase management
 * - Audio utilities and safe operations
 * - Thread-safe access to global audio state
 * 
 * Preserves all existing functionality and global variables for backward compatibility.
 */
class AudioManager {
private:
    // Encapsulated audio state - no more extern dependencies
    std::atomic<int> bombChannel{-1};
    std::atomic<bool> bombSoundPlaying{false};
    Core::Safe::MixChunkPtr bombChunk;
    std::atomic<bool> isBuying{false};
    std::thread buyThread;
    std::mutex buyMutex;
    
    AudioManager() = default;

public:
    /**
     * @brief Meyers singleton pattern for global access - thread-safe since C++11
     */
    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    /**
     * @brief Play bomb sound with proper RAII chunk management
     * 
     * Extract from PlayBombSound function:
     * - Stop any existing bomb sounds
     * - Setup channel finished callback
     * - Safe audio loading and playback
     * - Volume management and channel allocation
     * - Integration with g_bombChunk for lifecycle management
     * 
     * @param sound_file Path to bomb sound file
     * @param vol Volume level (0.0 to 1.0)
     */
    inline void playBombSound(const fs::path& sound_file, float vol) {
        // Убедиться что только один звук бомбы играет
        stopBombSound();
        
        // Настраиваем callback для автоматической очистки при завершении аудио
        Mix_ChannelFinished(bombChannelFinishedWrapper);

        // ================== Безопасная загрузка аудио ==================
        auto safeLoadResult = Core::Safe::SafeAudioOperation::loadSound(sound_file.string());
        if (safeLoadResult.isError()) {
            Logger::getInstance().log(LogLevel::ERROR, "Safe audio load failed: " + safeLoadResult.getError().toString());
            return;
        }
        
        // Перемещаем chunk в инкапсулированный держатель для обеспечения времени жизни
        bombChunk = safeLoadResult.getValue();

        // ================== Безопасное воспроизведение аудио ==================
        auto safePlayResult = Core::Safe::SafeAudioOperation::playSound(bombChunk, -1);
        if (safePlayResult.isError()) {
            LOG_RESULT_ERROR(safePlayResult);
            bombChunk.reset();  // Очищаем при ошибке
            return;
        }
        
        int channel = safePlayResult.getValue();
        extern bool debug_mode;
        
        bombChannel = channel;
        Mix_Volume(channel, static_cast<int>(MIX_MAX_VOLUME * vol));
        bombSoundPlaying = true;

        if (debug_mode) {
            std::cout << "[BOMB] Playing bomb sound on channel " << channel << "\n";
        }
        LOG_DEBUG_STREAM("Playing bomb sound on channel " << channel);
    }

    /**
     * @brief Stop bomb sound with proper cleanup
     * 
     * Extract from StopBombSound function:
     * - Stop channel playback  
     * - Reset global state variables
     * - Release chunk resources
     * - Debug logging
     */
    inline void stopBombSound() {
        extern bool debug_mode;
        
        int channel = bombChannel.load();
        if (channel >= 0) {
            Mix_HaltChannel(channel);
            bombChannel = -1;
            bombSoundPlaying = false;
            bombChunk.reset();  // Освобождаем chunk при принудительной остановке
            if (debug_mode) {
                std::cout << "[BOMB] Bomb sound stopped and chunk released\n";
            }
        }
    }

    /**
     * @brief Handle bomb channel finished callback
     * 
     * Extract from BombChannelFinished function:
     * - Channel cleanup when audio completes
     * - Reset global state variables
     * - Release chunk resources
     * - Debug logging
     * 
     * @param channel SDL audio channel that finished
     */
    inline void bombChannelFinished(int channel) {
        extern bool debug_mode;
        
        if (channel == bombChannel) {
            bombChannel = -1;
            bombSoundPlaying = false;
            bombChunk.reset();  // Освобождаем chunk когда канал завершается
            if (debug_mode) {
                std::cout << "[BOMB] Bomb sound finished on channel " << channel << ", chunk released\n";
            }
        }
    }

    /**
     * @brief Start buy phase with continuous buy sound loop
     * 
     * Extract from OnBuyPhaseStart function:
     * - Thread-safe buy state management
     * - Start buy sound loop thread
     * - Prevent duplicate buy phase starts
     * 
     * @param vol Volume level for buy sounds
     */
    inline void startBuyPhase(float vol) {
        {
            std::lock_guard<std::mutex> lk(buyMutex);
            if (isBuying) {
                // 已经在播放buy音效，不重复推送
                return;
            }
            isBuying = true;
        }

        // 启动buy音效循环线程
        buyThread = std::thread([this, vol]() { this->buySoundLoop(vol); });
    }

    /**
     * @brief End buy phase and cleanup
     * 
     * Extract from OnBuyPhaseEnd function:
     * - Stop buy sound loop
     * - Join buy thread
     * - Reset buy state
     */
    inline void endBuyPhase() {
        {
            std::lock_guard<std::mutex> lk(buyMutex);
            isBuying = false;
        }
        if (buyThread.joinable()) {
            buyThread.join();
        }
    }

    /**
     * @brief Buy sound continuous loop logic
     * 
     * Extract from BuySoundLoop function:
     * - Continuous buy sound playback
     * - Safe file path validation
     * - Safe audio loading and playback
     * - Volume management
     * - Thread-safe loop control
     * 
     * @param vol Volume level for buy sounds
     */
    inline void buySoundLoop(float vol) {
        extern bool use_ogg;
        
        fs::path base_path = Constants::Paths::GSI_SOUND_PATH;
        std::string ext = use_ogg ? ".ogg" : ".wav";
        fs::path buy_sound = base_path / ("buy" + ext);
        
        // ================== 安全文件路径验证 ==================
        auto pathValidation = Core::Safe::SafeFileOperation::validatePath(buy_sound.string());
        if (pathValidation.isError()) {
            Logger::getInstance().log(LogLevel::ERROR, "Buy sound path validation failed: " + pathValidation.getError().toString());
            return;
        }

        int buy_channel = Constants::Audio::BUY_CHANNEL;  // 用于播放buy音效的指定通道

        while (true) {
            {
                std::unique_lock<std::mutex> lk(buyMutex);
                if (!isBuying) break;
            }

            // Если buy звук не воспроизводится, воспроизвести его
            if (!Mix_Playing(buy_channel)) {
                // ================== Безопасная загрузка аудио ==================
                auto safeBuyLoadResult = Core::Safe::SafeAudioOperation::loadSound(buy_sound.string());
                if (safeBuyLoadResult.isError()) {
                    LOG_RESULT_ERROR(safeBuyLoadResult);
                    break;
                }
                
                auto chunk = safeBuyLoadResult.getValue();
                int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
                sdl_volume = (sdl_volume > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : sdl_volume;
                Mix_VolumeChunk(chunk.get(), sdl_volume);
                
                // ================== Безопасное воспроизведение аудио ==================
                auto safeBuyPlayResult = Core::Safe::SafeAudioOperation::playSound(chunk, buy_channel);
                if (safeBuyPlayResult.isError()) {
                    LOG_RESULT_ERROR(safeBuyPlayResult);
                    break;
                }

                // Ожидать завершения воспроизведения (chunk автоматически освободится)
                while (Mix_Playing(buy_channel)) {
                    SDL_Delay(50);
                }
                // chunk автоматически освобождается при выходе из области видимости
            }
            else {
                // buy音效还在播放，等一会儿再检测
                SDL_Delay(50);
            }
        }
    }

    /**
     * @brief Start live phase and stop all sounds
     * 
     * Extract from OnLivePhaseStart function:
     * - Stop all audio channels
     * - End buy phase if active
     * - Reset buy state
     * 
     * @param vol Volume level (unused but kept for compatibility)
     */
    inline void startLivePhase(float vol) {
        // 停止所有音效
        Mix_HaltChannel(-1);

        // 如果之前处于buy循环，结束buy循环线程
        {
            std::lock_guard<std::mutex> lk(buyMutex);
            isBuying = false;
        }
        if (buyThread.joinable()) {
            buyThread.join();
        }

        // 标记非buy状态
        isBuying = false;
    }

    /**
     * @brief Stop all sounds and reset global state
     * 
     * Extract from StopAllSounds function:
     * - Stop all SDL audio channels
     * - Reset bomb sound state
     * - Release bomb chunk resources
     */
    inline void stopAllSounds() {
        Mix_HaltChannel(-1);
        bombChannel = -1;
        bombSoundPlaying = false;
        bombChunk.reset();  // Освобождаем chunk при остановке всех звуков
    }

    /**
     * @brief Check if bomb sound is currently playing
     * 
     * Provides thread-safe access to bomb playback state without exposing internals.
     * Used by GameStateHandler to wait for bomb sound completion before buy sound.
     * 
     * @return true if bomb sound is currently playing, false otherwise
     */
    inline bool isBombPlaying() const { 
        int ch = bombChannel.load(); 
        return ch >= 0 && Mix_Playing(ch); 
    }

    /**
     * @brief Get current bomb channel number
     * 
     * Optional helper for logging and debugging bomb sound state.
     * Returns thread-safe copy of current bomb channel.
     * 
     * @return Current bomb channel number (-1 if no bomb sound playing)
     */
    inline int getBombChannel() const { 
        return bombChannel.load(); 
    }

private:
    /**
     * @brief Static wrapper for bomb channel finished callback
     * Required because SDL_mixer expects C-style function pointer
     * Scoped to only react to bomb channels to avoid affecting other audio users
     */
    static void bombChannelFinishedWrapper(int channel) {
        // Only handle bomb channels to avoid interfering with other audio
        auto& audioMgr = AudioManager::getInstance();
        if (channel == audioMgr.bombChannel.load()) {
            audioMgr.bombChannelFinished(channel);
        }
    }
};


// Backward compatibility wrappers removed to avoid ODR violations
// All wrappers are now only defined in GSI.cpp for entry-point orchestration