#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <deque>
#include <mutex>
#include <io.h>
#include <fcntl.h>


#include "QuickStop.h"
#include "Global.h"
#include "GSIClient.h"
#include "ProcessManager.h"
#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI ConsoleHandler(DWORD signal);
#endif

using namespace std;
namespace fs = std::filesystem;

#pragma pack(push, 1)
struct SharedInfo {
	char version[32];
	char module_name[32];
	uint8_t is_active;
};
#pragma pack(pop)

HANDLE hMapGSI = NULL;
SharedInfo* pGSI = nullptr;
int input_mode = 0; //输入模式，0代表抖动，1代表mousewheel

std::atomic<bool> g_console_visible{ true };
bool show_gsi_raw = true; // Home 键控制这个：是否在控制台打印 JSON
bool g_pausejiting_ctrl = false;
std::mutex g_output_mutex;
std::deque<std::wstring> g_output_buffer;
constexpr size_t MAX_OUTPUT_LINES = 2000;
//std::vector<int> silent_keys = { VK_SHIFT, VK_CONTROL }; // Shift 和 Ctrl 默认不急停
//
const int DOU_DONG_RANGE = 1; // 抖动幅度
const int HZ = 128;
const vector<wstring> GAME_NAMES = { L"Counter-Strike 2", L"反恐精英：全球攻势" };
//全局变量名
bool kai_guan = true;
bool jing_yin_mo_shi = false; // Home 控制这个
bool chuang_kou_focus = false;
wstring log_lu_jing = L"";
bool g_safe_mode = false; // 默认为 false，意味着默认进入“强制抖动”模式
bool g_manual_pause = false;//抖动的开关，受读log控制 

bool TryAttachGSI()
{
	hMapGSI = OpenFileMappingA(FILE_MAP_READ, FALSE, "KICore_GSI_Ver");
	if (!hMapGSI)
		return false;

	pGSI = (SharedInfo*)MapViewOfFile(
		hMapGSI,
		FILE_MAP_READ,
		0, 0,
		sizeof(SharedInfo)
	);

	if (!pGSI) {
		CloseHandle(hMapGSI);
		hMapGSI = NULL;
		return false;
	}

	return true;
}


// 检查是否按下了任意静默键
bool IsSilentKeyPressed() {
	for (int vk : KICore::silent_keys) {
		if (GetAsyncKeyState(vk) & 0x8000) return true;
	}
	return false;
}

// 颜色控制
void GeiYanSe(WORD color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void ResetColor() {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

// 转换函数
std::wstring ToWString(const std::string& s) {
	if (s.empty()) return L"";
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	std::wstring result(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], len);
	return result;
}


// GSI 回调
void OnGSIDataReceived(const std::string& json) {
	// 调试：看看数据包到底有没有进来
	if (show_gsi_raw) {
		std::lock_guard<std::mutex> lock(g_output_mutex);
		std::wcout << L"[DEBUG] GSI包到达, 长度: " << json.length() << std::endl;
	}

	KICore::ProcessQuickStopCommand(json);

	if (show_gsi_raw) {
		std::lock_guard<std::mutex> lock(g_output_mutex);
		GeiYanSe(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		std::wcout << L"\n[GSI_RAW] " << ToWString(json) << std::endl;
		ResetColor();
	}
}

void XiangYiXia(bool active) {
	if (active) Beep(1200, 150); else Beep(600, 150);
}

void PushOutput(const std::wstring& line, WORD color)
{
	{
		std::lock_guard<std::mutex> lock(g_output_mutex);
		g_output_buffer.push_back(line);
		if (g_output_buffer.size() > MAX_OUTPUT_LINES)
			g_output_buffer.pop_front();
	}

	if (g_console_visible.load())
	{
		GeiYanSe(color);
		std::wcout << line << std::endl;
	}
}




// 找日志路径
wstring ZhaoLogLuJing() {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return L"";
	wchar_t buffer[MAX_PATH];
	DWORD bSize = sizeof(buffer);
	if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, (LPBYTE)buffer, &bSize) != ERROR_SUCCESS) {
		RegCloseKey(hKey); return L"";
	}
	RegCloseKey(hKey);
	wstring vdfPath = wstring(buffer) + L"\\steamapps\\libraryfolders.vdf";
	if (!fs::exists(vdfPath)) return L"";
	ifstream file(vdfPath);
	string line, currentPath;
	while (getline(file, line)) {
		smatch match;
		if (regex_search(line, match, regex("\"path\"\\s*\"([^\"]+)\""))) currentPath = match[1].str();
		if (line.find("\"730\"") != string::npos && !currentPath.empty()) {
			wstring ws(currentPath.begin(), currentPath.end());
			return ws + L"\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\console.log";
		}
	}
	return L"";
}

