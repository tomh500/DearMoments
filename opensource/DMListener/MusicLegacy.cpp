

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
#include "MusicPlayer.h"
#include <windows.h>
#include <shlwapi.h>
#include "Head.h"
#include <curl/curl.h>
#include"Global.h"
#include <algorithm>  // for sort
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <commctrl.h>
#include <windowsx.h>
#include "resource.h"
#include <uxtheme.h> 
#include <richedit.h>
#include <commdlg.h>
#include <mmdeviceapi.h>
#include <AudioClient.h>
#include "Tools.h"
#include "MusicPlayer.h" 
#define ID_PAUSE_BUTTON 3401
#define ID_VOLUME_SLIDER 3105
#define ID_PROGRESS_SLIDER 3343
#pragma comment(lib, "Comctl32.lib")
using namespace std;

namespace Legacy_Audio {
	enum PlayMode {
		MODE_SINGLE,
		MODE_LOOP,
		MODE_SEQUENCE
	};

	PlayMode playMode = MODE_SINGLE;

	HWND hListBox = nullptr;
	HWND hSearchEdit = nullptr;
	HWND hProgressSlider = nullptr;
	HWND hDeviceCombo = nullptr;
	HWND hShowHiddenCheckbox = nullptr;
	HWND globalHwnd = nullptr;

	std::vector<Music> visibleMusicList;
	std::vector<Music> musicList;

	std::atomic<bool> g_threadExit{ false };
	std::atomic<bool> isPaused{ false };
	std::atomic<int>  currentVolume{ 64 };
	std::atomic<bool> isDownloading{ false };

	std::string currentTempFile;

	Mix_Music* currentTrack = nullptr;

	std::mutex g_audioMutex;
	HANDLE MusicMutex = nullptr;
	bool BypassProxy = false;




