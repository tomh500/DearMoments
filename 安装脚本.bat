chcp 65001
@echo off


cd /d %~dp0
cd ../
cd ../


set "EXPECTED_FOLDER_NAME=csgo"

for %%F in (.) do set "CURRENT_FOLDER_NAME=%%~nxF"

echo Current folder name: %CURRENT_FOLDER_NAME%
echo Expected folder name: %EXPECTED_FOLDER_NAME%

if /I "%CURRENT_FOLDER_NAME%" neq "%EXPECTED_FOLDER_NAME%" (
    echo ERROR: The current folder is wrong!! Please check the Guide again
    echo 错误: 你的 MoClient 放置位置错误， 请重新查看教程的安装指引
    pause
    exit /b 1
)

if not exist "%~dp0..\..\..\cfg" (
    mkdir "%~dp0..\..\..\cfg\"
    echo GAMEROOT Does not exist... Created
)

del /f /s /q "%~dp0..\..\..\cfg\"
xcopy "%~dp0src\resource" "%~dp0..\..\resource\" /Y /E

cd "%~dp0..\"

setlocal enabledelayedexpansion

:: 设置autoexec.cfg文件路径
set "AUTOEXEC_FILE=autoexec.cfg"

:: 检查autoexec.cfg文件是否存在
if not exist "%AUTOEXEC_FILE%" (
    echo %AUTOEXEC_FILE% does not exist. Creating it...
    echo. > "%AUTOEXEC_FILE%"
)

:: 获取文件的最后一行
set "LAST_LINE="
for /f "tokens=* delims=" %%A in ('type "%AUTOEXEC_FILE%" ^| findstr /r /n ".*"') do (
    set "LAST_LINE=%%A"
)

:: 提取文件最后一行内容
for /f "tokens=2 delims=:" %%B in ("!LAST_LINE!") do set "LAST_LINE_CONTENT=%%B"

:: 输出最后一行内容，用于调试
echo Last line content: !LAST_LINE_CONTENT!

:: 检查最后一行是否是 "exec MoClient/Setup"
if /i not "!LAST_LINE_CONTENT!"=="exec MoClient/Setup" (
    if /i not "!LAST_LINE_CONTENT!"=="exec MoClient/Setup " (
        echo Last line is not "exec MoClient/Setup". Adding it to the file...
        echo.  >> "%AUTOEXEC_FILE%" 
        echo exec MoClient/Setup >> "%AUTOEXEC_FILE%"
    ) else (
        echo Last line is already "exec MoClient/Setup". No changes made.
    )
) else (
    echo Last line is already "exec MoClient/Setup". No changes made.
)

echo Done
echo 运行结束
echo 按任意键关闭
echo 此文件只是复制几个文件 方便操作 无任何风险 你可以关闭此窗口了

pause