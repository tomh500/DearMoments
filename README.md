 # DearMacro 指令引擎使用指南
 
 本项目是一个基于 C++ 开发的高性能可视化宏引擎。通过云端编译（GitHub Actions），你无需安装 Visual Studio 即可获得专属的宏执行程序。
 
 ## 🚀 快速开始
 
 1. **在线编程**：
    访问可视化编程网址（即本项目配套的编辑器），通过拖拽积木或编写 `Custom()` 逻辑完成你的宏设计。
 
 2. **获取代码**：
    在编辑器中点击“生成 UserMacro.cpp”，复制生成的 `namespace UserMacro { ... }` 整个代码块。
 
 3. **替换代码**：
    Fork 本仓库后，在 `applist/DearMacro` 分支下找到 `main.cpp`，将你复制的代码块替换掉原有的 `UserMacro` 命名空间部分。
 
 4. **云端编译**：
    提交（Commit & Push）你的修改。GitHub Actions 会自动触发编译流程。
 
 5. **下载程序**：
    等待约 1 分钟，点击仓库上方的 **Actions** 选项卡，找到最近的一次构建记录，在 **Artifacts** 栏目下载 `DearMacro-Binary` 压缩包。
 
 ## 📦 运行环境
 
 - 下载后的 `DearMacro.exe` 必须与 `DearMacro_.dll` 放在同一个文件夹内。
 - **推荐用法**：将编译出的文件放入 `DearMoments` 目录下，作为插件或配套工具一起使用。
 
 ## 🛠 乱码修复说明
 
 如果你发现控制台显示乱码，请确保你的 `main.cpp` 入口函数如下所示：
 
 int main() {
     system("chcp 65001");  强制控制台使用 UTF-8 编码
      ... 其余逻辑
 }
 
 ## ⚠️ 注意事项
 
 - 请勿随意修改 `UserMacro` 命名空间之外的代码，除非你清楚如何配置 C++ 链接参数。
 - 默认触发按键为 `SPACE`（空格），退出按键为 `END`。
