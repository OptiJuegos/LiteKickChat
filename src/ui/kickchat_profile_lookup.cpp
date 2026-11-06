#include "kickchat_profile_lookup.hpp"
#include "kickchat_http.hpp"
#include <cerrno>
#include <climits>

namespace profileLookup {

bool validUsername(const std::string &name) {
    if (name.empty() || name.size() > 25) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool validNumericId(const std::string &value) {
    if (value.empty() || value.size() > 19) return false;
    for (size_t i = 0; i < value.size(); ++i)
        if (value[i] < '0' || value[i] > '9') return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(value.c_str(), &end, 10);
    return !errno && end && !*end && parsed && parsed <= LLONG_MAX;
}

DWORD WINAPI worker(LPVOID parameter) {
    Request *job = static_cast<Request *>(parameter);
    std::string json;
    Json response;
    if (httpGet(L"https://kick.com/api/v1/users/" + toWide(job->username), json, 1024 * 1024) &&
        parseJson(json, response) && response.type == Json::OBJECT) {
        const std::string id = response.get("id").str();
        std::string returnedName = response.get("username").str();
        if (returnedName.empty()) returnedName = response.get("name").str();
        const bool matches = returnedName.empty() ||
            CompareStringOrdinal(toWide(job->username).c_str(), -1,
                                 toWide(returnedName).c_str(), -1, TRUE) == CSTR_EQUAL;
        if (matches && validNumericId(id)) job->id = id;
    }
    if (!PostMessageW(job->window, Message, 0, reinterpret_cast<LPARAM>(job)))
        delete job;
    return 0;
}

} // namespace profileLookup
