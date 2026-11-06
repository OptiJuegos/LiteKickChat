@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
echo KickChat MultiChat - WinAPI / Windows 7 SP1 / MinGW-w64 GCC

where g++ >nul 2>nul
if errorlevel 1 (
  echo ERROR: g++ was not found in PATH. Install MinGW-w64.
  pause
  exit /b 1
)

set "INCLUDES=-Isrc\app -Isrc\core -Isrc\chat -Isrc\network -Isrc\auth -Isrc\emotes -Isrc\links -Isrc\ui -Isrc\settings"
set "OUTPUT=%~1"
set "CURL_OPTIONS="
set "CURL_SOURCE="
set "TLS_SOURCE=src\network\kickchat_tls_schannel.cpp"
set "CURL_LIBRARY="
if /I "%KICKCHAT_USE_CURL%"=="1" (
  for /f %%A in ('g++ -dumpmachine') do set "COMPILER_TARGET=%%A"
  if not "!COMPILER_TARGET!"=="i686-w64-mingw32" (
    echo ERROR: This libcurl build requires an i686-w64-mingw32 compiler.
    exit /b 1
  )
  if not exist "src\curl\lib\libcurl.dll.a" (
    echo ERROR: Missing src\curl\lib\libcurl.dll.a
    exit /b 1
  )
  set "CURL_OPTIONS=-DKICKCHAT_USE_CURL -Isrc\curl\include"
  set "CURL_SOURCE=src\network\kickchat_curl_backend.cpp src\network\kickchat_curl_http.cpp src\network\kickchat_tls_curl.cpp"
  set "TLS_SOURCE="
  set "CURL_LIBRARY=src\curl\lib\libcurl.dll.a"
  if not defined OUTPUT set "OUTPUT=KickChat-Curl-x86.exe"
)
if not defined OUTPUT set "OUTPUT=KickChat.exe"

g++ -std=c++11 -O2 -g -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -municode -mwindows -static %INCLUDES% %CURL_OPTIONS% ^
  src\app\kickchat_ui.cpp src\app\kickchat_main_window_events.cpp src\app\kickchat_message_input.cpp src\app\kickchat_channel_views.cpp src\app\kickchat_chat_events.cpp src\app\kickchat_platform.cpp ^
  src\core\kickchat_json.cpp ^
  src\chat\kickchat_chat_data.cpp src\chat\kickchat_chat_paint.cpp src\chat\kickchat_history.cpp src\chat\kickchat_recent.cpp src\chat\kickchat_recent_win.cpp ^
  src\network\kickchat_http.cpp src\network\kickchat_api_http.cpp src\network\kickchat_stream.cpp src\network\kickchat_websocket.cpp src\network\kickchat_websocket_handshake.cpp %TLS_SOURCE% src\network\kickchat_websocket_frame.cpp %CURL_SOURCE% ^
  src\auth\kickchat_oauth.cpp src\auth\kickchat_oauth_callback.cpp src\auth\kickchat_category_lookup.cpp src\auth\kickchat_send.cpp src\auth\kickchat_credentials.cpp src\auth\kickchat_credentials_format.cpp ^
  src\emotes\kickchat_emotes.cpp src\emotes\kickchat_emote_render.cpp src\emotes\kickchat_emote_tokens.cpp ^
  src\links\kickchat_link_paint.cpp src\links\kickchat_link_preview.cpp src\links\kickchat_link_metadata.cpp src\links\kickchat_link_url.cpp src\links\kickchat_link_fetch_winhttp.cpp src\links\kickchat_link_tooltip.cpp src\links\kickchat_link_hover.cpp ^
  src\ui\kickchat_buttons.cpp src\ui\kickchat_settings.cpp src\ui\kickchat_settings_layout.cpp src\ui\kickchat_main_controls.cpp src\ui\kickchat_chat_selection.cpp src\ui\kickchat_profile.cpp src\ui\kickchat_profile_paint.cpp src\ui\kickchat_profile_lookup.cpp src\ui\kickchat_channel_dialog.cpp src\ui\kickchat_badges.cpp ^
  src\settings\kickchat_preferences.cpp src\settings\kickchat_channels.cpp ^
  -o "%OUTPUT%" -lgdiplus -lole32 -lws2_32 -lsecur32 -lwininet -lwinhttp -lcrypt32 -lshell32 -ladvapi32 -lgdi32 -luser32 %CURL_LIBRARY%
if errorlevel 1 (
  echo ERROR: The build failed. Check the GCC diagnostics above.
  pause
  exit /b 1
)

if /I "%KICKCHAT_USE_CURL%"=="1" (
  for %%F in ("%OUTPUT%") do (
    copy /Y "src\curl\bin\libcurl.dll" "%%~dpFlibcurl.dll" >nul
    if errorlevel 1 exit /b 1
    copy /Y "src\curl\bin\curl-ca-bundle.crt" "%%~dpFcurl-ca-bundle.crt" >nul
    if errorlevel 1 exit /b 1
  )
)

echo OK: %OUTPUT% created with debug symbols.
pause
