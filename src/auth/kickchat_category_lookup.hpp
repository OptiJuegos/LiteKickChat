#ifndef KICKCHAT_CATEGORY_LOOKUP_HPP
#define KICKCHAT_CATEGORY_LOOKUP_HPP

#include <string>

bool lookupKickCategory(const std::wstring &requested, std::string &categoryId,
                        std::wstring &categoryName, std::wstring &error);

#endif
