@echo off
setlocal

set "APP_DIR=%~dp0build-qt683"
set "PATH=D:\Qt\6.8.3\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;%PATH%"

start "" /D "%APP_DIR%" "%APP_DIR%\GrowBoxDesktop.exe"
