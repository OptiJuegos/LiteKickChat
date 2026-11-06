#ifndef KICKCHAT_HTTP_HPP
#define KICKCHAT_HTTP_HPP

#include <string>
#include <cstddef>

bool httpGet(const std::wstring &url, std::string &body, size_t maximumBytes);

#endif