	// 补一个宽字符转 UTF8 的工具函数
	string WstringToString(const wstring& wstr) {
		if (wstr.empty()) return "";
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}
	vector<wstring> GetAudioDevices() {
		vector<wstring> devices;
		// 确保 SDL 已经 Init 过了
		int count = SDL_GetNumAudioDevices(0);
		for (int i = 0; i < count; ++i) {
			const char* name = SDL_GetAudioDeviceName(i, 0);
			if (name) {
				// 这里必须转成 wstring
				devices.push_back(Utf8ToWide(name));
			}
		}
		return devices;
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

	// 获取系统当前默认音频设备的采样率
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
	LRESULT CALLBACK ProgressSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK VolumeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

	using namespace std;
	void DebugMsg(const wstring& msg) {
		if (debug == 1) {
			MessageBoxW(NULL, msg.c_str(), L"调试信息", MB_OK | MB_ICONINFORMATION);
		}
	}




	size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
		string* data = static_cast<string*>(userp);
		data->append(static_cast<char*>(contents), size * nmemb);
		return size * nmemb;
	}
	HFONT CreatePreferredFont(int height = 16) {
		// 先尝试“等线”
		HFONT hFont = CreateFontW(
			height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"等线");
		// 如果创建失败，尝试“微软雅黑”
		if (!hFont) {
			hFont = CreateFontW(
				height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, VARIABLE_PITCH, L"微软雅黑");
		}
		// 如果还失败，使用系统默认 GUI 字体
		if (!hFont) {
			LOGFONT lf = { 0 };
			NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
			if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0)) {
				lf = ncm.lfMessageFont;
			}
			hFont = CreateFontIndirectW(&lf);
		}
		return hFont;
	}
	//字体函数
	void playTrackByIndex(int index) {
		if (index < 0 || index >= (int)visibleMusicList.size()) return;
		const auto& music = visibleMusicList[index];
		if (music.status == Music::DEV || music.status == Music::HIDE) return;
		// 开启异步线程播放，别卡死主界面
		thread([index, music]() {
			string ext = getExtFromUrl(music.url);
			string path = getTempFilePath(ext);

			isDownloading = true;

			if (downloadFile(music.url, path)) {

				if (currentTrack) {
					Mix_HaltMusic();
					Mix_FreeMusic(currentTrack);
					currentTrack = nullptr;
				}

				if (!currentTempFile.empty()) {
					remove(currentTempFile.c_str());
					currentTempFile.clear();
				}

				currentTempFile = path;

				{
					std::lock_guard<std::mutex> lock(g_audioMutex);

					if (currentTrack) {
						Mix_HaltMusic();
						Mix_FreeMusic(currentTrack);
						currentTrack = nullptr;
					}

					currentTrack = Mix_LoadMUS(path.c_str());
					if (currentTrack) {
						Mix_PlayMusic(currentTrack, 0);
					}
				}


			}

			isDownloading = false;
			}).detach();

		// UI 更新照旧
		ListView_SetItemState(hListBox, index, LVIS_SELECTED, LVIS_SELECTED);
		ListView_EnsureVisible(hListBox, index, FALSE);
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


	wstring utf8ToWide(const string& str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		wstring wstr(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
		wstr.resize(wcslen(wstr.c_str()));
		return wstr;
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

	LRESULT CALLBACK MusicPlayerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		static HWND hPlayButton, hPauseButton, hVolumeSlider;
		static vector<wstring> musicNames;
		switch (msg) {
		case WM_SIZE:
		{
			if (hListBox) {
				RECT rc; GetClientRect(hListBox, &rc);
				ListView_SetColumnWidth(hListBox, 0, rc.right - rc.left);
			}
			break;
		}
		case WM_APP + 2:
		{
			if (playMode == MODE_LOOP) {
				// 循环模式：不需要重下，直接重播
				Mix_PlayMusic(currentTrack, 0);
			}
			else if (playMode == MODE_SEQUENCE) {
				int curIndex = ListView_GetNextItem(hListBox, -1, LVNI_SELECTED);
				int nextIndex = curIndex + 1;
				if (nextIndex < (int)visibleMusicList.size()) {
					playTrackByIndex(nextIndex); // 自动下一首
				}
				else {
					// 如果到头了，可以回到第一首
					playTrackByIndex(0);
				}
			}
			// MODE_SINGLE 模式下什么都不做，播完就停了
			return 0;
		}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_CREATE:
		{
			// ===== SDL / Audio 初始化 =====
			SDL_setenv("SDL_AUDIODRIVER", "wasapi", 1);
			//SDL_setenv("SDL_AUDIO_ALLOW_ANY_CHANGE", "1", 1);
			SDL_setenv("SDL_AUDIO_ALLOW_ANY_CHANGE", "0", 1); // ❗禁止改变


			if (SDL_Init(SDL_INIT_AUDIO) < 0) {
				DebugMsg(L"SDL音频初始化失败");
			}

			int systemRate = GetSystemSampleRate();
			if (Mix_OpenAudio(systemRate, AUDIO_F32SYS, 2, 2048) < 0) {
				Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048);
			}
			if (debug == 1) {
				wchar_t buf[256];
				swprintf_s(buf, L"[Audio] OpenAudio rate=%d format=FLOAT32 channels=2", systemRate);
				MessageBoxW(NULL, buf, L"Audio Debug", MB_OK);
			}
			// --- 插入开始 (WM_CREATE after Mix_OpenAudio) ---
			{
				std::wstring msg = L"SDL_Init and Mix_OpenAudio status:\n";
				msg += L"SystemRate: " + std::to_wstring(systemRate) + L"\n";

				if (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) {
					msg += L"SDL audio initialized OK.\n";
				}
				else {
					msg += L"SDL audio NOT initialized.\n";
				}

				const char* mixErr = Mix_GetError();
				msg += L"Mixer error (last): ";
				msg += utf8ToWide(mixErr ? mixErr : "");
				DebugMsg(msg);
			}
			// --- 插入结束 ---

			Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC);
			// --- 插入开始 (after Mix_Init) ---
			{
				int inited = Mix_Init(0);
				// we already called Mix_Init earlier; but print the current flags:
				std::wstring m = L"Mixer init flags: ";
				m += std::to_wstring(inited);
				DebugMsg(m);
			}
			// --- 插入结束 ---

			Mix_HookMusicFinished([]() {
				if (globalHwnd)
					PostMessage(globalHwnd, WM_APP + 2, 0, 0);
				});

			curl_global_init(CURL_GLOBAL_DEFAULT);

			globalHwnd = hwnd;
			HFONT hFont = CreatePreferredFont(16);

			INITCOMMONCONTROLSEX icex{};
			icex.dwSize = sizeof(icex);
			icex.dwICC = ICC_WIN95_CLASSES;
			InitCommonControlsEx(&icex);

			// ===== 搜索框 =====
			hSearchEdit = CreateWindowW(
				WC_EDIT, L"",
				WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
				10, 10, 330, 25,
				hwnd, (HMENU)5001, nullptr, nullptr
			);
			Edit_SetCueBannerText(hSearchEdit, L"过滤器...");
			SendMessage(hSearchEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

			// ===== 音频设备选择 =====
			hDeviceCombo = CreateWindowW(
				WC_COMBOBOX, L"",
				WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
				350, 10, 120, 200,
				hwnd, (HMENU)6001, nullptr, nullptr
			);
			auto devices = GetAudioDevices();
			for (const auto& d : devices)
				SendMessage(hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)d.c_str());
			SendMessage(hDeviceCombo, CB_SETCURSEL, 0, 0);
			SendMessage(hDeviceCombo, WM_SETFONT, (WPARAM)hFont, TRUE);

			// ===== 歌曲列表（只创建一次）=====
			hListBox = CreateWindowW(
				WC_LISTVIEW, nullptr,
				WS_CHILD | WS_VISIBLE | WS_BORDER |
				LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
				10, 40, 460, 170,
				hwnd, (HMENU)2001, nullptr, nullptr
			);

			ListView_SetExtendedListViewStyle(
				hListBox,
				LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER
			);

			RECT rc;
			GetClientRect(hListBox, &rc);
			LVCOLUMN lvc{};
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = (LPWSTR)L"歌曲名";
			lvc.cx = rc.right - rc.left;
			ListView_InsertColumn(hListBox, 0, &lvc);

			SendMessage(hListBox, WM_SETFONT, (WPARAM)hFont, TRUE);

			// ===== 播放 / 暂停 =====
			hPlayButton = CreateWindowW(
				WC_BUTTON, L"播放",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				50, 220, 100, 30,
				hwnd, (HMENU)ID_PLAY_BUTTON, nullptr, nullptr
			);

			hPauseButton = CreateWindowW(
				WC_BUTTON, L"暂停",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				180, 220, 100, 30,
				hwnd, (HMENU)ID_PAUSE_BUTTON, nullptr, nullptr
			);

			SendMessage(hPlayButton, WM_SETFONT, (WPARAM)hFont, TRUE);
			SendMessage(hPauseButton, WM_SETFONT, (WPARAM)hFont, TRUE);

			// ===== 显示隐藏 =====
			hShowHiddenCheckbox = CreateWindowW(
				WC_BUTTON, L"显示CFG SONG",
				WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				350, 220, 120, 30,
				hwnd, (HMENU)4001, nullptr, nullptr
			);
			SendMessage(hShowHiddenCheckbox, WM_SETFONT, (WPARAM)hFont, TRUE);

			// ===== 音量 =====
			hVolumeSlider = CreateWindowW(
				TRACKBAR_CLASS, nullptr,
				WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
				10, 260, 460, 30,
				hwnd, (HMENU)ID_VOLUME_SLIDER, nullptr, nullptr
			);
			SendMessage(hVolumeSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 128));
			SendMessage(hVolumeSlider, TBM_SETPOS, TRUE, currentVolume);
			SendMessage(hVolumeSlider, WM_SETFONT, (WPARAM)hFont, TRUE);
			SetWindowSubclass(hVolumeSlider, VolumeSubclassProc, 1, (DWORD_PTR)hwnd);

			// ===== 进度条 =====
			hProgressSlider = CreateWindowW(
				TRACKBAR_CLASS, nullptr,
				WS_CHILD | WS_VISIBLE | TBS_NOTICKS,
				10, 300, 460, 20,
				hwnd, (HMENU)ID_PROGRESS_SLIDER, nullptr, nullptr
			);
			SetWindowSubclass(hProgressSlider, ProgressSubclassProc, 1, (DWORD_PTR)hwnd);

			// ===== 播放模式 =====
			CreateWindowW(L"button", L"单曲播放",
				WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
				10, 330, 100, 25, hwnd, (HMENU)3771, nullptr, nullptr);

			CreateWindowW(L"button", L"循环播放",
				WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
				120, 330, 100, 25, hwnd, (HMENU)3772, nullptr, nullptr);

			CreateWindowW(L"button", L"顺序播放",
				WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
				230, 330, 100, 25, hwnd, (HMENU)3773, nullptr, nullptr);

			SendMessage(GetDlgItem(hwnd, 3771), BM_SETCHECK, BST_CHECKED, 0);

			// ===== 拉取歌曲列表 =====
			thread([hwnd]() {
				const char* url =
					(regionCode != 1)
					? "https://raw.githubusercontent.com/tomh500/tomh500.github.io/refs/heads/main/Square/MusicList.txt"
					: "https://raw.githubusercontent.com/tomh500/tomh500.github.io/refs/heads/main/Square/MusicList_CN.txt";

				if (fetchMusicList(url))
					PostMessage(hwnd, WM_USER_UPDATE_LIST, 0, 0);
				}).detach();

			// ===== 进度刷新 =====
			thread([hwnd]() {
				while (!g_threadExit) {
					{
						std::lock_guard<std::mutex> lock(g_audioMutex);
						if (Mix_PlayingMusic() && currentTrack) {
							int pos = (int)Mix_GetMusicPosition(currentTrack);
							int total = (int)Mix_MusicDuration(currentTrack);
							if (total > 0)
								PostMessage(hwnd, WM_USER + 100, pos, total);
						}
					}

					Sleep(500);
				}
				}).detach();

			break;
		}

		case WM_COMMAND:
			// 监听搜索框输入变动
			if (LOWORD(wParam) == 5001 && HIWORD(wParam) == EN_CHANGE) {
				buildMusicListUI();
				return 0;
			}
			if (LOWORD(wParam) == 6001 && HIWORD(wParam) == CBN_SELCHANGE) {
				int idx = SendMessage(hDeviceCombo, CB_GETCURSEL, 0, 0);
				wchar_t deviceName[256];
				SendMessage(hDeviceCombo, CB_GETLBTEXT, idx, (LPARAM)deviceName);
				// 转换回 UTF8 给 SDL 用
				string utf8Name = WideToUtf8(deviceName);
				// 重新打开音频
				{
					std::lock_guard<std::mutex> lock(g_audioMutex);
					Mix_HaltMusic();
					Mix_CloseAudio();
					Mix_OpenAudioDevice(
						GetSystemSampleRate(),
						MIX_DEFAULT_FORMAT,
						2,
						2048,
						utf8Name.c_str(),
						SDL_AUDIO_ALLOW_ANY_CHANGE
					);
				}

			}
			switch (LOWORD(wParam)) {
			case 3771:
				playMode = MODE_SINGLE;
				break;
			case 3772:
				playMode = MODE_LOOP;
				break;
			case 3773:
				playMode = MODE_SEQUENCE;
				break;
			case ID_PLAY_BUTTON: {
				static int lastIndex = -1;
				int index = ListView_GetNextItem(hListBox, -1, LVNI_SELECTED);
				if (index == -1) break; // 没选中
				if (index == lastIndex && Mix_PlayingMusic()) break;
				lastIndex = index;
				playTrackByIndex(index);
				break;
			}
			case WM_USER_UPDATE_LIST: {
				buildMusicListUI();
				break;
			}
			case 4001: {
				if (HIWORD(wParam) == BN_CLICKED) {
					buildMusicListUI();
				}
				break;
			}
			case ID_PAUSE_BUTTON:
				if (isPaused) {
					Mix_ResumeMusic();
					SetWindowTextW(hPauseButton, L"暂停");
				}
				else {
					Mix_PauseMusic();
					SetWindowTextW(hPauseButton, L"继续");
				}
				isPaused = !isPaused;
				break;
			}
			break;
		case WM_HSCROLL:
		{
			if ((HWND)lParam == hVolumeSlider) {
				currentVolume = SendMessage(hVolumeSlider, TBM_GETPOS, 0, 0);
				Mix_VolumeMusic(currentVolume);
			}
			else if ((HWND)lParam == hProgressSlider) {
				if (LOWORD(wParam) == TB_ENDTRACK) {
					int seekPos = SendMessage(hProgressSlider, TBM_GETPOS, 0, 0);
					if (currentTrack && !isDownloading) {
						Mix_SetMusicPosition(static_cast<double>(seekPos));
					}
				}
			}
			break;
		}
		case WM_LBUTTONDOWN: {
			if ((HWND)lParam == hProgressSlider) {
				// 获取进度条的位置
				RECT progressRect;
				GetClientRect(hProgressSlider, &progressRect);
				int clickPos = GET_X_LPARAM(lParam);
				// 计算点击的位置对应的进度值
				int rangeStart = SendMessage(hProgressSlider, TBM_GETRANGEMIN, 0, 0);
				int rangeEnd = SendMessage(hProgressSlider, TBM_GETRANGEMAX, 0, 0);
				int newPos = rangeStart + (clickPos * (rangeEnd - rangeStart)) / (progressRect.right - progressRect.left);
				// 更新进度条和播放进度
				SendMessage(hProgressSlider, TBM_SETPOS, TRUE, newPos);
				if (currentTrack && !isDownloading) {
					Mix_SetMusicPosition(static_cast<double>(newPos));
				}
			}
			break;
		}
		case WM_APP + 1:
		{
			int newPos = (int)wParam;
			if (currentTrack && !isDownloading) {
				Mix_SetMusicPosition((double)newPos);
			}
			return 0;
		}
		case WM_USER + 100: {
			int pos = (int)wParam;
			int total = (int)lParam;
			if (total > 0)
				SendMessage(hProgressSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, total));
			SendMessage(hProgressSlider, TBM_SETPOS, TRUE, pos);
			break;
		}
		case WM_DESTROY:
			//	if(MusicMutex){
			//		ReleaseMutex(MusicMutex);
			//		CloseHandle(MusicMutex);
			//		MusicMutex = nullptr;
			//	}
		{
			std::lock_guard<std::mutex> lock(g_audioMutex);
			Mix_HaltMusic();
			if (currentTrack) {
				Mix_FreeMusic(currentTrack);
				currentTrack = nullptr;
			}
		}

		{
			char tempPath[MAX_PATH]{};
			GetTempPathA(MAX_PATH, tempPath);

			WIN32_FIND_DATAA fd;
			HANDLE hFind = FindFirstFileA((string(tempPath) + "SQ_*.*").c_str(), &fd);

			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					remove((string(tempPath) + fd.cFileName).c_str());
				} while (FindNextFileA(hFind, &fd));
				FindClose(hFind);
			}
		}

		g_threadExit = true;
		Mix_CloseAudio();
		Mix_Quit();
		SDL_Quit();
		curl_global_cleanup();

		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}


	LRESULT CALLBACK ProgressSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
		{
			RECT rc; GetClientRect(hWnd, &rc);
			int rangeMin = (int)SendMessage(hWnd, TBM_GETRANGEMIN, 0, 0);
			int rangeMax = (int)SendMessage(hWnd, TBM_GETRANGEMAX, 0, 0);
			int x = GET_X_LPARAM(lParam);
			if (x < 0) x = 0;
			if (x > rc.right) x = rc.right;
			int newPos = rangeMin + (int)((rangeMax - rangeMin) * (double)x / rc.right);
			SendMessage(hWnd, TBM_SETPOS, TRUE, newPos);
			HWND hParent = (HWND)dwRefData;
			PostMessage(hParent, WM_APP + 1, newPos, 0);
			SetCapture(hWnd);
			return 0;
		}
		case WM_LBUTTONUP:
			ReleaseCapture();
			return 0;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hWnd, ProgressSubclassProc, 1);
			break;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
	LRESULT CALLBACK VolumeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
		{
			RECT rc; GetClientRect(hWnd, &rc);
			int rangeMin = (int)SendMessage(hWnd, TBM_GETRANGEMIN, 0, 0);
			int rangeMax = (int)SendMessage(hWnd, TBM_GETRANGEMAX, 0, 0);
			int x = GET_X_LPARAM(lParam);
			if (x < 0) x = 0;
			if (x > rc.right) x = rc.right;
			int newPos = rangeMin + (int)((rangeMax - rangeMin) * (double)x / rc.right);
			SendMessage(hWnd, TBM_SETPOS, TRUE, newPos);
			int sdlVolume = (int)(newPos * MIX_MAX_VOLUME / 100);
			Mix_VolumeMusic(sdlVolume);
			SetCapture(hWnd);
			return 0;
		}
		case WM_LBUTTONUP:
			ReleaseCapture();
			return 0;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hWnd, VolumeSubclassProc, 1);
			break;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
}