#pragma once
#include <vector>
#include <string>
#include <map>

class ConfigParser {
public:
    void ReadConfigFile();
    const std::vector<std::wstring>& GetConfigLines() const;
    const std::map<std::wstring, std::wstring>& GetConfigMap() const;

private:
std::wstring RemoveUnwantedMarkers(const std::wstring& input);
    std::wstring ExtractBracketContent(const std::wstring& commentLine, const std::wstring& configValue);
    std::wstring ProcessSpecialFormat(const std::wstring& input);
    std::wstring ResolveJumpReferences(const std::wstring& input);
    std::wstring RemoveAllSpecialMarkers(const std::wstring& input); // 新增函数
        std::wstring ExtractExternParam(const std::wstring& commentLine);
    std::wstring ExtractExternContent(const std::wstring& text);
    std::vector<std::wstring> configLines;
    std::map<std::wstring, std::wstring> configMap;
    std::map<std::wstring, std::vector<std::wstring>> moduleOptions;
};