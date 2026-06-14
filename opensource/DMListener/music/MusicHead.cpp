
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "Tools.h"
#include "MusicHead.h"
#include <windowsx.h>
#include <SDL.h>
#include <SDL_mixer.h>
#pragma pack(push, 1)
#pragma comment(lib, "Ole32.lib") // GetSystemSampleRate 需要

EngineType MNG = LATEST;

namespace MA_Audio {
	bool BypassProxy = false;
	// 声明一个全局线程句柄代替 detach
	vector<DeviceItem> g_realDeviceList;

	vector<wstring> GetAudioDevices() {
		g_realDeviceList.clear();
		vector<wstring> names = { L"系统默认设备" };
		g_realDeviceList.push_back({ {}, L"Default" });

		// 1. 确保 Context 初始化
		if (!g_contextInited) {
			// 尝试默认初始化
			if (ma_context_init(NULL, 0, NULL, &g_context) != MA_SUCCESS) {
				return names; // 失败则直接返回默认
			}
			g_contextInited = true;
		}

		ma_device_info* pPlaybackInfos = nullptr;
		ma_uint32 count = 0;

		// 2. 枚举设备
		ma_result result = ma_context_get_devices(&g_context, &pPlaybackInfos, &count, NULL, NULL);

		if (result == MA_SUCCESS) {
			AudioLog("Found %d audio devices", count);
			for (ma_uint32 i = 0; i < count; ++i) {
				// 打印每个设备名到调试台，看看底层是否真的拿到了
				// AudioLog("Device %d: %s", i, pPlaybackInfos[i].name);

				g_realDeviceList.push_back({ pPlaybackInfos[i].id, utf8ToWide(pPlaybackInfos[i].name) });
				names.push_back(utf8ToWide(pPlaybackInfos[i].name));
			}
		}
		else {
			AudioLog("ma_context_get_devices failed: %d", result);
		}

		return names;
	}

	void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		// 1. 核心保护：如果开关没打开，或者正在初始化，立刻刷静音并返回
		// 不要尝试拿锁，因为主线程在 uninit 时会握住锁，这里拿不到锁会直接 memset，这是对的。
		if (!g_isDecoderValid.load() || !g_isDecoderInited.load()) {
			memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
			return;
		}

