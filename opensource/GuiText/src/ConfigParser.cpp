#include "ConfigParser.h"
#include "Utils.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cwchar>
#include <algorithm>
#include <regex>

void ConfigParser::ReadConfigFile() {
    configLines.clear();
    configMap.clear();
    moduleOptions.clear();

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path configPath(exePath);
    for (int i = 0; i < 3; ++i) {
        configPath = configPath.parent_path();
    }
    configPath = configPath / L"setting"  / L"UserSetting.cfg";

    std::ifstream file(configPath, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "无法打开配置文件: " << Utils::WideToUTF8(configPath.wstring()) << std::endl;
        return;
    }

    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::wstring wideContent = Utils::UTF8ToWide(fileContent);
    std::vector<std::wstring> allLines;
    size_t pos = 0;
    while (pos < wideContent.size()) {
        size_t end = wideContent.find(L'\n', pos);
        if (end == std::wstring::npos) {
            allLines.push_back(wideContent.substr(pos));
            break;
        }
        allLines.push_back(wideContent.substr(pos, end - pos));
        pos = end + 1;
    }

    // 第一遍：收集所有配置项并识别模块
    for (size_t i = 0; i < allLines.size(); i++) {
        std::wstring line = allLines[i];
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }

        if (line.empty() || line.find(L"//") == 0) {
            continue;
        }

        size_t start = line.find_first_not_of(L" \t");
        size_t end = line.find_last_not_of(L" \t");
        if (start == std::wstring::npos || end == std::wstring::npos) continue;

        std::wstring configValue = line.substr(start, end - start + 1);
        std::wstring displayText = L"";
        bool foundValidContent = false;

        // 检查当前行上面的注释行
        for (int j = static_cast<int>(i) - 1; j >= 0; j--) {
            if (allLines[j].find(L"//") == 0) {
                std::wstring bracketContent = ExtractBracketContent(allLines[j], configValue);
                if (!bracketContent.empty()) {
                    displayText = ProcessSpecialFormat(bracketContent);
                    foundValidContent = true;
                    break;
                }
            }
        }

        if (!foundValidContent) {
            displayText = configValue;
        }
        

        // 存储到配置映射
        configMap[configValue] = displayText;

        // 识别模块：提取模块前缀（最后一个下划线之前的部分）
size_t lastUnderscore = configValue.find_last_of(L'_');
if (lastUnderscore != std::wstring::npos) {
    std::wstring modulePrefix = configValue.substr(0, lastUnderscore);
    moduleOptions[modulePrefix].push_back(configValue);

    // 调试输出
    std::wcout << L"注册模块: " << modulePrefix
               << L" -> 选项: " << configValue << std::endl;
}
    }

    // 第二遍：解析跳转引用
    for (auto& pair : configMap) {
        pair.second = ResolveJumpReferences(pair.second);
    }

    // 第三遍：移除不需要的特殊标记
    for (auto& pair : configMap) {
        pair.second = RemoveUnwantedMarkers(pair.second);
    }

  // === 替换这部分开始 ===
    // 生成最终显示列表 - 跳过空行和NULL值
    std::vector<std::wstring> tempLines;
    for (const auto& pair : configMap) {
        if (pair.second.empty() ||
            pair.second == L"[%NULL%]" ||
            pair.second.find(L'%') != std::wstring::npos) {
            continue;
        }
        tempLines.push_back(pair.second);
    }

    // 按星号数量排序（星号越多越靠前）
    std::sort(tempLines.begin(), tempLines.end(), [](const std::wstring& a, const std::wstring& b) {
        // 计算星号数量
        auto countStars = [](const std::wstring& s) {
            return std::count(s.begin(), s.end(), L'*');
        };
        
        size_t countA = countStars(a);
        size_t countB = countStars(b);
        
        // 星号多的排前面
        if (countA != countB) {
            return countA > countB;
        }
        // 星号数量相同时按原顺序
        return false;
    });

    configLines = std::move(tempLines);
    // === 替换这部分结束 ===


    std::cout << "成功读取配置文件: " << Utils::WideToUTF8(configPath.wstring()) << std::endl;
    std::cout << "找到 " << configLines.size() << " 个配置项" << std::endl;
}

