#include "UpdateChecker.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <wininet.h>
#include <shlwapi.h>
#include "Global.h"
#include <string>
#include "Resource.h"
#pragma comment(lib, "wininet.lib")
#include <filesystem>
using namespace std;
// �ⲿ�����汾�ű������������������ж��壩
extern int LocalVersion;
wchar_t title[256];


using namespace std;
using namespace filesystem;
namespace fs = std::filesystem;




void CheckForUpdate(HWND hWnd) {
    std::thread([hWnd]() {
        const wchar_t* url = L"https://tomh500.github.io/Square/version.txt";

        if (debug == 1) {
            MessageBoxW(hWnd, L"[����] ��ʼִ�� CheckForUpdate", L"����", MB_OK);
        }

        // ����������
        HINTERNET hInternet = InternetOpenW(L"UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[����] InternetOpenW ʧ��", L"����", MB_ICONERROR);
            }
            return;
        }

        // ��ȡԶ�̰汾
        HINTERNET hFile = InternetOpenUrlW(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (!hFile) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[����] InternetOpenUrlW ʧ��", L"����", MB_ICONERROR);
            }
            InternetCloseHandle(hInternet);
            return;
        }

        char buffer[32] = { 0 };
        DWORD bytesRead = 0;
        if (!InternetReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead) || bytesRead == 0) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[����] InternetReadFile ʧ��", L"����", MB_ICONERROR);
            }
            InternetCloseHandle(hFile);
            InternetCloseHandle(hInternet);
            return;
        }

        buffer[bytesRead] = '\0';
        int remoteVersion = atoi(buffer); // ����汾

        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);

        // ��ȡ���ذ汾�ļ� src/resources/version.txt
        int localVersion = -1;
        try {
            fs::path relativePath = L"..\\..\\..\\src\\resources\\version.txt";

            // ��ȡ��ǰ����Ŀ¼
            fs::path currentPath = fs::current_path();

            // �������·��
            fs::path absolutePath = currentPath / relativePath;

            // ��ӡ����·����������
            std::wcout << L"Absolute path: " << absolutePath << std::endl;

            // ʹ�þ���·�����ļ�
            std::ifstream localFile(absolutePath);
            if (!localFile.is_open()) {
                MessageBoxW(hWnd, L"�޷���ȡ���ذ汾�ļ�", L"����", MB_ICONERROR);
                return;
            }

            std::string line;
            std::getline(localFile, line);
            localVersion = std::stoi(line);
            localFile.close();
        }
        catch (...) {
            MessageBoxW(hWnd, L"��ȡ���ذ汾ʱ�����쳣", L"����", MB_ICONERROR);
            return;
        }

        if (debug == 1) {
            wchar_t msg[128];
            swprintf_s(msg, 128, L"[����] ���ذ汾��%d��Զ�̰汾��%d", localVersion, remoteVersion);
            MessageBoxW(hWnd, msg, L"����", MB_OK);
        }

        // �жϰ汾
        if (remoteVersion > localVersion) {
            SetWindowTextW(hWnd, L"ֿ����ʱ��CFG-Listener �����°汾���ã���");
            if (debug == 1) {
                MessageBoxW(hWnd, L"[����] ��⵽�°汾����", L"������ʾ", MB_ICONINFORMATION);
            }
        }
        else if (remoteVersion == localVersion) {
            SetWindowTextW(hWnd, L"ֿ����ʱ��CFG-Listener �����°棩");
            if (debug == 1) {
                MessageBoxW(hWnd, L"[����] ��ǰ�汾��������", L"����", MB_OK);
            }
        }
        else {
            // �쳣�����Զ�̰汾С�ڱ��ذ汾
            path devKeyPath = L"..\\..\\..\\..\\MoClient_development.key";
            if (!exists(devKeyPath)) {
                MessageBoxW(hWnd, L"�汾���쳣������ѯ�����ߣ�", L"����", MB_ICONERROR | MB_OK);
                ExitProcess(1);
            }
            else {
                MessageBoxW(hWnd, L"�汾���쳣�������ڿ�����ģʽ", L"����", MB_ICONWARNING | MB_OK);
            }
        }
        }).detach();
}
