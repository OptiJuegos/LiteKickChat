@echo off
setlocal
cd /d "%~dp0"
if not defined MINGW32_BIN set "MINGW32_BIN=C:\msys64\mingw32\bin"
if not exist "%MINGW32_BIN%\g++.exe" (
  echo ERROR: Set MINGW32_BIN to your i686-w64-mingw32 compiler bin directory.
  exit /b 1
)
set "PATH=%MINGW32_BIN%;%PATH%"
set "KICKCHAT_USE_CURL=1"
if not exist "dist\curl-x86" mkdir "dist\curl-x86"
call compilar_kickchat.bat "dist\curl-x86\KickChat-Curl-x86.exe"
exit /b %ERRORLEVEL%