// --- 模块: 盯着日志看 ---
// --- 新增：专门处理日志指令的分发器 ---
void DispatchLogCommand(const string& clean_txt) {
	// 1. 处理原本的 quick 指令 (急停逻辑)
	if (clean_txt.find("quick") != string::npos) {
		KICore::ProcessQuickStopCommand(clean_txt);
	}

	// 2. 新增：核心模拟输入开关控制
	if (clean_txt.find("KICoreStop") != string::npos) {
		g_manual_pause = true;
		GeiYanSe(FOREGROUND_RED | FOREGROUND_INTENSITY);
		wcout << L"[CMD] 接收到停止指令 -> 模拟输入已挂起" << endl;
		ResetColor();
	}
	else if (clean_txt.find("KICoreRestart") != string::npos) {
		g_manual_pause = false;
		GeiYanSe(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		wcout << L"[CMD] 接收到恢复指令 -> 模拟输入已就绪" << endl;
		ResetColor();
	}
}

// --- 重构后的日志监听主循环 ---
void LogJianTing() {
	std::uintmax_t last_size = 0;
	static regex prefix_reg(R"(^\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}\s+)");

	if (!log_lu_jing.empty() && fs::exists(log_lu_jing)) {
		last_size = fs::file_size(log_lu_jing);
	}

	while (true) {
		try {
			if (log_lu_jing.empty() || !fs::exists(log_lu_jing)) {
				this_thread::sleep_for(chrono::milliseconds(500));
				continue;
			}

			auto cur_size = fs::file_size(log_lu_jing);
			if (cur_size < last_size) last_size = 0;
			if (cur_size > last_size) {
				ifstream file(log_lu_jing, ios::binary);
				if (file) {
					file.seekg(last_size);
					string line;
					while (getline(file, line)) {
						// A. 基础清理
						if (!line.empty() && line.back() == '\r') line.pop_back();
						if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line = line.substr(3);
						string clean_txt = regex_replace(line, prefix_reg, "");
						if (clean_txt.empty()) continue;

						// B. 逻辑分发 (只有在窗口焦点时才处理，避免误触)
						if (chuang_kou_focus) {
							DispatchLogCommand(clean_txt);
						}

						// C. UI 显示逻辑 (独立出来)
						if (show_gsi_raw) {
							if (clean_txt.find("Unknown command") == string::npos &&
								clean_txt.find("fps_set") == string::npos) {
								std::lock_guard<std::mutex> lock(g_output_mutex);
								GeiYanSe(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
								wcout << L"[CS2] " << wstring(clean_txt.begin(), clean_txt.end()) << endl;
								ResetColor();
							}
						}
					}
					last_size = cur_size;
				}
			}
		}
		catch (...) {}
		this_thread::sleep_for(chrono::milliseconds(1));
	}
}

// --- 模块: 看看游戏窗口还在不在前面
void ChuangKouJianCe() {
	while (true) {
		HWND current_hwnd = GetForegroundWindow();
		if (current_hwnd) {
			wchar_t title[256];
			GetWindowTextW(current_hwnd, title, 256);
			wstring wTitle(title);
			bool is_game = false;
			for (const auto& name : GAME_NAMES) {
				if (wTitle.find(name) != wstring::npos) { is_game = true; break; }
			}
			if (is_game != chuang_kou_focus) {
				chuang_kou_focus = is_game;
				if (!jing_yin_mo_shi) {
					GeiYanSe(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
					wcout << L"[SYSTEM] 游戏状态: " << (chuang_kou_focus ? L"ACTIVE" : L"IDLE") << endl;
				}
			}
		}
		this_thread::sleep_for(chrono::milliseconds(500));
	}
}


// 全局句柄
HANDLE hMapFile = NULL;
SharedInfo* pSharedData = nullptr;

int main(int argc, char* argv[]) {

	// A. 注册退出清理
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	//ProcessManager::KillProcess(L"DearMouseHook.exe");
	system("chcp 65001");

	// --- C. 写入共享内存版本号 ---
	hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedInfo), "KICore_Mouse_Ver");
	if (hMapFile) {
		pSharedData = (SharedInfo*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedInfo));
		if (pSharedData) {
			strcpy_s(pSharedData->version, "alpha-20260211");
			strcpy_s(pSharedData->module_name, "Mouse_Hook_Core");
			pSharedData->is_active = true;
		}
	}

	// --- 参数解析 ---
	KICore::jiting = false;
	g_safe_mode = false; // 默认为 false，意味着默认进入“无视环境强制抖动”模式

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		if (arg == "-quickstop") {
			KICore::jiting = true;
			continue;
		}

		if (arg == "-safe") {
			g_safe_mode = true;
			continue;
		}
	}

	SetProcessAffinityMask(GetCurrentProcess(), 1);
	(void)_setmode(_fileno(stdout), _O_U16TEXT);

	GeiYanSe(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	wcout << L"==================================================" << endl;
	if (KICore::jiting)
	{
		wcout << L"        KICore 2026 Pro - 含急停                  " << endl;
	}
	else {
		wcout << L"        KICore 2026 Pro - 常规版                  " << endl;
	}

	wcout << L"   [Pause]: 总开关 | [Home]: 刷屏开关             " << endl;
	wcout << L"   模式: " << (g_safe_mode ? L"安全模式 (焦点检测)" : L"强制模式 (始终抖动)") << endl;
	wcout << L"==================================================" << endl;

	// --- 打印自身版本 ---
	GeiYanSe(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	wcout << L"[CORE] 模块: Mouse_Hook_Core" << endl;
	wcout << L"[CORE] 版本: alpha-20260211" << endl;
	ResetColor();

	// --- 尝试侦测 GSI ---
	if (TryAttachGSI())
	{
		if (pGSI->is_active)
		{
			GeiYanSe(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			wcout << L"[GSI ] 模块: " << ToWString(pGSI->module_name) << endl;
			wcout << L"[GSI ] 版本: " << ToWString(pGSI->version) << endl;
		}
		else
		{
			GeiYanSe(FOREGROUND_RED | FOREGROUND_INTENSITY);
			wcout << L"[GSI ] 已侦测到，但当前未激活" << endl;
		}
	}
	else
	{
		GeiYanSe(FOREGROUND_RED | FOREGROUND_INTENSITY);
		wcout << L"[GSI ] 未侦测到 GSI_Broadcaster" << endl;
	}
	ResetColor();

	wcout << L"==================================================" << endl;

	bool has_ability = fs::exists(fs::current_path().parent_path() / L"resource" / L"cache" / L".userquickstop");

	// --- 启动 GSI 监听 ---
	StartGSIListener(OnGSIDataReceived);
	wcout << L"[INFO] GSI 监听已启动 (Port: 1009)" << endl;
	KICore::LoadConfigFromSharedMemory();
	log_lu_jing = ZhaoLogLuJing();
	if (!log_lu_jing.empty()) {
		wcout << L"[INFO] 日志路径抓到了: " << log_lu_jing << endl;
	}

	thread(ChuangKouJianCe).detach();
	thread(LogJianTing).detach();

	int fx_dir = 1;
	auto step_time = chrono::microseconds(1000000 / HZ);
	while (true) {
		// --- 1. 急停按键逻辑（不影响抖动，只影响注入） ---
		if (has_ability)
		{
			if ((GetAsyncKeyState('1') & 0x8000) || (GetAsyncKeyState('2') & 0x8000)) {
				KICore::jiting = true;
			}
			if ((GetAsyncKeyState('3') & 0x8000) || (GetAsyncKeyState('4') & 0x8000) ||
				(GetAsyncKeyState('5') & 0x8000) || (GetAsyncKeyState('Q') & 0x8000)) {
				KICore::jiting = false;
			}
		}

		// --- 2. 功能开关及 UI 交互 ---
		if (GetAsyncKeyState(VK_PAUSE) & 0x8000) {
			if (has_ability) {
				if (KICore::jiting) { g_pausejiting_ctrl = true; }
				else { g_pausejiting_ctrl = false; }
				KICore::jiting = !KICore::jiting;
				XiangYiXia(KICore::jiting);
				GeiYanSe(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				wcout << L"[SYSTEM] 硬件急停功能 -> " << (KICore::jiting ? L"已激活" : L"已禁用") << endl;
				ResetColor();
			}
			else {
				GeiYanSe(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				wcout << L"[SYSTEM] 硬件急停功能 -> 不具备能力" << endl;
				ResetColor();
			}
			this_thread::sleep_for(chrono::milliseconds(300));
		}

		if (GetAsyncKeyState(VK_HOME) & 0x8000) {
			show_gsi_raw = !show_gsi_raw;
			GeiYanSe(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			wcout << L"[UI] GSI 原始文本显示 -> " << (show_gsi_raw ? L"显示" : L"隐藏") << endl;
			ResetColor();
			this_thread::sleep_for(chrono::milliseconds(300));
		}

		// --- 3. 核心抖动决策 ---
		bool should_shake = false;
		if (!g_safe_mode) {
			// 强制模式也要听从日志指令的暂停
			should_shake = !g_manual_pause;
		}
		else {
			// 安全模式下，既要开关开启、有焦点，且没有被日志手动暂停
			should_shake = (kai_guan && chuang_kou_focus && !g_manual_pause);
		}

		if (should_shake) {
			auto start = chrono::high_resolution_clock::now();

			// 物理抖动
			INPUT mouse_in = { 0 };
			mouse_in.type = INPUT_MOUSE;

			if (input_mode == 0) {
				// --- 模式 0: 鼠标左右抖动 ---
				mouse_in.mi.dwFlags = MOUSEEVENTF_MOVE;
				mouse_in.mi.dx = DOU_DONG_RANGE * fx_dir;
				mouse_in.mi.dy = 0;
				//  SendInput(1, &mouse_in, sizeof(INPUT));
				fx_dir *= -1;
			}
			else
			{
				// --- 模式 1: 模拟滚轮 (适合 CS2 连跳/连点) ---
				mouse_in.mi.dwFlags = MOUSEEVENTF_WHEEL;
				mouse_in.mi.mouseData = (DWORD)240; // 这里的 120 尝试改成 240 或 480 看看
				SendInput(1, &mouse_in, sizeof(INPUT));
			}
			// 这里你可以根据 KICore::jiting 发送急停相关的指令注入...
			SendInput(1, &mouse_in, sizeof(INPUT));

			auto end = chrono::high_resolution_clock::now();
			auto diff = chrono::duration_cast<chrono::microseconds>(end - start);
			if (diff < step_time) this_thread::sleep_for(step_time - diff);
		}
		else {
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}
	return 0;
}

#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT) {
		if (pSharedData) {
			pSharedData->is_active = false; // 告诉 A 进程我下线了
			UnmapViewOfFile(pSharedData);
		}
		if (hMapFile) CloseHandle(hMapFile);

		// 如果有其他硬件驱动需要卸载，也写在这里
		return TRUE;
	}
	return FALSE;
}
#endif

