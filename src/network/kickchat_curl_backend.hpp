#ifndef KICKCHAT_CURL_BACKEND_HPP
#define KICKCHAT_CURL_BACKEND_HPP

#include <curl/curl.h>

namespace curlBackend {

bool ready();
bool configureTls(CURL *handle);

}

#endif
