# CS2 RTSS FPS Controller

> 一个基于 RTSS 的键盘锁帧/解锁工具，支持按键触发 FPS 限制，延迟模式和立即触发模式。
>
> Powered by AsulTop & SquareTeam

---

## 目录

- [简介](#简介)
- [功能](#功能)
- [配置示例](#配置示例)
- [使用方法](#使用方法)
- [键绑定说明](#键绑定说明)
- [线程与安全](#线程与安全)
- [注意事项](#注意事项)

---

## 简介

此程序通过读取 YAML 配置文件和 RTSS API，实现对指定游戏的 FPS 锁定/解锁。  
支持按键按下立即触发或延迟触发锁帧，同时兼容多线程安全执行。

程序使用了：
- C++17
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) 解析配置
- Windows API 获取键盘状态
- RTSS 接口管理 FPS 配置

---

## 功能

- 按键锁定/解锁 FPS
- 支持延迟触发
- 支持立即触发模式
- 自动生成默认配置
- 支持多 profile 绑定
- 控制台调试输出

---

## 配置示例

默认配置文件路径：`rtss_config.yml`

```yaml
ReadConsole: false
cs2.exe:
  binds:
    - key: SPACE
      fps: 64
      hold: true
      delay: false
```
---

## 字段说明

| 字段            | 类型     | 描述               |
| ------------- | ------ | ---------------- |
| `ReadConsole` | bool   | 是否启用控制台读取（目前不支持） |
| `key`         | string | 键名（如 SPACE、F1、A） |
| `fps`         | int    | 要锁定的帧数           |
| `hold`        | bool   | 是否按住触发           |
| `delay`       | bool   | 按住模式下是否延迟触发      |

## 编译说明

此项目仅公开了 CPP 和 H 文件，如果要编译，需要准备以下环境和依赖：

### 依赖

1. **C++17 支持的编译器**  
   - Visual Studio 2019/2022  
   - 或 MinGW-w64 / g++ 9+  

2. **第三方库**  
   - [yaml-cpp](https://github.com/jbeder/yaml-cpp)（用于解析 YAML 配置文件）  
   - RTSS SDK / DLL（RTSSApi 文件夹提供接口封装）  

3. **Windows SDK**  
   - 需要 `WinUser.h`、`windows.h` 等头文件  

---

### Visual Studio 编译

1. 创建一个 **空项目**  
2. 将所有 `.cpp` 和 `.h` 文件添加到项目中  
3. 配置项目属性：
   - C/C++ → 语言 → C++17  
   - 链接器 → 输入 → 附加依赖项：`yaml-cpp.lib`、RTSS SDK 所需库  
4. 确保 `yaml-cpp` 的头文件路径添加到 C/C++ → 常规 → 附加包含目录  
5. 编译即可生成可执行文件  

---

