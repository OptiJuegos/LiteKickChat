@echo off
setlocal
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 exit /b 1
if not exist "build\tests" mkdir "build\tests"
if errorlevel 1 exit /b 1
set "INCLUDES=-Isrc\app -Isrc\core -Isrc\chat -Isrc\network -Isrc\auth -Isrc\emotes -Isrc\links -Isrc\ui -Isrc\settings"

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_commands.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_commands.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_commands.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_json.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_json.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_json.exe
if errorlevel 1 exit /b 1


g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_credentials.cpp src\auth\kickchat_credentials_format.cpp -o build\tests\test_kickchat_credentials.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_credentials.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_chat_data.cpp src\chat\kickchat_chat_data.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_chat_data.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_chat_data.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_emotes.cpp src\emotes\kickchat_emote_tokens.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_emotes.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_emotes.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_channels.cpp src\settings\kickchat_channels.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_channels.exe -lshell32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_channels.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_recent.cpp src\chat\kickchat_recent.cpp src\chat\kickchat_chat_data.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_recent.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_recent.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_preferences.cpp src\settings\kickchat_preferences.cpp src\links\kickchat_link_preview.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_preferences.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_preferences.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_link_metadata.cpp src\links\kickchat_link_metadata.cpp src\links\kickchat_link_url.cpp src\links\kickchat_link_fetch_winhttp.cpp -o build\tests\test_kickchat_link_metadata.exe -lwinhttp -lws2_32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_link_metadata.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_link_url.cpp src\links\kickchat_link_url.cpp -o build\tests\test_kickchat_link_url.exe -lwinhttp
if errorlevel 1 exit /b 1
build\tests\test_kickchat_link_url.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_scroll.cpp -o build\tests\test_kickchat_scroll.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_scroll.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_profile.cpp src\ui\kickchat_profile.cpp src\ui\kickchat_profile_paint.cpp src\ui\kickchat_profile_lookup.cpp src\core\kickchat_json.cpp -municode -mwindows -o build\tests\test_kickchat_profile.exe -lgdi32 -luser32 -lshell32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_profile.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_text_selection.cpp -o build\tests\test_kickchat_text_selection.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_text_selection.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_edit_keys.cpp -o build\tests\test_kickchat_edit_keys.exe -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_edit_keys.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_settings_layout.cpp src\ui\kickchat_settings_layout.cpp -o build\tests\test_kickchat_settings_layout.exe -lgdi32 -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_settings_layout.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_oauth_callback.cpp src\auth\kickchat_oauth_callback.cpp -o build\tests\test_kickchat_oauth_callback.exe -lws2_32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_oauth_callback.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_api_commands.cpp src\auth\kickchat_category_lookup.cpp src\auth\kickchat_send.cpp src\core\kickchat_json.cpp -o build\tests\test_kickchat_api_commands.exe -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_api_commands.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_message_input.cpp src\app\kickchat_message_input.cpp -o build\tests\test_kickchat_message_input.exe -luser32 -lshell32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_message_input.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_channel_views.cpp src\app\kickchat_channel_views.cpp src\app\kickchat_chat_events.cpp -o build\tests\test_kickchat_channel_views.exe -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_channel_views.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_localization.cpp -o build\tests\test_kickchat_localization.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_localization.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_main_controls.cpp src\ui\kickchat_main_controls.cpp -o build\tests\test_kickchat_main_controls.exe -luser32 -lgdi32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_main_controls.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_chat_selection.cpp src\ui\kickchat_chat_selection.cpp -o build\tests\test_kickchat_chat_selection.exe -luser32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_chat_selection.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_websocket_frame.cpp src\network\kickchat_websocket_frame.cpp -o build\tests\test_kickchat_websocket_frame.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_frame.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra %INCLUDES% tests\test_kickchat_websocket_handshake.cpp src\network\kickchat_websocket_handshake.cpp -o build\tests\test_kickchat_websocket_handshake.exe
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_handshake.exe
if errorlevel 1 exit /b 1

g++ -std=c++11 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 %INCLUDES% tests\test_kickchat_websocket_transport.cpp src\network\kickchat_websocket.cpp src\network\kickchat_websocket_frame.cpp src\network\kickchat_websocket_handshake.cpp -o build\tests\test_kickchat_websocket_transport.exe -ladvapi32
if errorlevel 1 exit /b 1
build\tests\test_kickchat_websocket_transport.exe
if errorlevel 1 exit /b 1

echo All portable tests passed.
