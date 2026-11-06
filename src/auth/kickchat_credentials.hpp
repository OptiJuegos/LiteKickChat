#ifndef KICKCHAT_CREDENTIALS_HPP
#define KICKCHAT_CREDENTIALS_HPP

#include <string>

bool loadCredentials(std::string &clientId, std::string &clientSecret);
bool saveCredentials(const std::string &clientId, const std::string &clientSecret,
                     bool &changed, std::wstring &error);

#endif
