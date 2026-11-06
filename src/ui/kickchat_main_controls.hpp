#ifndef KICKCHAT_MAIN_CONTROLS_HPP
#define KICKCHAT_MAIN_CONTROLS_HPP

#include "kickchat_platform.hpp"

static const int ID_EDIT = 3003;
static const int ID_CHAT_SCROLLBAR = 3004;

struct MainControls {
    HWND addButton, sendButton, messageEdit, settingsButton, chatScrollbar;
    WNDPROC originalEditProc;
    MainControls() : addButton(NULL), sendButton(NULL), messageEdit(NULL),
                     settingsButton(NULL), chatScrollbar(NULL), originalEditProc(NULL) {}
};

void layoutMainControls(HWND window, const MainControls &controls);
void createMainControls(HWND window, HINSTANCE instance, HFONT font,
                        WNDPROC editProc, MainControls &controls);

#endif
