#ifndef KICKCHAT_EDIT_KEYS_HPP
#define KICKCHAT_EDIT_KEYS_HPP

#include <windows.h>
#include <string>

inline bool isHighSurrogate(wchar_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

inline bool isLowSurrogate(wchar_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

inline size_t previousEditCharacter(const std::wstring &text, size_t position) {
    if (!position) return 0;
    --position;
    if (position && isLowSurrogate(text[position]) && isHighSurrogate(text[position - 1]))
        --position;
    return position;
}

inline size_t nextEditCharacter(const std::wstring &text, size_t position) {
    if (position == text.size()) return position;
    return position + (isHighSurrogate(text[position]) &&
                       position + 1 < text.size() && isLowSurrogate(text[position + 1]) ? 2 : 1);
}

inline int editCharacterClass(wchar_t character) {
    WORD type = 0;
    if (character == L' ' || character == L'\t' ||
        (GetStringTypeW(CT_CTYPE1, &character, 1, &type) && (type & C1_SPACE)))
        return 0;
    if (character == L'_' || (type & (C1_ALPHA | C1_DIGIT))) return 1;
    return 2;
}

inline size_t editWordBoundary(const std::wstring &text, size_t caret, bool backwards) {
    size_t position = caret;
    if (backwards) {
        while (position) {
            const size_t previous = previousEditCharacter(text, position);
            if (editCharacterClass(text[previous]) != 0) break;
            position = previous;
        }
        if (!position) return 0;
        const int kind = editCharacterClass(text[previousEditCharacter(text, position)]);
        while (position) {
            const size_t previous = previousEditCharacter(text, position);
            if (editCharacterClass(text[previous]) != kind) break;
            position = previous;
        }
    } else if (position < text.size()) {
        const int kind = editCharacterClass(text[position]);
        while (position < text.size() && editCharacterClass(text[position]) == kind)
            position = nextEditCharacter(text, position);
        while (position < text.size() && editCharacterClass(text[position]) == 0)
            position = nextEditCharacter(text, position);
    }
    return position;
}

inline bool handleEditControlShortcut(HWND edit, WPARAM key, bool controlPressed) {
    if (!controlPressed) return false;
    if (key == 'A') {
        SendMessageW(edit, EM_SETSEL, 0, -1);
        return true;
    }
    if (key != VK_BACK && key != VK_DELETE) return false;

    DWORD start = 0, end = 0;
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start),
                 reinterpret_cast<LPARAM>(&end));
    if (start == end) {
        const int length = GetWindowTextLengthW(edit);
        std::wstring text(length + 1, L'\0');
        GetWindowTextW(edit, &text[0], length + 1);
        text.resize(length);
        const size_t boundary = editWordBoundary(text, start, key == VK_BACK);
        if (key == VK_BACK) start = static_cast<DWORD>(boundary);
        else end = static_cast<DWORD>(boundary);
    }
    if (start != end) {
        SendMessageW(edit, EM_SETSEL, start, end);
        SendMessageW(edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
    }
    return true;
}

#endif
