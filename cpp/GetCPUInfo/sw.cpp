#include <iostream>
#include <filesystem>
#include <string>
#include <Windows.h>  // ��� Windows.h ͷ�ļ���֧�� Windows API
using namespace std;
// wstring ת UTF-8 string
string WStringToString(const wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], size_needed, NULL, NULL);
    return str;
}