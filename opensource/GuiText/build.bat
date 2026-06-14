@echo off
setlocal

set COMPILER=cl
set OPTIONS= /utf-8 /MT /EHsc /std:c++17 /DUNICODE /D_UNICODE /I".\include"
set SOURCES=src\main.cpp src\ConfigParser.cpp src\Renderer.cpp src\HotkeyManager.cpp src\Utils.cpp
set LIBS=user32.lib gdi32.lib shell32.lib
set OUTPUT=DearMomentsViewer.exe

%COMPILER% %OPTIONS% %SOURCES% /link %LIBS% /out:%OUTPUT%

endlocal
