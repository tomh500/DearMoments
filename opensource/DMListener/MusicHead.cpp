
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

#include <mmdeviceapi.h>
#include <AudioClient.h>

#pragma pack(push, 1)



#pragma comment(lib, "Ole32.lib") // GetSystemSampleRate 需要

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

			g_realDeviceList.push_back({ pPlaybackInfos[i].id, utf8ToWide1(pPlaybackInfos[i].name) });
			names.push_back(utf8ToWide1(pPlaybackInfos[i].name));
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
	if (index < 0 || index >= (int)visibleMusicList.size() || isDownloading)
		return;

	if (g_workThread.joinable())
		g_workThread.join();

	const Music music = visibleMusicList[index];

	g_workThread = std::thread([music]() {
		isDownloading = true;
		UpdateDeviceComboState(true);

		std::string path;
		// ===== 1. 获取播放文件路径 =====
		if (!music.isLocal) {
			std::string ext = getExtFromUrl(music.url);
			path = getTempFilePath(ext);
			if (!downloadFile(music.url, path)) {
				isDownloading = false;
				UpdateDeviceComboState(false);
				return;
			}
		}
		else {
			path = music.url; // 本地路径已经是 UTF-8 存着的
		}

		// ===== 2. 初始化 decoder =====
		{
			std::lock_guard<std::mutex> lock(g_audioMutex);

			// 停止设备，防止回调函数在初始化时访问 decoder
			if (ma_device_is_started(&g_device)) {
				ma_device_stop(&g_device);
			}

			g_isDecoderValid = false;
			if (g_isDecoderInited) {
				ma_decoder_uninit(&g_decoder);
				g_isDecoderInited = false;
			}

			ma_format targetFormat = g_device.playback.format;
			ma_uint32 targetSR = (g_device.sampleRate == 0) ? 48000 : g_device.sampleRate;

			ma_decoder_config cfg = ma_decoder_config_init(targetFormat, 2, targetSR);

			ma_result res;
			if (music.isLocal) {
				// 【核心修复】本地文件直接用宽字符接口，绕过所有 UTF-8 转换烦恼
				// 假设你的 music.wname 存的是完整路径，或者把 path 转回 wstring
				std::wstring wPath = utf8ToWide1(path);
				res = ma_decoder_init_file_w(wPath.c_str(), &cfg, &g_decoder);
			}
			else {
				res = ma_decoder_init_file(path.c_str(), &cfg, &g_decoder);
			}

			if (res == MA_SUCCESS) {
				if (!music.isLocal && !currentTempFile.empty()) {
					remove(currentTempFile.c_str());
				}
				if (!music.isLocal) currentTempFile = path;

				g_isDecoderInited = true;
				g_isDecoderValid = true;

				ma_device_start(&g_device);
				isPaused = false;
			}
			else {
				// 打印个错误，不然真不知道为什么没反应
				wchar_t errBuf[256];
				swprintf_s(errBuf, L"解码失败! 错误码: %d\n路径: %ls", res, utf8ToWide1(path).c_str());
				MessageBoxW(NULL, errBuf, L"播放错误", MB_OK | MB_ICONERROR);
			}
		}

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
				wmsg += utf8ToWide1(selectedProxy);
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
		wstring wname = utf8ToWide1(music.name);
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
		wmsg += utf8ToWide1(url);
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


string getExtFromUrl(const string& url) {
	auto pos = url.find_last_of('.');
	if (pos == string::npos) return "mp3";
	string ext = url.substr(pos + 1);
	auto q = ext.find_first_of("?#");
	if (q != string::npos) ext = ext.substr(0, q);
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return ext;
}

bool SelectFolder(std::wstring& outPath) {
	IFileDialog* pDialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog))))
		return false;

	DWORD options;
	pDialog->GetOptions(&options);
	pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

	if (FAILED(pDialog->Show(nullptr))) {
		pDialog->Release();
		return false;
	}

	IShellItem* pItem = nullptr;
	if (SUCCEEDED(pDialog->GetResult(&pItem))) {
		PWSTR path = nullptr;
		if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
			outPath = path;
			CoTaskMemFree(path);
		}
		pItem->Release();
	}

	pDialog->Release();
	return !outPath.empty();
}


bool IsAudioFile(const fs::path& p) {
	auto ext = p.extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	return ext == L".wav" || ext == L".mp3" || ext == L".flac";
}
// 在 ImportLocalMusicFolder 里确保用这个
void ImportLocalMusicFolder(const std::wstring& folder) {
	int baseId = (int)musicList.size();
	for (auto& entry : fs::recursive_directory_iterator(folder)) {
		if (!entry.is_regular_file()) continue;
		if (!IsAudioFile(entry.path())) continue;

		Music m;
		m.id = baseId++;
		m.isLocal = true;
		// 建议：url 存 UTF-8 没问题，但要保证 WstringToString 是正确的 UTF-8 转换
		m.url = WString2String(entry.path().wstring());
		m.wname = entry.path().stem().wstring();
		m.name = WString2String(m.wname);

		musicList.push_back(m);
	}
	visibleMusicList = musicList;
	buildMusicListUI();
}



int GetSystemSampleRate() {
	int rate = 48000;
	// 1. 初始化 COM
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	// 如果初始化失败且不是因为模式改变，直接返回
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return rate;

	IMMDeviceEnumerator* pEnumerator = NULL;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
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
					CoTaskMemFree(pwfx); // 释放系统分配的内存
				}
				pAudioClient->Release();
			}
			pDevice->Release();
		}
		pEnumerator->Release();
	}

	// 只要 CoInitializeEx 没彻底失败，最后都要 Uninitialize
	CoUninitialize();
	return rate;
}