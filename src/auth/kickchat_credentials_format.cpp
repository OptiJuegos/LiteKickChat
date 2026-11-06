#include "kickchat_credentials_format.hpp"

bool validCredentialField(const std::string &field, size_t limit) {
    if (field.empty() || field.size() > limit) return false;
    for (size_t i = 0; i < field.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(field[i]);
        if (c <= 32 || c >= 127) return false;
    }
    return true;
}

std::string encodeCredentialRecord(const std::string &clientId, const std::string &clientSecret) {
    return "KICKCHAT1\n" + clientId + "\n" + clientSecret;
}

bool decodeCredentialRecord(const std::string &data, std::string &clientId, std::string &clientSecret) {
    clientId.clear();
    clientSecret.clear();
    const std::string header = "KICKCHAT1\n";
    if (data.compare(0, header.size(), header) != 0) return false;
    size_t separator = data.find('\n', header.size());
    if (separator == std::string::npos) return false;
    std::string id = data.substr(header.size(), separator - header.size());
    std::string secret = data.substr(separator + 1);
    if (!validCredentialField(id, 200) || !validCredentialField(secret, 300)) return false;
    clientId.swap(id);
    clientSecret.swap(secret);
    return true;
}
