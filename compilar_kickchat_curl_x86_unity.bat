@echo off
setlocal
cd /d "%~dp0"

if not defined MINGW32_BIN set "MINGW32_BIN=C:\msys64\mingw32\bin"
if not exist "%MINGW32_BIN%\g++.exe" (
  echo ERROR: Set MINGW32_BIN to the i686-w64-mingw32 compiler directory.
  exit /b 1
)
if not exist "src\curl\lib\libcurl.dll.a" (
  echo ERROR: Missing bundled libcurl import library.
  exit /b 1
)
set "PATH=%MINGW32_BIN%;%PATH%"

for /f %%A in ('g++ -dumpmachine') do set "COMPILER_TARGET=%%A"
if not "%COMPILER_TARGET%"=="i686-w64-mingw32" (
  echo ERROR: This Unity Build needs the i686-w64-mingw32 compiler.
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\generate_kickchat_unity.ps1"
if errorlevel 1 exit /b 1

if not exist "dist\curl-x86" mkdir "dist\curl-x86"
set "OUTPUT=%~1"
if not defined OUTPUT set "OUTPUT=dist\curl-x86\KickChat-Curl-Unity-x86.exe"
set "INCLUDES=-Isrc\app -Isrc\core -Isrc\chat -Isrc\network -Isrc\auth -Isrc\emotes -Isrc\links -Isrc\ui -Isrc\settings -Isrc\curl\include"

g++ -std=c++11 -O2 -g -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DKICKCHAT_USE_CURL -municode -mwindows -static %INCLUDES% ^
  @build/unity-curl-x86/unity_sources.rsp -o "%OUTPUT%" ^
  -lgdiplus -lole32 -lws2_32 -lsecur32 -lwininet -lwinhttp -lcrypt32 -lshell32 -ladvapi32 -lgdi32 -luser32 src\curl\lib\libcurl.dll.a
if errorlevel 1 (
  echo ERROR: Unity Build failed.
  exit /b 1
)

for %%F in ("%OUTPUT%") do (
  copy /Y "src\curl\bin\libcurl.dll" "%%~dpFlibcurl.dll" >nul
  if errorlevel 1 exit /b 1
  copy /Y "src\curl\bin\curl-ca-bundle.crt" "%%~dpFcurl-ca-bundle.crt" >nul
  if errorlevel 1 exit /b 1
)

echo OK: %OUTPUT% created with Unity Build.
