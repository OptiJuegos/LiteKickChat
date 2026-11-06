#include "kickchat_preferences.hpp"
#include "kickchat_json.hpp"

#ifdef _WIN32
#include "kickchat_platform.hpp"
#include <shlobj.h>
#endif

bool parseVisualPreferences(const std::string &data, VisualPreferences &settings) {
    Json root;
    if (!parseJson(data, root) || root.type != Json::OBJECT) return false;
    const Json &top = root.get("alwaysOnTop");
    const Json &links = root.get("linkPreviews");
    const Json &gifs = root.get("disableGifs");
    const Json &language = root.get("language");
    if (top.type != Json::PRIMITIVE || links.type != Json::PRIMITIVE ||
        (top.value != "true" && top.value != "false") ||
        (links.value != "true" && links.value != "false") ||
        (gifs.type != Json::NIL &&
         (gifs.type != Json::PRIMITIVE ||
          (gifs.value != "true" && gifs.value != "false"))) ||
        (language.type != Json::NIL &&
         (language.type != Json::STRING ||
          (language.value != "es" && language.value != "en")))) return false;
    settings.alwaysOnTop = top.value == "true";
    settings.linkPreviews = links.value == "true";
    settings.disableGifs = gifs.type == Json::PRIMITIVE && gifs.value == "true";
    settings.language = language.type == Json::STRING && language.value == "en" ?
        kickchatText::English : kickchatText::Spanish;
    return true;
}

std::string serializeVisualPreferences(const VisualPreferences &settings) {
    return std::string("{\"alwaysOnTop\":") + (settings.alwaysOnTop ? "true" : "false") +
        ",\"linkPreviews\":" + (settings.linkPreviews ? "true" : "false") +
        ",\"disableGifs\":" + (settings.disableGifs ? "true" : "false") +
        ",\"language\":\"" + (settings.language == kickchatText::English ? "en" : "es") + "\"}\n";
}

#ifdef _WIN32
static std::wstring settingsPath(bool create) {
    wchar_t appdata[MAX_PATH] = {0};
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata) != S_OK)
        return L"";
    const std::wstring folder = std::wstring(appdata) + L"\\KickChat";
    if (create && !CreateDirectoryW(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return L"";
    return folder + L"\\settings.json";
}

bool loadVisualPreferences(VisualPreferences &settings) {
    settings = VisualPreferences();
    const std::wstring path = settingsPath(false);
    if (path.empty()) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size > 1024) { CloseHandle(file); return false; }
    std::string data(size, '\0');
    DWORD read = 0;
    const bool ok = !size || (ReadFile(file, &data[0], size, &read, NULL) && read == size);
    CloseHandle(file);
    return ok && parseVisualPreferences(data, settings);
}

bool saveVisualPreferences(const VisualPreferences &settings) {
    const std::wstring path = settingsPath(true);
    if (path.empty()) return false;
    const std::wstring temp = path + L".tmp";
    const std::string data = serializeVisualPreferences(settings);
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, NULL) &&
              written == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temp.c_str(), path.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temp.c_str());
    return ok;
}
#else
bool loadVisualPreferences(VisualPreferences &settings) { settings = VisualPreferences(); return false; }
bool saveVisualPreferences(const VisualPreferences &) { return false; }
#endif
