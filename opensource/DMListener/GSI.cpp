// GSI.cpp

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


using json = nlohmann::json;
std::atomic<bool> gsi_thread_running(false);  // ���ڼ���߳��Ƿ���������
std::atomic<bool> is_playing(false);  // ���ڱ���Ƿ�����Ч�ڲ���
std::thread gsi_thread;  // ��̨�߳�



// ���Ż�ɱ��ʾ��Ч
void PlayKillSound(int kills_count, bool match, float vol) {
    // �������Ч���ڲ��ţ�����ֹͣ��
    if (is_playing.load()) {
        Mix_HaltMusic();  // ֹͣ��ǰ��Ч�Ĳ���
        while (Mix_PlayingMusic()) {
            SDL_Delay(100);  // �ȴ���Чֹͣ
        }
    }

    // �����µ���Ч�����߳�
    std::thread([kills_count, match, vol]() {
        is_playing.store(true);  // �����Ч���ڲ���

        if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
            std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
            is_playing.store(false);
            return;
        }

        std::string sound_file;

        // ��� match Ϊ true�����ݻ�ɱ��������Ч�����򲥷� extra.mp3
        if (match) {
            switch (kills_count) {
            case 1:
                sound_file = "src/CS2/resources/1.mp3";
                break;
            case 2:
                sound_file = "src/CS2/resources/2.mp3";
                break;
            case 3:
                sound_file = "src/CS2/resources/3.mp3";
                break;
            case 4:
                sound_file = "src/CS2/resources/4.mp3";
                break;
            case 5:
                sound_file = "src/CS2/resources/5.mp3";
                break;
            default:
                sound_file = "src/CS2/resources/extra.mp3";
                break;
            }
        }
        else {
            // match Ϊ false������ extra.mp3
            sound_file = "src/CS2/resources/extra.mp3";
        }

        Mix_Music* music = Mix_LoadMUS(sound_file.c_str());
        if (music == nullptr) {
            std::cerr << "Failed to load sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
            is_playing.store(false);
            return;
        }

        if (Mix_PlayMusic(music, 1) == -1) {
            std::cerr << "Mix_PlayMusic failed! SDL_mixer Error: " << Mix_GetError() << std::endl;
            is_playing.store(false);
            return;
        }

        // �� vol ת��Ϊ SDL ������������Χ�� 0 �� MIX_MAX_VOLUME
        int sdl_volume = static_cast<int>(std::round(vol * MIX_MAX_VOLUME));
        if (sdl_volume > MIX_MAX_VOLUME) {
            sdl_volume = MIX_MAX_VOLUME;  // ȷ���������������ֵ
        }

        // ��������
        Mix_VolumeMusic(sdl_volume);  // ����������С��vol �Ǹ���ֵ

        while (Mix_PlayingMusic()) {
            SDL_Delay(100);  // �ȴ���Ч�������
        }

        Mix_FreeMusic(music);
        Mix_CloseAudio();
        is_playing.store(false);  // ��Ч������ɣ����±��
        }).detach();  // ʹ�� detach �����첽�߳�
}

// �����ɱ�����ĺ���
void ReportKill(int kills_count) {
    switch (kills_count) {
    case 0:
        break;  // ����� 0 ɱ�������κβ���
    case 1:
        std::cout << "One kill!" << std::endl;
        break;
    case 2:
        std::cout << "Double kill!" << std::endl;
        break;
    case 3:
        std::cout << "Triple kill!" << std::endl;
        break;
    case 4:
        std::cout << "Quadra kill!" << std::endl;
        break;
    case 5:
        std::cout << "ACE!" << std::endl;
        break;
    default:
        std::cout << "Multiple kills!" << std::endl;
        break;
    }
}

// �̰߳�ȫ����
std::queue<int> kills_queue;
std::mutex queue_mutex;

// ���ڱ��汾�ش洢�Ļ�ɱ���������ظ�������Ч
int roundkill_local = -1;  // ��ʼ��Ϊһ�������ܵ���ֵ
bool is_first_data_received = true;  // ����Ƿ��ǵ�һ�ν��յ�����

