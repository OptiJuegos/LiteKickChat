#include "kickchat_http.hpp"
#include "kickchat_api_http.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_curl_backend.hpp"
#include <curl/curl.h>
#include <limits>

namespace {

struct CurlResponse {
    std::string body;
    size_t limit;
    bool exceeded;
    explicit CurlResponse(size_t maximum) : limit(maximum), exceeded(false) {}
};

size_t receiveBody(char *data, size_t size, size_t count, void *user) {
    CurlResponse &response = *static_cast<CurlResponse *>(user);
    if (size && count > std::numeric_limits<size_t>::max() / size) return 0;
    const size_t bytes = size * count;
    if (response.body.size() > response.limit ||
        bytes > response.limit - response.body.size()) {
        response.exceeded = true;
        return 0;
    }
    response.body.append(data, bytes);
    return bytes;
}

bool configure(CURL *handle, const std::string &url, CurlResponse &response, bool redirects) {
    return curlBackend::configureTls(handle) &&
           curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receiveBody) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, redirects ? 1L : 0L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 3L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 5000L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "") == CURLE_OK &&
           curl_easy_setopt(handle, CURLOPT_USERAGENT, "KickChat/1.0") == CURLE_OK;
}

bool appendHeader(curl_slist *&headers, const char *value) {
    curl_slist *next = curl_slist_append(headers, value);
    if (!next) return false;
    headers = next;
    return true;
}

} // namespace

bool httpGet(const std::wstring &url, std::string &body, size_t maximumBytes) {
    body.clear();
    if (url.compare(0, 8, L"https://") != 0 || !maximumBytes ||
        maximumBytes > 1024 * 1024 || !curlBackend::ready()) return false;
    CURL *handle = curl_easy_init();
    if (!handle) return false;
    CurlResponse response(maximumBytes);
    const std::string address = utf8FromWide(url);
    const bool configured = configure(handle, address, response, true);
    long status = 0;
    const CURLcode code = configured ? curl_easy_perform(handle) : CURLE_FAILED_INIT;
    if (code == CURLE_OK) curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(handle);
    if (code != CURLE_OK || response.exceeded || status != 200 || response.body.empty())
        return false;
    body.swap(response.body);
    return true;
}

HttpResult kickHttp(const wchar_t *host, const wchar_t *path, const wchar_t *method,
                    const std::string &body, const std::string &bearer, bool form) {
    HttpResult result;
    if (!host || !path || !method || path[0] != L'/' ||
        (wcscmp(host, L"api.kick.com") != 0 && wcscmp(host, L"id.kick.com") != 0) ||
        (wcscmp(method, L"GET") != 0 && wcscmp(method, L"POST") != 0 &&
         wcscmp(method, L"PATCH") != 0 && wcscmp(method, L"DELETE") != 0) ||
        body.size() > 131072 || !curlBackend::ready()) {
        result.error = "Invalid HTTP request or curl initialization failed";
        return result;
    }
    CURL *handle = curl_easy_init();
    if (!handle) { result.error = "curl_easy_init failed"; return result; }
    CurlResponse response(131072);
    const std::string url = "https://" + utf8FromWide(host) + utf8FromWide(path);
    const std::string verb = utf8FromWide(method);
    bool configured = configure(handle, url, response, false);
    configured = configured && curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, verb.c_str()) == CURLE_OK;
    if (!body.empty()) {
        configured = configured &&
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.data()) == CURLE_OK &&
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size())) == CURLE_OK;
    }
    curl_slist *headers = NULL;
    bool headersOk = appendHeader(headers, "Accept: application/json");
    if (!body.empty() && headersOk)
        headersOk = appendHeader(headers, form ?
            "Content-Type: application/x-www-form-urlencoded" :
            "Content-Type: application/json; charset=utf-8");
    std::string authorization;
    if (!bearer.empty() && headersOk) {
        authorization = "Authorization: Bearer " + bearer;
        headersOk = appendHeader(headers, authorization.c_str());
    }
    configured = configured && headersOk &&
                 curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers) == CURLE_OK;
    const CURLcode code = configured ? curl_easy_perform(handle) : CURLE_FAILED_INIT;
    long status = 0;
    if (code == CURLE_OK)
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    if (response.exceeded) result.error = "HTTP response exceeded 128 KiB";
    else if (code != CURLE_OK) result.error = curl_easy_strerror(code);
    result.status = static_cast<DWORD>(status);
    result.body.swap(response.body);
    if (!authorization.empty()) SecureZeroMemory(&authorization[0], authorization.size());
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    return result;
}
