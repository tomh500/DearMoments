@echo off
if exist "Asul_Editor.ico" (
    copy "Asul_Editor.ico" "dist\"
)
nuitka --windows-console-mode=disable --standalone --onefile --windows-icon-from-ico=Asul_Editor.ico --include-package=tkinter --enable-plugin=tk-inter --output-dir=dist --output-filename=Asul_Editor main.py


