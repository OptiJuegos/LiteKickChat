#ifndef KICKCHAT_BUTTONS_HPP
#define KICKCHAT_BUTTONS_HPP

#include "kickchat_platform.hpp"

static const int ID_LOGIN = 3001;
static const int ID_SEND = 3002;
static const int ID_SETTINGS = 3004;
static const int ID_ADD_CHANNEL = 3005;

bool drawRoundedButton(const DRAWITEMSTRUCT *item, HFONT font,
                       COLORREF backgroundColor, bool primary);
bool drawActionButton(const DRAWITEMSTRUCT *item, HFONT font);

#endif