// 只移除不需要的标记，保留跳转引用
std::wstring ConfigParser::RemoveUnwantedMarkers(const std::wstring& input) {
    std::wstring result = input;

    // === 修改：处理 %extern 标记 ===
    size_t externPos = 0;
    while ((externPos = result.find(L"%extern ")) != std::wstring::npos) {
        size_t end = result.find(L'%', externPos + 8);
        if (end == std::wstring::npos) break;

        // 完全移除 %extern ...% 标记
        result.erase(externPos, end - externPos + 1);
    }

    // 移除 %NULL% 模式
    size_t nullPos = 0;
    while ((nullPos = result.find(L"%NULL%")) != std::wstring::npos) {
        result.erase(nullPos, 6);
    }

    return result;
}

std::wstring ConfigParser::ExtractBracketContent(const std::wstring& commentLine, const std::wstring& configValue) {
    // 查找配置值在注释行中的位置
    size_t pos = commentLine.find(configValue);
    if (pos == std::wstring::npos) return L"";

    // 向后查找第一个方括号
    size_t bracketStart = commentLine.find(L'[', pos);
    if (bracketStart == std::wstring::npos) return L"";

    // 向前查找最后一个方括号
    size_t bracketEnd = commentLine.find(L']', bracketStart);
    if (bracketEnd == std::wstring::npos) return L"";

    // 提取方括号内容
    return commentLine.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
}

std::wstring ConfigParser::ProcessSpecialFormat(const std::wstring& input) {
    std::wstring result;
    bool escape = false;

    for (wchar_t c : input) {
        if (escape) {
            switch (c) {
                case L'\\': result += L'\\'; break;
                case L'[': result += L'['; break;
                case L']': result += L']'; break;
                case L'%': result += L'%'; break;
                case L'n': result += L'\n'; break;
                default: result += c;
            }
            escape = false;
        } else if (c == L'\\') {
            escape = true;
        } else {
            result += c;
        }
    }

    if (escape) {
        result += L'\\';
    }

    return result;
}

std::wstring ConfigParser::ResolveJumpReferences(const std::wstring& input) {
    std::wstring result = input;
    const int maxDepth = 5;
    int depth = 0;
    bool changed;

    do {
        changed = false;
        size_t start = 0;

        while ((start = result.find(L"%jump->", start)) != std::wstring::npos) {
            size_t end = result.find(L'%', start + 7);
            if (end == std::wstring::npos) break;

            std::wstring refName = result.substr(start + 7, end - start - 7);
            std::wstring externContent = L"";

            // === 新逻辑：先尝试作为模块处理 ===
            bool handledAsModule = false;

            // 检查是否有下划线（可能是模块）
            size_t lastUnderscore = refName.find_last_of(L'_');
            if (lastUnderscore != std::wstring::npos) {
                std::wstring possibleModule = refName.substr(0, lastUnderscore);

                // 检查是否是已知模块
                auto moduleIt = moduleOptions.find(possibleModule);
                if (moduleIt != moduleOptions.end()) {
                    // 查找模块中实际选择的选项
                    for (const auto& option : moduleIt->second) {
                        auto configIt = configMap.find(option);
                        if (configIt != configMap.end()) {
                            // 尝试提取 extern 内容
                            externContent = ExtractExternContent(configIt->second);
                            if (!externContent.empty()) {
                                handledAsModule = true;
                                break;
                            }
                        }
                    }
                }
            }

            // === 如果不是模块，尝试作为具体配置项 ===
            if (!handledAsModule) {
                auto configIt = configMap.find(refName);
                if (configIt != configMap.end()) {
                    externContent = ExtractExternContent(configIt->second);
                }
            }

            // 替换引用
            if (!externContent.empty()) {
                result.replace(start, end - start + 1, externContent);
                changed = true;
                start += externContent.length();
            } else {
                // 保留原始引用格式以便调试
                //result.replace(start, end - start + 1, L"[未解析:" + refName + L"]");
                result.replace(start, end - start + 1, L"");
                changed = true;
                start += refName.length() + 8; // [未解析:].length
            }
        }

        depth++;
    } while (changed && depth < maxDepth);

    return result;
}

// 辅助函数：从文本中提取extern内容
std::wstring ConfigParser::ExtractExternContent(const std::wstring& text) {
    size_t externPos = text.find(L"%extern ");
    if (externPos == std::wstring::npos) return L"";

    size_t externEnd = text.find(L'%', externPos + 8);
    if (externEnd == std::wstring::npos) return L"";

    return text.substr(externPos + 8, externEnd - externPos - 8);
}

const std::vector<std::wstring>& ConfigParser::GetConfigLines() const {
    return configLines;
}

const std::map<std::wstring, std::wstring>& ConfigParser::GetConfigMap() const {
    return configMap;
}