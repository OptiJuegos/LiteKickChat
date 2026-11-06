#include "kickchat_channels.hpp"
#include "kickchat_json.hpp"
#ifdef _WIN32
#include "kickchat_platform.hpp"
#include <shlobj.h>
#endif

std::string normalizeChannel(const std::string &value) {
    std::string name = value;
    const std::string prefix = "https://kick.com/";
    if (name.compare(0, prefix.size(), prefix) == 0) name.erase(0, prefix.size());
    if (name.empty() || name.size() > 32) return "";
    for (size_t i = 0; i < name.size(); ++i) {
        char &c = name[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') continue;
        return "";
    }
    return name;
}

#ifdef _WIN32
static std::wstring channelsFile(bool create) {
    wchar_t appdata[MAX_PATH] = {0};
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata) != S_OK) return L"";
    std::wstring folder = std::wstring(appdata) + L"\KickChat";
    if (create && !CreateDirectoryW(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    return folder + L"\channels.json";
}

bool loadChannels(std::vector<std::string> &channels) {
    channels.clear();
    const std::wstring file = channelsFile(false);
    if (file.empty()) return false;
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(handle, NULL);
    if (size == INVALID_FILE_SIZE || size > 4096) { CloseHandle(handle); return false; }
    std::string bytes(size, '\0');
    DWORD read = 0;
    bool ok = !size || (ReadFile(handle, &bytes[0], size, &read, NULL) && read == size);
    CloseHandle(handle);
    Json root;
    if (!ok || !parseJson(bytes, root) || root.type != Json::ARRAY) return false;
    for (size_t i = 0; i < root.arr.size() && channels.size() < 8; ++i) {
        std::string slug = normalizeChannel(root.arr[i].str());
        if (!slug.empty() && std::find(channels.begin(), channels.end(), slug) == channels.end())
            channels.push_back(slug);
    }
    return true;
}

bool saveChannels(const std::vector<std::string> &channels) {
    const std::wstring file = channelsFile(true);
    if (file.empty() || channels.size() > 8) return false;
    std::string data = "[";
    for (size_t i = 0; i < channels.size(); ++i) {
        if (normalizeChannel(channels[i]) != channels[i]) return false;
        if (i) data += ",";
        data += jsonEscape(channels[i]);
    }
    data += "]\n";
    const std::wstring temp = file + L".tmp";
    HANDLE handle = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), &written, NULL) &&
              written == data.size() && FlushFileBuffers(handle);
    CloseHandle(handle);
    if (ok) ok = MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temp.c_str());
    return ok;
}
#else
bool loadChannels(std::vector<std::string> &channels) { channels.clear(); return false; }
bool saveChannels(const std::vector<std::string> &) { return false; }
#endif