// �� killing_sound.json �ļ���ȡ����
bool LoadKillingSoundConfig(bool& match, float& vol) {
    std::ifstream config_file("src/CS2/resources/killing_sound.json");
    if (!config_file.is_open()) {
        std::cerr << "Failed to open killing_sound.json file!" << std::endl;
        return false;
    }

    json config;
    config_file >> config;

    if (config.contains("match")) {
        match = config["match"].get<bool>();
    }

    if (config.contains("vol")) {
        vol = config["vol"].get<float>();
    }

    return true;
}

// �������ݵ��߳�
void ReceiveData(httplib::Server& svr, bool match, float vol) {
    svr.Post("/", [&match, &vol](const httplib::Request& req, httplib::Response& res) {
        std::string body = req.body;
        std::cout << "Received body: " << body << std::endl;

        try {
            json j = json::parse(body);
            std::cout << "Received GSI data:" << std::endl;
            std::cout << j.dump(4) << std::endl;

            if (j.contains("player") && j["player"].contains("state") && j["player"]["state"].contains("health")) {
                int health = j["player"]["state"]["health"].get<int>();

                if (health > 0) {  // �����Ȼ����
                    if (j["player"]["state"].contains("round_kills")) {
                        int round_kills = j["player"]["state"]["round_kills"].get<int>();
                        std::lock_guard<std::mutex> lock(queue_mutex);  // ����

                        // ����ǵ�һ�ν��յ������һ�ɱ��С�ڵ���5���򲥷���Ч
                        if (is_first_data_received) {
                            if (round_kills <= 5) {
                                roundkill_local = round_kills;
                                kills_queue.push(round_kills);
                                is_first_data_received = false;  // ��ǵ�һ�������ѽ���
                            }
                            // �����һ�����ݴ���5���򲻲�����Ч
                            else {
                                roundkill_local = round_kills;
                                is_first_data_received = false;  // ��ǵ�һ�������ѽ���
                            }
                        }
                        // �����ǰ�������Ļ�ɱ�����ϴδ洢�Ĳ�ͬ���򲥷���Ч������ local ����
                        else if (round_kills != roundkill_local) {
                            roundkill_local = round_kills;  // ���´洢�Ļ�ɱ��
                            kills_queue.push(round_kills);  // ����ɱ���������
                        }
                    }
                }
                else {
                    std::cout << "Player is dead and spectating!" << std::endl;
                }
            }

            res.set_content("OK", "text/plain");
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            res.status = 400;
        }
        });

    svr.listen("127.0.0.1", 12345);
}

// ��װ����ĺ���
void RunGSI() {
    httplib::Server svr;

    bool match = true;
    float vol = 1.0f;

    // ��������
    if (!LoadKillingSoundConfig(match, vol)) {
        return;
    }

    // �����������ݵ��߳�
    std::thread receive_thread(ReceiveData, std::ref(svr), match, vol);

    // ÿ 50 ������һ�ζ��У�������Ч
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::lock_guard<std::mutex> lock(queue_mutex);  // ����
        if (!kills_queue.empty()) {
            int kills_count = kills_queue.front();
            kills_queue.pop();
            ReportKill(kills_count);  // ���Ż�ɱ����
            PlayKillSound(kills_count, match, vol);  // ���Ż�ɱ��Ч
        }
    }

    receive_thread.join();  // �ȴ������߳̽���
}

void StartGSIThread() {
    if (!gsi_thread_running.load()) {
        gsi_thread = std::thread(RunGSI);
        gsi_thread_running.store(true);
        std::cout << "GSI thread started." << std::endl;
    }
}

// ֹͣ GSI �߳�
void StopGSIThread() {
    if (gsi_thread_running.load()) {
        // ͨ��Ҫ�ں�̨�̺߳����ڲ������˳���־�����ŵ�ֹͣ�߳�
        gsi_thread_running.store(false);
        if (gsi_thread.joinable()) {
            gsi_thread.join();  // �ȴ��߳̽���
        }
        std::cout << "GSI thread stopped." << std::endl;
    }
}