
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

bool BypassProxy = false;
#include "MusicHead.h"
#include "Tools.h"
#include <windowsx.h>

#define ID_IMPORT_FOLDER 7001

	// 第三方库
#include <curl/curl.h>
#include <mmdeviceapi.h>
#include <AudioClient.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Msimg32.lib")  // 顺便把这个也加上，UI经常用到
#include <commctrl.h>


// --- 1. 基础配置与开关 ---
	
	PlayMode playMode = MODE_SINGLE;
	HANDLE MusicMutex = nullptr;
	atomic<bool> g_threadExit(false);
	bool isBusy = false;
	// --- 2. 音频核心组件 ---
	ma_device  g_device;
	ma_decoder g_decoder;
	ma_context g_context;
	bool g_contextInited = false;
	mutex g_audioMutex;

	// 原子状态标志
	atomic<bool> g_isDecoderValid{ false };
	atomic<bool> g_isDecoderInited{ false };
	atomic<bool> isPaused{ false };
	atomic<bool> isDownloading{ false };
	atomic<int> currentVolume{ 64 };   // 默认音量
	int g_SampleRate = 0;

	// --- 3. UI 句柄与数据容器 ---
	HWND globalHwnd = nullptr;
	HWND hListBox = nullptr;
	HWND hSearchEdit = nullptr;
	HWND hDeviceCombo = nullptr;
	HWND hProgressSlider = nullptr;
	HWND hShowHiddenCheckbox = nullptr;
	HWND hSampleRateLabel = nullptr;
	HWND hApplyBtn = nullptr;
	HWND hHotKeyEdit = nullptr;
	HWND hSampleRateEdit = nullptr;
	HWND hSharedCheck = nullptr;
	std::thread g_fetchThread;
	std::thread g_refreshThread;
	std::thread g_workThread;
	vector<Music> musicList;
	vector<Music> visibleMusicList;
	string currentTempFile = "";

	// --- 4. 辅助函数实现 ---

	// 调试弹窗，实话实说，这玩意儿多了挺烦，但调代码必不可少
	void DebugMsg(const wstring& msg) {
		if (debug == 1) {
			MessageBoxW(NULL, msg.c_str(), L"Debug Info", MB_OK | MB_ICONINFORMATION);
		}
	}

	// 宽字符转换逻辑
	std::wstring utf8ToWide1(const std::string& s) {
		if (s.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		if (len <= 0) return L"";
		wstring wstr(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wstr[0], len);
		wstr.resize(wcslen(wstr.c_str()));
		return wstr;
	}

	// CURL 下载回调：把数据追加到 string 里
	size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
		size_t totalSize = size * nmemb;
		string* data = static_cast<string*>(userp);
		data->append(static_cast<char*>(contents), totalSize);
		return totalSize;
	}


	void SafeCleanupAudio() {
		// 1. 关掉逻辑开关，让 data_callback 彻底静默
		g_isDecoderValid.store(false);

		// 2. 停止并销毁设备
		if (g_device.pUserData != nullptr) {
			ma_device_stop(&g_device);

			// 【关键】给系统音频服务一点“下班”时间，防止 AudioSes.dll 反应不过来
			std::this_thread::sleep_for(std::chrono::milliseconds(20));

			ma_device_uninit(&g_device);

			// 【防重复清理】清空结构体，防止 ma_IAudioCaptureClient_Release 再次被触发
			g_device.pUserData = nullptr;
			memset(&g_device, 0, sizeof(ma_device));
		}

		// 3. 彻底结束工作线程（防止切歌时下载线程撞车）
		if (g_workThread.joinable()) {
			g_workThread.join();
		}

		// 4. 清理解码器
		std::lock_guard<std::mutex> lock(g_audioMutex);
		if (g_isDecoderInited) {
			ma_decoder_uninit(&g_decoder);
			g_isDecoderInited = false;
		}
		UpdateDeviceComboState(false);
	}


	// 补一个宽字符转 UTF8 的工具函数
	string WstringToString(const wstring& wstr) {
		if (wstr.empty()) return "";
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	//wstring utf8ToWide1(const string& str);

	LRESULT CALLBACK ProgressSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK VolumeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

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


	MP_WNDPROC MusicPlayerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
				std::lock_guard<std::mutex> lock(g_audioMutex);
				ma_decoder_seek_to_pcm_frame(&g_decoder, 0);
				g_isDecoderValid = true;
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

			// 1. 强行先初始化全局 Context（如果没初始化的话）
			if (!g_contextInited) {
				if (ma_context_init(NULL, 0, NULL, &g_context) == MA_SUCCESS) {
					g_contextInited = true;
				}
			}
			// 1. 基本配置
			int systemRate = GetSystemSampleRate();
			ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback); // 强行指定 playback
			deviceConfig.playback.format = ma_format_f32;
			deviceConfig.playback.channels = 2;
			deviceConfig.sampleRate = g_SampleRate; // 【建议】设为0，让硬件自适应，能极大减少初始化失败率
			deviceConfig.dataCallback = data_callback;
			deviceConfig.pUserData = hwnd;
			deviceConfig.playback.shareMode = ma_share_mode_exclusive;
			deviceConfig.wasapi.noAutoStreamRouting = MA_TRUE;
			// 2. 核心修改：设置独占模式
			// 注意看，路径是 playback.shareMode，而不是 wasapi.shareMode
			deviceConfig.playback.shareMode = ma_share_mode_exclusive;

			// 3. 尝试初始化
			ma_result res = ma_device_init(&g_context, &deviceConfig, &g_device);

			// 4. 如果独占模式失败（res != MA_SUCCESS），回退到共享模式
			if (res != MA_SUCCESS) {
				if (debug == 1) OutputDebugStringW(L"[miniaudio] 独占模式失败，回退到共享模式...\n");

				deviceConfig.playback.shareMode = ma_share_mode_shared;
				res = ma_device_init(&g_context, &deviceConfig, &g_device);
			}

			// 5. 启动设备
			if (res == MA_SUCCESS) {
				ma_device_start(&g_device);
				if (debug == 1) {
					wchar_t buf[256];
					// 验证最终生效的模式
					const char* finalMode = (g_device.playback.shareMode == ma_share_mode_exclusive) ? "Exclusive" : "Shared";
					swprintf_s(buf, L"[miniaudio] 启动成功 | 模式: %hs | 采样率: %d", finalMode, g_device.sampleRate);
					MessageBoxW(NULL, buf, L"Audio Debug", MB_OK);
				}
				g_contextInited = true;
			}

			else {
				DebugMsg(L"miniaudio 彻底初始化失败！");
			}

			// 这里的 Mix_HookMusicFinished 逻辑已经整合到 data_callback 的末尾检测中了
			// 所以不需要专门的 Hook 函数

			curl_global_init(CURL_GLOBAL_DEFAULT);

			globalHwnd = hwnd;
			HFONT hFont = CreatePreferredFont(16);

			INITCOMMONCONTROLSEX icex{};
			icex.dwSize = sizeof(icex);
			icex.dwICC = ICC_WIN95_CLASSES;
			InitCommonControlsEx(&icex);

			HWND hImportButton = CreateWindowW(
				WC_BUTTON, L"导入文件夹",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				350, 330, 100, 25,
				hwnd, (HMENU)ID_IMPORT_FOLDER, nullptr, nullptr
			);
			SendMessage(hImportButton, WM_SETFONT, (WPARAM)hFont, TRUE);


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

			// ===== 采样率设置 (ComboBox 右侧) =====
			// 采样率输入框 (ID: 6002)
			hSampleRateEdit = CreateWindowW(
				L"EDIT", L"48000", 
				WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, // 只允许数字
				510, 10, 60, 24, // x 坐标接在 ComboBox (350+120+10) 后面
				hwnd, (HMENU)6002, nullptr, nullptr
			);

			// 提交按钮 (ID: 6003)
			hApplyBtn = CreateWindowW(
				L"BUTTON", L"更改采样",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				580, 10, 100, 24,
				hwnd, (HMENU)6003, nullptr, nullptr
			);


			// 设置字体
			SendMessage(hSampleRateEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
			SendMessage(hApplyBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
			SendMessage(hHotKeyEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

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
				// 启动前先 join 之前的（防止重复赋值触发 abort）
			if (g_fetchThread.joinable()) g_fetchThread.join();
			g_fetchThread = std::thread([hwnd]() {
				const char* url = (regionCode != 1)
					? ""
					: "";

				if (fetchMusicList(url))
					PostMessage(hwnd, WM_USER_UPDATE_LIST, 0, 0);
				});

			// ===== 进度刷新线程 =====
			if (g_refreshThread.joinable()) g_refreshThread.join();
			g_refreshThread = std::thread([hwnd]() {
				while (!g_threadExit) {
					if (g_isDecoderValid && g_isDecoderInited && !isPaused) {
						std::lock_guard<std::mutex> lock(g_audioMutex);
						ma_uint64 cursor, length;
						if (ma_decoder_get_cursor_in_pcm_frames(&g_decoder, &cursor) == MA_SUCCESS &&
							ma_decoder_get_length_in_pcm_frames(&g_decoder, &length) == MA_SUCCESS) {

							ma_uint32 sr = g_device.sampleRate > 0 ? g_device.sampleRate : 48000;
							int pos = (int)(cursor / sr);
							int total = (int)(length / sr);
							PostMessage(hwnd, WM_USER + 100, pos, total);
						}
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
				}
				});


		}
		case WM_COMMAND:
		{
			WORD wmId = LOWORD(wParam);
			WORD wmEvent = HIWORD(wParam);




			if (wmId == 6001 && wmEvent == CBN_SELCHANGE) {
				// 如果你没在 UI 层禁用，代码层也要拦截
				if (g_isDecoderValid && !isPaused) {
					MessageBoxW(hwnd, L"请先暂停播放再切换设备", L"提示", MB_OK);
					return 0;
				}

				int idx = (int)SendMessage(hDeviceCombo, CB_GETCURSEL, 0, 0);

				// 1. 暴力清理
				SafeCleanupAudio();
				currentTempFile = ""; // 清空当前歌曲路径

				// 重置 UI 进度条（如果有的话）
				SendMessage(hProgressSlider, TBM_SETPOS, TRUE, 0);

				// 2. 配置并初始化新设备
				ma_device_config config = ma_device_config_init(ma_device_type_playback);
				if (idx > 0 && idx < (int)g_realDeviceList.size()) {
					config.playback.pDeviceID = &g_realDeviceList[idx].id;
				}

				config.playback.format = ma_format_f32;
				config.playback.channels = 2;
				config.sampleRate = g_SampleRate; // 自适应
				config.dataCallback = data_callback;
				config.pUserData = hwnd;
				config.playback.shareMode = ma_share_mode_exclusive;
				config.wasapi.noAutoStreamRouting = MA_TRUE; // 减少系统干扰

				// 3. 尝试初始化
				ma_result res = ma_device_init(&g_context, &config, &g_device);
				if (res != MA_SUCCESS) {
					config.playback.shareMode = ma_share_mode_shared;
					res = ma_device_init(&g_context, &config, &g_device);
				}

				if (res == MA_SUCCESS) {
					ma_device_start(&g_device);
					DebugMsg(L"设备已重置，请重新选择歌曲播放");
				}
				return 0;
			}


			// 监听搜索框
			if (wmId == 5001 && wmEvent == EN_CHANGE) {
				buildMusicListUI();
				return 0;
			}

			// 原有的按钮逻辑
			switch (wmId) {
			case ID_IMPORT_FOLDER: {
				std::wstring folder;
				if (SelectFolder(folder)) {
					ImportLocalMusicFolder(folder);
				}
				break;
			}
			case 6003: {

				wchar_t buf[16];
				GetWindowTextW(hSampleRateEdit, buf, 16);
				int newRate = _wtoi(buf);
				if (newRate >= 8000 && newRate <= 384000) {
					g_SampleRate = newRate;
					int idx = (int)SendMessage(hDeviceCombo, CB_GETCURSEL, 0, 0);
					SafeCleanupAudio();
					currentTempFile = ""; // 清空当前歌曲路径

					// 重置 UI 进度条（如果有的话）
					SendMessage(hProgressSlider, TBM_SETPOS, TRUE, 0);

					// 2. 配置并初始化新设备
					ma_device_config config = ma_device_config_init(ma_device_type_playback);
					if (idx > 0 && idx < (int)g_realDeviceList.size()) {
						config.playback.pDeviceID = &g_realDeviceList[idx].id;
					}

					config.playback.format = ma_format_f32;
					config.playback.channels = 2;
					config.sampleRate = g_SampleRate; // 自适应
					config.dataCallback = data_callback;
					config.pUserData = hwnd;
					config.playback.shareMode = ma_share_mode_exclusive;
					config.wasapi.noAutoStreamRouting = MA_TRUE; // 减少系统干扰

					// 3. 尝试初始化
					ma_result res = ma_device_init(&g_context, &config, &g_device);
				}
					 break;
			}
			case 3771: playMode = MODE_SINGLE; break;
			case 3772: playMode = MODE_LOOP; break;
			case 3773: playMode = MODE_SEQUENCE; break;
			case ID_PLAY_BUTTON: {
				int index = ListView_GetNextItem(hListBox, -1, LVNI_SELECTED);
				if (index != -1) {
					UpdateDeviceComboState(isBusy); // 立即检查并锁定
					playTrackByIndex(index);
				}
				break;
			}
			case 4001: buildMusicListUI(); break;
			case ID_PAUSE_BUTTON:
				if (isPaused) {
					ma_device_start(&g_device);
					SetWindowTextW(hPauseButton, L"暂停");
					isBusy = true;
				}
				else {
					ma_device_stop(&g_device);
					SetWindowTextW(hPauseButton, L"继续");
					isBusy = false;
				}
				isPaused = !isPaused;
				UpdateDeviceComboState(isBusy);
				break;
			}
			break;
		}
		case WM_HSCROLL:
		{
			if ((HWND)lParam == hVolumeSlider) {
				currentVolume = SendMessage(hVolumeSlider, TBM_GETPOS, 0, 0);
				ma_device_set_master_volume(&g_device, (float)currentVolume / 128.0f);
			}
			else if ((HWND)lParam == hProgressSlider) {
				if (LOWORD(wParam) == TB_ENDTRACK) {
					int seekPos = SendMessage(hProgressSlider, TBM_GETPOS, 0, 0);
					if (g_isDecoderValid && !isDownloading) {
						std::lock_guard<std::mutex> lock(g_audioMutex);
						ma_decoder_seek_to_pcm_frame(&g_decoder, (ma_uint64)seekPos * g_device.sampleRate);
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
				int clickPos = GET_X_LPARAM(lParam);  // 获取点击位置的x坐标
				// 计算点击的位置对应的进度值
				int rangeStart = SendMessage(hProgressSlider, TBM_GETRANGEMIN, 0, 0);
				int rangeEnd = SendMessage(hProgressSlider, TBM_GETRANGEMAX, 0, 0);
				int newPos = rangeStart + (clickPos * (rangeEnd - rangeStart)) / (progressRect.right - progressRect.left);
				// 更新进度条和播放进度
				SendMessage(hProgressSlider, TBM_SETPOS, TRUE, newPos);
				//if (currentTrack && !isDownloading) {
					//Mix_SetMusicPosition(static_cast<double>(newPos));
				//}
				if (g_isDecoderValid && !isDownloading) {
					std::lock_guard<std::mutex> lock(g_audioMutex);
					ma_decoder_seek_to_pcm_frame(&g_decoder, (ma_uint64)newPos * g_device.sampleRate);
				}
			}
			break;
		}

		case WM_APP + 1:
		{
			int newPosSecond = (int)wParam; // 假设传入的是秒
			if (g_isDecoderValid && !isDownloading) {
				std::lock_guard<std::mutex> lock(g_audioMutex);
				// 跳转到：秒 * 采样率 = 目标帧
				ma_decoder_seek_to_pcm_frame(&g_decoder, (ma_uint64)newPosSecond * g_device.sampleRate);
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
		{
			// 1. 设置退出标志，让 refreshThread 的 while 循环能停下来
			g_threadExit = true;

			// 2. 停止音频逻辑开关，防止回调函数再访问 decoder
			g_isDecoderValid = false;

			// 3. 【核心修复】安全回收所有线程
			// 如果 fetchThread 还在下载，这里会阻塞一会儿直到它结束，这是安全的做法
			if (g_fetchThread.joinable())   g_fetchThread.join();
			if (g_refreshThread.joinable()) g_refreshThread.join();

			// 如果 playTrackByIndex 开启的下载线程还在跑，也必须接回来
			if (g_workThread.joinable())    g_workThread.join();

			// 4. 停止并释放硬件设备
			// 此时回调函数 data_callback 已经确定不会被并发调用了
			if (g_device.pUserData != nullptr) {
				ma_device_stop(&g_device);
				ma_device_uninit(&g_device);
				g_device.pUserData = nullptr;
			}

			// 5. 清理解码器
			{
				std::lock_guard<std::mutex> lock(g_audioMutex);
				if (g_isDecoderInited) {
					ma_decoder_uninit(&g_decoder);
					g_isDecoderInited = false;
				}
			}

			// 强制清理：最后的兜底保护
		// 无论是否播放，无论指针状态，强制归还音频设备所有权
			if (g_device.type != ma_device_type_loopback) {
				// 1. 强制停止硬件流（解开独占模式的关键）
				ma_device_stop(&g_device);

				// 2. 彻底销毁设备实例，释放 WASAPI/DirectSound 句柄
				ma_device_uninit(&g_device);

				// 3. 标记设备类型为 loopback 仅作为重置标识，防止二次重复释放
				g_device.type = ma_device_type_loopback;
			}

			// 解除任何可能挂起的解码器资源
			{
				std::lock_guard<std::mutex> lock(g_audioMutex);
				if (g_isDecoderInited) {
					ma_decoder_uninit(&g_decoder);
					g_isDecoderInited = false;
				}
			}

			// 强制结束进程
			// 如果有线程卡死（比如下载死锁），常规退出会失效
			// PostQuitMessage 只是温柔提醒，这里直接让系统回收资源
			curl_global_cleanup();
			//TerminateProcess(GetCurrentProcess(), 0);
			//PostQuitMessage(0);
			break;
		}
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	LRESULT CALLBACK ProgressSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		HWND hMainWnd = (HWND)dwRefData;
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

			// 计算新位置
			int newPos = rangeMin + (int)((rangeMax - rangeMin) * (double)x / rc.right);
			SendMessage(hWnd, TBM_SETPOS, TRUE, newPos);

			// 通知主窗口跳转进度 (WM_APP + 1)
			PostMessage(hMainWnd, WM_APP + 1, (WPARAM)newPos, 0);

			SetCapture(hWnd);
			return 0;
		}
		case WM_LBUTTONUP:
			ReleaseCapture();
			break;
		case WM_MOUSEMOVE:
			if (wParam & MK_LBUTTON) {
				// 拖动时也可以实时更新
				RECT rc; GetClientRect(hWnd, &rc);
				int rangeMin = (int)SendMessage(hWnd, TBM_GETRANGEMIN, 0, 0);
				int rangeMax = (int)SendMessage(hWnd, TBM_GETRANGEMAX, 0, 0);
				int x = GET_X_LPARAM(lParam);
				if (x < 0) x = 0; if (x > rc.right) x = rc.right;
				int newPos = rangeMin + (int)((rangeMax - rangeMin) * (double)x / rc.right);
				SendMessage(hWnd, TBM_SETPOS, TRUE, newPos);
				// 注意：拖动时频繁 Seek 可能会卡，这里通常只更新 UI
			}
			break;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
	LRESULT CALLBACK VolumeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{


		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
		case WM_MOUSEMOVE:
			if (uMsg == WM_LBUTTONDOWN || (wParam & MK_LBUTTON)) {
				RECT rc; GetClientRect(hWnd, &rc);
				int rangeMin = (int)SendMessage(hWnd, TBM_GETRANGEMIN, 0, 0);
				int rangeMax = (int)SendMessage(hWnd, TBM_GETRANGEMAX, 0, 0);
				int x = GET_X_LPARAM(lParam);
				if (x < 0) x = 0; if (x > rc.right) x = rc.right;

				int newPos = rangeMin + (int)((rangeMax - rangeMin) * (double)x / rc.right);
				SendMessage(hWnd, TBM_SETPOS, TRUE, newPos);

				// 转换成 0.0 ~ 1.0 的音量
				float vol = (float)newPos / (float)rangeMax;
				ma_device_set_master_volume(&g_device, vol);

				if (uMsg == WM_LBUTTONDOWN) SetCapture(hWnd);
				return 0;
			}
			break;
		case WM_LBUTTONUP:
			ReleaseCapture();
			break;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
