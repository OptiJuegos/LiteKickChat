#include "kickchat_curl_backend.hpp"
#include "kickchat_platform.hpp"
#include <vector>

namespace curlBackend {
namespace {

const std::vector<char> &certificateBundle() {
    static const std::vector<char> certificate = []() {
        std::vector<char> bytes;
        wchar_t module[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(NULL, module, MAX_PATH);
        if (!length || length >= MAX_PATH) return bytes;
        std::wstring path(module);
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return bytes;
        path.resize(slash + 1);
        path += L"curl-ca-bundle.crt";
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) return bytes;
        const DWORD fileSize = GetFileSize(file, NULL);
        if (fileSize > 0 && fileSize <= 1024 * 1024) {
            bytes.resize(fileSize);
            DWORD read = 0;
            if (!ReadFile(file, &bytes[0], fileSize, &read, NULL) || read != fileSize)
                bytes.clear();
        }
        CloseHandle(file);
        return bytes;
    }();
    return certificate;
}

struct CurlSession {
    bool initialized;
    CurlSession() : initialized(false) {
        if (curl_global_sslset(CURLSSLBACKEND_OPENSSL, NULL, NULL) == CURLSSLSET_OK)
            initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }
    ~CurlSession() { if (initialized) curl_global_cleanup(); }
};

} // namespace

bool ready() {
    static CurlSession session;
    return session.initialized;
}

bool configureTls(CURL *handle) {
    const std::vector<char> &certificate = certificateBundle();
    if (!handle || certificate.empty()) return false;
    curl_blob ca = {const_cast<char *>(&certificate[0]), certificate.size(), CURL_BLOB_NOCOPY};
    // The CA bytes remain valid for the lifetime of every curl handle.
    return curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &ca) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) == CURLE_OK;
}

} // namespace curlBackend
