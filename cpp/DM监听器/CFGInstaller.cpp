#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "Tools.h"
#include "Global.h"
using namespace std;
using namespace std::filesystem;

void AppendIfMissing(const path& filePath, const string& lineToAdd)
{
    string content;
    ifstream in(filePath, ios::binary);
    if (in) {
        content.assign((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
        in.close();
    }

    if (content.find(lineToAdd) == string::npos) {
        ofstream out(filePath, ios::app | ios::binary);
        if (!out) {
            MessageBoxW(nullptr, L"�޷��� autoexec.cfg ����д��", L"����", MB_OK | MB_ICONERROR);
            return;
        }

        // ��ѡ���Զ�������
        if (!content.empty() && content.back() != '\n') {
            out << "\n";
        }

        out << lineToAdd << "\n";
        MessageBoxW(nullptr, L"�ѳɹ���� exec DearMoments/Setup �� autoexec.cfg", L"���", MB_OK | MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(nullptr, L"autoexec.cfg �Ѱ��� exec DearMoments/Setup���������", L"��Ϣ", MB_OK | MB_ICONINFORMATION);
    }
}

int CFGInstaller()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    path CFGRootPath = path(exePath).parent_path(); // ��ǰĿ¼
    CFGRootPath = CFGRootPath.parent_path();        // ��һ��
    CFGRootPath = CFGRootPath.parent_path();        // ����һ��
    CFGRootPath = CFGRootPath.parent_path();        // ��������Ŀ¼

    path srcRoot = CFGRootPath / L"src" / L"resources";

    // �ļ����Ʋ�����һ��ʧ���������� 1
    if (!CopyFile(
        (srcRoot / L"DearMoments_Installed.cfg").wstring(),
        (CFGRootPath.parent_path().parent_path().parent_path() / L"cfg" / L"DearMoments_Installed.cfg").wstring()
    )) return 1;

    if (!CopyFile(
        (srcRoot / L"keybindings_schinese.txt").wstring(),
        (CFGRootPath.parent_path().parent_path() / L"resource" / L"keybindings_schinese.txt").wstring()
    )) return 1;

    if (!CopyFile(
        (srcRoot / L"keybindings_english.txt").wstring(),
        (CFGRootPath.parent_path().parent_path() / L"resource" / L"keybindings_english.txt").wstring()
    )) return 1;

    if (!CopyFile(
        (srcRoot / L"gamestate_integration_dearmoments.cfg").wstring(),
        (CFGRootPath.parent_path() / L"gamestate_integration_dearmoments.cfg").wstring()
    )) return 1;

    //��Sounds�´���Ŀ¼
    path soundsDir = CFGRootPath.parent_path().parent_path() / L"sounds" / L"DearMoments";
    path musicModeDir = soundsDir / L"musicmode";
    path tempDir = soundsDir / L"temp";

    try {
        create_directories(musicModeDir);
        create_directories(tempDir);
    }
    catch (const filesystem_error& e) {
        if (debug==1) {
            wstring err = L"����Ŀ¼ʧ�ܣ�\n" + soundsDir.wstring() +
                L"\n������Ϣ��" + String2WString(e.what());
            MessageBoxW(nullptr, err.c_str(), L"����Ŀ¼ʧ��", MB_OK | MB_ICONERROR);
        }
        return 3;
    }
	if (StartApps(L"LinkListener.exe", L"", false) != 0)
	{
		MessageBoxW(nullptr, L"���� LinkListener.exe ʧ��", L"����", MB_OK | MB_ICONERROR);
		return 4;
	}
    // ���Ƴɹ���ѯ���Ƿ���ӵ� autoexec.cfg
    int response = MessageBoxW(nullptr, L"ȫ���ļ��ѳɹ����ƣ��Ƿ���ӵ� autoexec.cfg��", L"����ȷ��", MB_YESNO | MB_ICONQUESTION);
    if (response == IDYES) {
        path autoexecPath = CFGRootPath.parent_path() / L"autoexec.cfg";

        try {
            // ���������򴴽�
            if (!exists(autoexecPath)) {
                ofstream create(autoexecPath); // UTF-8 Ĭ�ϴ�
                if (!create) throw runtime_error("���� autoexec.cfg ʧ��");
                create.close();
            }

            AppendIfMissing(autoexecPath, "exec DearMoments/Setup");
        }
        catch (...) {
            MessageBoxW(nullptr, L"д�� autoexec.cfg ʧ��", L"����", MB_OK | MB_ICONERROR);
            return 2;
        }
    }

    return 0;
}