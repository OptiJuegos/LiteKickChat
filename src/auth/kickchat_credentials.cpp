#include "kickchat_credentials.hpp"
#include "kickchat_credentials_format.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_strings.hpp"
#include <shlobj.h>

static bool credentialsPath(std::wstring &path, bool create) {
    wchar_t folder[MAX_PATH] = {0};
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder) != S_OK)
        return false;
    std::wstring directory = std::wstring(folder) + L"\\KickChat";
    if (create && !CreateDirectoryW(directory.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    path = directory + L"\\credentials.dat";
    return true;
}

bool loadCredentials(std::string &clientId, std::string &clientSecret) {
    clientId.clear();
    clientSecret.clear();
    std::wstring path;
    if (!credentialsPath(path, false)) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD length = GetFileSize(file, NULL);
    if (length == INVALID_FILE_SIZE || length < 16 || length > 8192) {
        CloseHandle(file);
        return false;
    }
    std::string encrypted(length, '\0');
    DWORD read = 0;
    bool ok = ReadFile(file, &encrypted[0], length, &read, NULL) != 0 && read == length;
    CloseHandle(file);
    if (!ok) return false;

    DATA_BLOB source = {length, reinterpret_cast<BYTE *>(&encrypted[0])};
    DATA_BLOB decoded = {0, NULL};
    if (!CryptUnprotectData(&source, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &decoded))
        return false;
    if (!decoded.pbData || decoded.cbData == 0) {
        if (decoded.pbData) LocalFree(decoded.pbData);
        return false;
    }
    std::string plain(reinterpret_cast<char *>(decoded.pbData), decoded.cbData);
    SecureZeroMemory(decoded.pbData, decoded.cbData);
    LocalFree(decoded.pbData);
    bool decodedOk = decodeCredentialRecord(plain, clientId, clientSecret);
    if (!plain.empty()) SecureZeroMemory(&plain[0], plain.size());
    return decodedOk;
}

bool saveCredentials(const std::string &clientId, const std::string &clientSecret,
                     bool &changed, std::wstring &error) {
    error.clear();
    changed = false;
    if (!validCredentialField(clientId, 200) || !validCredentialField(clientSecret, 300)) {
        error = kickchatText::get(kickchatText::CredentialInvalidField);
        return false;
    }
    std::string previousId, previousSecret;
    bool existed = loadCredentials(previousId, previousSecret);
    changed = !existed || previousId != clientId || previousSecret != clientSecret;
    if (!previousSecret.empty()) SecureZeroMemory(&previousSecret[0], previousSecret.size());
    if (!changed) return true;

    std::wstring path;
    if (!credentialsPath(path, true)) {
        error = kickchatText::get(kickchatText::CredentialFolderError);
        return false;
    }
    std::string plain = encodeCredentialRecord(clientId, clientSecret);
    DATA_BLOB source = {static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE *>(&plain[0])};
    DATA_BLOB encoded = {0, NULL};
    BOOL protectedOk = CryptProtectData(&source, L"KickChat OAuth credentials", NULL, NULL, NULL,
                                         CRYPTPROTECT_UI_FORBIDDEN, &encoded);
    SecureZeroMemory(&plain[0], plain.size());
    if (!protectedOk) {
        error = kickchatText::get(kickchatText::CredentialEncryptError);
        return false;
    }
    std::wstring temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = kickchatText::get(kickchatText::CredentialCreateError);
        LocalFree(encoded.pbData);
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, encoded.pbData, encoded.cbData, &written, NULL) != 0 &&
              written == encoded.cbData && FlushFileBuffers(file) != 0;
    CloseHandle(file);
    LocalFree(encoded.pbData);
    if (ok) ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        error = kickchatText::get(kickchatText::CredentialSaveError);
        return false;
    }
    return true;
}