		if (g_audioMutex.try_lock()) {
			ma_uint64 framesRead;
			// 关键点：检查解码器输出格式和设备输入格式是否统一
			ma_result result = ma_decoder_read_pcm_frames(&g_decoder, pOutput, frameCount, &framesRead);

			if (framesRead < frameCount) {
				// 剩下的部分补静音，防止最后一点内存垃圾产生爆音
				ma_uint8* pRemainingOutput = (ma_uint8*)pOutput + (framesRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
				memset(pRemainingOutput, 0, (frameCount - framesRead) * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));

				g_isDecoderValid = false;
				PostMessage((HWND)pDevice->pUserData, WM_APP + 2, 0, 0);
			}
			g_audioMutex.unlock();
		}
		else {
			// 拿不到锁说明主线程正在改内存，为了防止轰鸣声，必须刷静音
			memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
		}
	}

	void AudioLog(const char* format, ...) {
		if (debug == 1) { // 只有在你的 Global.h 里的 debug 为 1 时才输出
			char buffer[512];
			va_list args;
			va_start(args, format);
			vsnprintf(buffer, sizeof(buffer), format, args);
			va_end(args);
			std::cout << "[AudioLog] " << buffer << std::endl;
		}
	}
	void UpdateDeviceComboState(bool isBusy) {
		if (!hDeviceCombo) return;

		// 只要是在下载，或者设备正在运行（且没暂停），就禁用
		bool isPlaying = ma_device_is_started(&g_device);
		bool busy = (isDownloading.load() || (g_isDecoderValid && isPlaying && !isPaused));

		EnableWindow(hDeviceCombo, !busy);
	}

	void playTrackByIndex(int index) {
		if (index < 0 || index >= (int)visibleMusicList.size() || isDownloading) return;

		// 1. 如果有旧线程，先清理
		if (g_workThread.joinable()) g_workThread.join();

		const auto& music = visibleMusicList[index];

		g_workThread = std::thread([index, music]() {
			// ---【重点：开始工作，禁用下拉框】---
			isDownloading = true;
			UpdateDeviceComboState(true);

			string ext = getExtFromUrl(music.url);
			string path = getTempFilePath(ext);

			if (downloadFile(music.url, path)) {
				g_isDecoderValid = false;
				std::lock_guard<std::mutex> lock(g_audioMutex);

				if (g_isDecoderInited) {
					ma_decoder_uninit(&g_decoder);
					g_isDecoderInited = false;
				}

				ma_format targetFormat = g_device.playback.format;
				ma_uint32 targetSR = g_device.sampleRate;
				if (targetSR == 0) targetSR = 48000;

				ma_decoder_config decoderCfg = ma_decoder_config_init(targetFormat, 2, targetSR);

				if (ma_decoder_init_file(path.c_str(), &decoderCfg, &g_decoder) == MA_SUCCESS) {
					if (!currentTempFile.empty()) remove(currentTempFile.c_str());
					currentTempFile = path;
					g_isDecoderInited = true;
					g_isDecoderValid = true;

					// 启动播放
					ma_device_start(&g_device);
					isPaused = false; // 确保暂停状态被重置
				}
			}

			// ---【重点：下载结束，恢复下拉框】---
			// 注意：如果你希望播放全程都不准切，这里就不要恢复
			// 只有当播放停止或切歌时才恢复。这里我们先恢复，保证用户能切设备
			isDownloading = false;
			UpdateDeviceComboState(false);
			});
	}
	static string trim(const string& s) {
		auto l = s.find_first_not_of(" \t\r\n");
		if (l == string::npos) return "";
		auto r = s.find_last_not_of(" \t\r\n");
		return s.substr(l, r - l + 1);
	}
	void applyProxySettings(CURL* curl) {
		HKEY hKey;
		DWORD proxyEnable = 0;
		DWORD bufSize = sizeof(proxyEnable);
		char proxyServer[256] = { 0 };
		bool isProxyOn = false;
		// 1. 读取系统代理设置
		if (RegOpenKeyExA(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
			0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueExA(hKey, "ProxyEnable", nullptr, nullptr,
				reinterpret_cast<LPBYTE>(&proxyEnable), &bufSize) == ERROR_SUCCESS
				&& proxyEnable == 1)
			{
				bufSize = sizeof(proxyServer);
				if (RegQueryValueExA(hKey, "ProxyServer", nullptr, nullptr,
					reinterpret_cast<LPBYTE>(proxyServer), &bufSize) == ERROR_SUCCESS)
				{
					isProxyOn = true;
				}
			}
			RegCloseKey(hKey);
		}
		if (isProxyOn && curl) {
			string proxyStr(proxyServer);
			string selectedProxy;
			// 2. 判断当前 URL 是 http 还是 https
			char* url = nullptr;
			curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
			if (!url) {
				// 如果 EFFECTIVE_URL 获取不到，就使用 CURLOPT_URL（libcurl 没开始请求前）
				curl_easy_getinfo(curl, CURLINFO_PRIVATE, &url); // 可设置为你自己传入的 URL
			}
			bool isHttps = false;
			if (url != nullptr && _strnicmp(url, "https://", 8) == 0) {
				isHttps = true;
			}
			// 3. 解析 ProxyServer 字符串
			if (proxyStr.find('=') != string::npos) {
				// 多协议代理格式
				if (isHttps) {
					auto pos = proxyStr.find("https=");
					if (pos != string::npos) {
						auto semi = proxyStr.find(';', pos);
						selectedProxy = proxyStr.substr(pos + 6, semi - pos - 6);
					}
				}
				if (selectedProxy.empty()) {
					auto pos = proxyStr.find("http=");
					if (pos != string::npos) {
						auto semi = proxyStr.find(';', pos);
						selectedProxy = proxyStr.substr(pos + 5, semi - pos - 5);
					}
				}
			}
			else {
				// 单一代理，直接使用
				selectedProxy = proxyStr;
			}
			// 4. 加上前缀
			if (!selectedProxy.empty() && selectedProxy.find("://") == string::npos) {
				selectedProxy = "http://" + selectedProxy;
			}
			// 5. 应用到 curl
			if (!selectedProxy.empty() && BypassProxy == false) {
				curl_easy_setopt(curl, CURLOPT_PROXY, selectedProxy.c_str());
				curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
				if (debug == 1) {
					wstring wmsg = L"使用系统代理: ";
					wmsg += utf8ToWide(selectedProxy);
					MessageBoxW(NULL, wmsg.c_str(), L"代理设置", MB_OK | MB_ICONINFORMATION);
				}
			}
		}
		else {
			if (debug == 1) {
				MessageBoxW(NULL,
					L"未检测到系统代理，所有请求将直连。",
					L"代理设置",
					MB_OK | MB_ICONINFORMATION);
			}
		}
	}
	void buildMusicListUI() {
		ListView_DeleteAllItems(hListBox);
		BOOL showHidden = SendMessage(hShowHiddenCheckbox, BM_GETCHECK, 0, 0);
		// 获取搜索框文字
		wchar_t szSearch[256] = { 0 };
		GetWindowTextW(hSearchEdit, szSearch, 256);
		wstring searchKey = szSearch;
		// 转小写实现模糊搜索
		transform(searchKey.begin(), searchKey.end(), searchKey.begin(), ::towlower);
		visibleMusicList.clear();
		for (const auto& music : musicList) {
			if (music.status == Music::DEV) continue;
			if (music.status == Music::HIDE && !showHidden) continue;
			// 搜索逻辑
			wstring wname = utf8ToWide(music.name);
			wstring wnameLower = wname;
			transform(wnameLower.begin(), wnameLower.end(), wnameLower.begin(), ::towlower);
			if (!searchKey.empty() && wnameLower.find(searchKey) == wstring::npos) {
				continue; // 不匹配，跳过
			}
			visibleMusicList.push_back(music);
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.iItem = ListView_GetItemCount(hListBox);
			lvi.pszText = (LPWSTR)wname.c_str();
			ListView_InsertItem(hListBox, &lvi);
		}
		InvalidateRect(hListBox, NULL, TRUE);
	}
	size_t writeFile(void* ptr, size_t size, size_t nmemb, void* stream) {
		return fwrite(ptr, size, nmemb, (FILE*)stream);
	}

	bool downloadFile(const string& url, const string& filename) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		FILE* fp = nullptr;
		if (fopen_s(&fp, filename.c_str(), "wb") != 0 || !fp) {
			curl_easy_cleanup(curl);
			return false;
		}

		if (!fp) {
			curl_easy_cleanup(curl);
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		applyProxySettings(curl);

		bool ok = (curl_easy_perform(curl) == CURLE_OK);

		fclose(fp);
		curl_easy_cleanup(curl);
		return ok;
	}



	bool isSupportedFormat(const string& url) {
		auto ext = url.substr(url.find_last_of('.') + 1);
		for (auto& c : ext) c = static_cast<char>(tolower(c));
		return (ext == "mp3" || ext == "ogg" || ext == "wav" || ext == "m4a" || ext == "flac");
	}
	string getTempFilePath(const string& extension) {
		char tempPath[MAX_PATH]{};
		GetTempPathA(MAX_PATH, tempPath);

		static atomic<int> counter{ 0 };
		int id = ++counter;

		char fileName[MAX_PATH]{};
		sprintf_s(fileName, "SQ_%05d.%s", id, extension.c_str());

		return string(tempPath) + fileName;
	}


	bool fetchMusicList(const string& url) {
		if (debug == 1) {
			std::wstring wmsg = L"开始获取歌曲列表:\n";
			wmsg += utf8ToWide(url);
			MessageBoxW(NULL, wmsg.c_str(), L"调试信息", MB_OK);
		}
		CURL* curl = curl_easy_init();
		if (!curl) {
			if (debug == 1) {
				MessageBoxW(NULL, L"curl初始化失败", L"错误", MB_OK);
			}
			return false;
		}
		string rawData;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rawData);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		applyProxySettings(curl);
		if (curl_easy_perform(curl) != CURLE_OK) {
			if (debug == 1) {
				MessageBoxW(NULL, L"获取音乐列表失败", L"错误", MB_OK);
			}
			curl_easy_cleanup(curl);
			return false;
		}
		curl_easy_cleanup(curl);
		musicList.clear();
		istringstream stream(rawData);
		string line;
		Music current;
		int step = 0;
		while (getline(stream, line)) {
			auto t = trim(line);
			if (t.empty()) continue;
			switch (step) {
			case 0:
				if (isdigit(t[0]) && t.back() == ':') {
					current = Music();
					current.id = stoi(t.substr(0, t.size() - 1));
					step = 1;
				}
				break;
			case 1:
				if (t.rfind("- ", 0) == 0) {
					current.name = t.substr(2);
					step = 2;
				}
				break;
			case 2:
				if (t.rfind("- ", 0) == 0) {
					current.url = t.substr(2);
					step = 3;
				}
				break;
			case 3:
				if (t.rfind("- ", 0) == 0) {
					string flag = t.substr(2);
					if (flag == "hide") { current.status = Music::HIDE; }
					else if (flag == "dev") { current.status = Music::DEV; }
					else current.status = Music::NORMAL;
				}
				else {
					current.status = Music::NORMAL;
				}
				musicList.push_back(current);
				step = 0;
				break;
			}
		}
		// 如果文件末尾漏了状态行，补上最后一个
		if (step == 3) {
			current.status = Music::NORMAL;
			musicList.push_back(current);
		}
		sort(musicList.begin(), musicList.end(), [](const Music& a, const Music& b) {
			return a.id < b.id;
			});
		if (debug == 1) {
			std::wstring wmsg = L"成功解析歌曲列表:\n";
			wmsg += L"原始数据大小: " + std::to_wstring(rawData.size()) + L" 字节\n";
			wmsg += L"解析出歌曲数量: " + std::to_wstring(musicList.size());
			MessageBoxW(NULL, wmsg.c_str(), L"调试信息", MB_OK);
		}
		buildMusicListUI();
		return true;
	}

	int GetSystemSampleRate() {
		int rate = 48000;
		// 使用 COINIT_MULTITHREADED 兼容性更好，或者确保与你程序的线程模型一致
		HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return rate;
		IMMDeviceEnumerator* pEnumerator = NULL;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
		if (SUCCEEDED(hr)) {
			IMMDevice* pDevice = NULL;
			hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
			if (SUCCEEDED(hr)) {
				IAudioClient* pAudioClient = NULL;
				hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
				if (SUCCEEDED(hr)) {
					WAVEFORMATEX* pwfx = NULL;
					hr = pAudioClient->GetMixFormat(&pwfx);
					if (SUCCEEDED(hr)) {
						rate = pwfx->nSamplesPerSec;
						CoTaskMemFree(pwfx);
					}
					pAudioClient->Release();
				}
				pDevice->Release();
			}
			pEnumerator->Release();
		}
		CoUninitialize(); // 记得收尾
		return rate;
	}

	string getExtFromUrl(const string& url) {
		auto pos = url.find_last_of('.');
		if (pos == string::npos) return "mp3";
		string ext = url.substr(pos + 1);
		auto q = ext.find_first_of("?#");
		if (q != string::npos) ext = ext.substr(0, q);
		transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		return ext;
	}

}

