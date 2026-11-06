#ifndef KICKCHAT_PROFILE_LOOKUP_HPP
#define KICKCHAT_PROFILE_LOOKUP_HPP

#include "kickchat_platform.hpp"

namespace profileLookup {

const UINT Message = WM_APP + 21;

struct Request {
    HWND window;
    std::string username;
    std::string id;
};

bool validUsername(const std::string &name);
bool validNumericId(const std::string &value);
DWORD WINAPI worker(LPVOID parameter);

}

#endif
