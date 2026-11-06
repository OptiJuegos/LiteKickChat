#ifndef KICKCHAT_CREDENTIALS_FORMAT_HPP
#define KICKCHAT_CREDENTIALS_FORMAT_HPP

#include <string>

bool validCredentialField(const std::string &field, size_t limit);
std::string encodeCredentialRecord(const std::string &clientId, const std::string &clientSecret);
bool decodeCredentialRecord(const std::string &data, std::string &clientId, std::string &clientSecret);

#endif
