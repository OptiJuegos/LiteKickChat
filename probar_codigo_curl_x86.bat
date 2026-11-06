@echo off
setlocal
cd /d "%~dp0"
if not defined MINGW32_BIN set "MINGW32_BIN=C:\msys64\mingw32\bin"
if not exist "%MINGW32_BIN%\g++.exe" (
  echo ERROR: Set MINGW32_BIN to an i686-w64-mingw32 compiler.
  exit /b 1
)
set "PATH=%MINGW32_BIN%;src\curl\bin;%PATH%"
if not exist "build\tests" mkdir "build\tests"
copy /Y "src\curl\bin\curl-ca-bundle.crt" "build\tests\curl-ca-bundle.crt" >nul
if errorlevel 1 exit /b 1
g++ -std=c++11 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DKICKCHAT_USE_CURL -Isrc\app -Isrc\core -Isrc\network -Isrc\chat -Isrc\curl\include ^
  tests\test_kickchat_curl_http.cpp src\network\kickchat_curl_http.cpp src\network\kickchat_curl_backend.cpp src\curl\lib\libcurl.dll.a ^
  -o "build\tests\test_kickchat_curl_http.exe" -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_curl_http.exe
if errorlevel 1 exit /b 1
g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 ^
  -Isrc\app -Isrc\core -Isrc\chat -Isrc\network -Isrc\curl\include ^
  tests\test_kickchat_tls_curl.cpp src\network\kickchat_tls_curl.cpp ^
  src\network\kickchat_curl_backend.cpp src\network\kickchat_websocket_handshake.cpp ^
  src\network\kickchat_websocket_frame.cpp ^
  src\curl\lib\libcurl.dll.a ^
  -o build\tests\test_kickchat_tls_curl_x86.exe -lws2_32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_tls_curl_x86.exe
if errorlevel 1 exit /b 1
g++ -std=c++11 -Wall -Wextra -Isrc\network ^
  tests\test_kickchat_websocket_frame.cpp src\network\kickchat_websocket_frame.cpp ^
  -o build\tests\test_kickchat_websocket_frame_x86.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_frame_x86.exe
if errorlevel 1 exit /b 1
g++ -std=c++11 -Wall -Wextra -Isrc\network ^
  tests\test_kickchat_websocket_handshake.cpp src\network\kickchat_websocket_handshake.cpp ^
  -o build\tests\test_kickchat_websocket_handshake_x86.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_handshake_x86.exe
if errorlevel 1 exit /b 1
g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 ^
  -Isrc\app -Isrc\core -Isrc\chat -Isrc\network ^
  tests\test_kickchat_websocket_transport.cpp src\network\kickchat_websocket.cpp ^
  src\network\kickchat_websocket_frame.cpp src\network\kickchat_websocket_handshake.cpp ^
  -o build\tests\test_kickchat_websocket_transport_x86.exe -ladvapi32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_transport_x86.exe
exit /b %ERRORLEVEL%