void CleanupMusicPlayer() {
	// 无论哪个引擎，先把网络清理掉（如果是公用的）
	curl_global_cleanup();

	if (MNG == LATEST) {
		// --- Miniaudio 退出逻辑 ---
		MA_Audio::g_threadExit = true;
		MA_Audio::g_isDecoderValid = false;

		// 回收线程
		if (MA_Audio::g_fetchThread.joinable())   MA_Audio::g_fetchThread.join();
		if (MA_Audio::g_refreshThread.joinable()) MA_Audio::g_refreshThread.join();
		if (MA_Audio::g_workThread.joinable())    MA_Audio::g_workThread.join();

		// 停止并销毁设备 (注意这里取地址的写法 &MA_Audio::g_device)
		if (MA_Audio::g_device.pUserData != nullptr) {
			ma_device_stop(&MA_Audio::g_device);
			ma_device_uninit(&MA_Audio::g_device);
			MA_Audio::g_device.pUserData = nullptr;
		}

		// 销毁解码器
		{
			std::lock_guard<std::mutex> lock(MA_Audio::g_audioMutex);
			if (MA_Audio::g_isDecoderInited) {
				ma_decoder_uninit(&MA_Audio::g_decoder);
				MA_Audio::g_isDecoderInited = false;
			}
		}
	}
	else {
		// --- SDL / Legacy 退出逻辑 ---
		if (Legacy_Audio::MusicMutex) {
			ReleaseMutex(Legacy_Audio::MusicMutex);
			CloseHandle(Legacy_Audio::MusicMutex);
			Legacy_Audio::MusicMutex = nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(Legacy_Audio::g_audioMutex);
			Mix_HaltMusic();
			if (Legacy_Audio::currentTrack) {
				Mix_FreeMusic((Mix_Music*)Legacy_Audio::currentTrack);
				Legacy_Audio::currentTrack = nullptr;
			}
		}

		// 清理临时文件（SDL版通常喜欢自己管文件）
		char tempPath[MAX_PATH]{};
		GetTempPathA(MAX_PATH, tempPath);
		string searchPath = string(tempPath) + "SQ_*.*";

		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				string fullPath = string(tempPath) + fd.cFileName;
				remove(fullPath.c_str());
			} while (FindNextFileA(hFind, &fd));
			FindClose(hFind);
		}
		Legacy_Audio::g_threadExit = true;
		Mix_CloseAudio();
		Mix_Quit();
		SDL_Quit();
	}
}