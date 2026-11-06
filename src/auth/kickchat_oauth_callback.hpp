#ifndef KICKCHAT_OAUTH_CALLBACK_HPP
#define KICKCHAT_OAUTH_CALLBACK_HPP

#include "kickchat_platform.hpp"

bool parseOAuthCallbackRequest(const std::string &request, const std::string &expectedState,
                               std::string &code, std::string &error);
bool receiveOAuthCallback(SOCKET listener, const std::string &expectedState,
                          std::string &code, std::wstring &reason);

#endif
