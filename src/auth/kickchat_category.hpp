#ifndef KICKCHAT_CATEGORY_HPP
#define KICKCHAT_CATEGORY_HPP

#include "kickchat_json.hpp"
#include <windows.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <vector>
#include <cwctype>

namespace kickcategory {
enum Match { NOT_FOUND, FOUND, AMBIGUOUS, INVALID_RESPONSE };

inline Match findExact(const Json &response, const std::wstring &requested,
                       std::string &categoryId, std::wstring &categoryName) {
    const Json &data = response.get("data");
    if (data.type != Json::ARRAY) return INVALID_RESPONSE;
    for (size_t i = 0; i < data.arr.size(); ++i) {
        const Json &item = data.arr[i];
        const Json &name = item.get("name");
        if (item.type != Json::OBJECT || name.type != Json::STRING) return INVALID_RESPONSE;
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       name.value.c_str(), (int)name.value.size(), NULL, 0);
        if (count <= 0) return INVALID_RESPONSE;
        std::wstring canonical(count, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                 name.value.c_str(), (int)name.value.size(), &canonical[0], count))
            return INVALID_RESPONSE;
        if (CompareStringOrdinal(canonical.c_str(), (int)canonical.size(),
                                 requested.c_str(), (int)requested.size(), TRUE) != CSTR_EQUAL)
            continue;

        const Json &id = item.get("id");
        if (id.type != Json::PRIMITIVE || id.value.empty()) return INVALID_RESPONSE;
        errno = 0;
        char *end = NULL;
        unsigned long long numericId = strtoull(id.value.c_str(), &end, 10);
        if (errno || !end || *end || !numericId || numericId > LLONG_MAX) return INVALID_RESPONSE;
        if (!categoryId.empty() && categoryId != id.value) return AMBIGUOUS;
        categoryId = id.value;
        categoryName = canonical;
    }
    return categoryId.empty() ? NOT_FOUND : FOUND;
}

struct Candidate {
    std::string id;
    std::wstring name;
    int score;
};

enum Choice { NO_MATCH, CLEAR_MATCH, NEEDS_CHOICE };

namespace detail {

inline bool equal(const std::wstring &a, const std::wstring &b) {
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()),
                                b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

inline std::wstring trim(const std::wstring &value) {
    size_t first = 0, last = value.size();
    while (first < last && iswspace(value[first])) ++first;
    while (last > first && iswspace(value[last - 1])) --last;
    return value.substr(first, last - first);
}

inline bool wordCharacter(wchar_t c) {
    WORD type = 0;
    return GetStringTypeW(CT_CTYPE1, &c, 1, &type) != 0 &&
           (type & (C1_ALPHA | C1_DIGIT)) != 0;
}

inline bool digit(wchar_t c) { return c >= L'0' && c <= L'9'; }

inline std::wstring roman(const std::wstring &token) {
    if (token == L"i") return L"1";
    if (token == L"ii") return L"2";
    if (token == L"iii") return L"3";
    if (token == L"iv") return L"4";
    if (token == L"v") return L"5";
    if (token == L"vi") return L"6";
    if (token == L"vii") return L"7";
    if (token == L"viii") return L"8";
    if (token == L"ix") return L"9";
    if (token == L"x") return L"10";
    return token;
}

inline std::wstring normalize(const std::wstring &value) {
    if (value.empty() || value.size() > 512) return L"";
    int size = LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE,
                            value.c_str(), static_cast<int>(value.size()), NULL, 0);
    if (size <= 0) return L"";
    std::wstring lower(size, L'\0');
    if (LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE,
                     value.c_str(), static_cast<int>(value.size()), &lower[0], size) != size)
        return L"";
    std::wstring tokens, token;
    for (size_t i = 0; i <= lower.size(); ++i) {
        const wchar_t c = i == lower.size() ? L' ' : lower[i];
        if (wordCharacter(c)) {
            if (!token.empty() && digit(c) != digit(token.back()) &&
                (digit(c) || digit(token.back()))) {
                if (!tokens.empty()) tokens += L' ';
                tokens += roman(token);
                token.clear();
            }
            token += c;
        } else if (!token.empty()) {
            if (!tokens.empty()) tokens += L' ';
            tokens += roman(token);
            token.clear();
        }
    }
    return tokens;
}

inline std::wstring baseName(const std::wstring &value) {
    std::wstring trimmed = trim(value);
    if (trimmed.empty() || trimmed.back() != L')') return trimmed;
    size_t opening = trimmed.rfind(L'(');
    if (opening == std::wstring::npos || opening == 0) return trimmed;
    return trim(trimmed.substr(0, opening));
}

inline std::wstring gtaNumber(const std::wstring &normalized) {
    if (normalized == L"gta") return L"";
    if (normalized.compare(0, 4, L"gta ") == 0)
        return normalized.substr(4);
    if (normalized.compare(0, 3, L"gta") == 0 && normalized.size() > 3)
        return roman(normalized.substr(3));
    return L"";
}

inline bool numericToken(const std::wstring &value, std::wstring &number) {
    number.clear();
    for (size_t i = 0; i < value.size(); ++i) {
        if (digit(value[i])) {
            size_t end = i;
            while (end < value.size() && digit(value[end])) ++end;
            number += value.substr(i, end - i) + L" ";
            i = end - 1;
        }
    }
    return !number.empty();
}

inline std::wstring acronym(const std::wstring &value) {
    std::wstring out;
    bool newWord = true;
    for (size_t i = 0; i < value.size(); ++i) {
        wchar_t c = value[i];
        if (c == L' ') { newWord = true; continue; }
        if (digit(c)) {
            if (!out.empty() && out.back() != L' ') out += L' ';
            size_t end = i;
            while (end < value.size() && digit(value[end])) ++end;
            out += value.substr(i, end - i);
            i = end - 1;
        } else if (newWord) {
            out += c;
        }
        newWord = false;
    }
    return out;
}

inline int editDistance(const std::wstring &a, const std::wstring &b) {
    if (a.empty() || b.empty() || a.size() > 128 || b.size() > 128) return 99;
    if (a.size() > b.size() + 2 || b.size() > a.size() + 2) return 99;
    std::vector<int> previous(b.size() + 1), current(b.size() + 1), before(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = static_cast<int>(j);
    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            int replacement = previous[j - 1] + (a[i - 1] != b[j - 1]);
            int insertion = current[j - 1] + 1;
            int deletion = previous[j] + 1;
            current[j] = std::min(replacement, std::min(insertion, deletion));
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] &&
                a[i - 2] == b[j - 1])
                current[j] = std::min(current[j], before[j - 2] + 1);
        }
        before.swap(previous);
        previous.swap(current);
    }
    return previous[b.size()];
}

inline int score(const std::wstring &requested, const std::wstring &name) {
    const std::wstring query = normalize(requested);
    const std::wstring full = normalize(name);
    const std::wstring base = normalize(baseName(name));
    if (query.empty() || full.empty()) return 0;
    if (equal(query, full)) return 1000;
    if (equal(query, base)) return 985;

    std::wstring expanded = query;
    const std::wstring gta = gtaNumber(query);
    if (!gta.empty() && gta.find(L' ') == std::wstring::npos &&
        gta.size() <= 2 && digit(gta[0]))
        expanded = L"grand theft auto " + gta;
    if (!equal(expanded, query) && equal(expanded, base)) return 970;

    const std::wstring abbrev = acronym(base);
    if (equal(abbrev, query) && query.size() >= 4 && query.find(L' ') != std::wstring::npos)
        return 950;
    if (query == L"gta" &&
        (base.compare(0, 17, L"grand theft auto ") == 0 ||
         base.compare(0, 4, L"gta ") == 0)) return 730;
    if (equal(abbrev, query) && query.size() >= 3) return 700;

    const std::wstring &comparison = expanded;
    std::wstring requestedNumbers, candidateNumbers;
    numericToken(comparison, requestedNumbers);
    numericToken(base, candidateNumbers);
    if (requestedNumbers != candidateNumbers) return 0;

    size_t longest = std::max(comparison.size(), base.size());
    if (longest >= 7 && comparison.size() >= 6 && base.size() >= 6) {
        int edits = editDistance(comparison, base);
        if (edits == 1) return 935;
        if (edits == 2 && longest >= 10) return 865;
    }
    if (comparison.size() >= 3 && base.size() > comparison.size() &&
        base.compare(0, comparison.size(), comparison) == 0) return 640;
    if (comparison.size() >= 4 && base.find(comparison) != std::wstring::npos)
        return 610;
    return 0;
}

inline bool validId(const std::string &value, std::string &normalized) {
    if (value.empty()) return false;
    for (size_t i = 0; i < value.size(); ++i)
        if (value[i] < '0' || value[i] > '9') return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long id = strtoull(value.c_str(), &end, 10);
    if (errno || !end || *end || !id || id > LLONG_MAX) return false;
    normalized = std::to_string(id);
    return true;
}

inline bool validName(const std::string &utf8, std::wstring &name) {
    if (utf8.empty() || utf8.size() > 512) return false;
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                  static_cast<int>(utf8.size()), NULL, 0);
    if (size <= 0) return false;
    name.resize(size);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                            static_cast<int>(utf8.size()), &name[0], size) != size)
        return false;
    return name.find(L'\0') == std::wstring::npos && !normalize(name).empty();
}

inline bool candidateBefore(const Candidate &a, const Candidate &b) {
    if (a.score != b.score) return a.score > b.score;
    int ordering = CompareStringOrdinal(a.name.c_str(), -1, b.name.c_str(), -1, TRUE);
    return ordering == CSTR_LESS_THAN || (ordering == CSTR_EQUAL && a.id < b.id);
}

inline void appendTerm(std::vector<std::wstring> &terms, const std::wstring &term) {
    const std::wstring candidate = trim(term);
    if (candidate.empty() || candidate.size() > 100 || terms.size() >= 4) return;
    size_t letters = 0;
    size_t numbers = 0;
    for (size_t i = 0; i < candidate.size(); ++i) {
        WORD type = 0;
        if (GetStringTypeW(CT_CTYPE1, &candidate[i], 1, &type) && (type & C1_ALPHA))
            ++letters;
        if (digit(candidate[i])) ++numbers;
    }
    if (letters < 3 && !(letters >= 2 && numbers && candidate.size() >= 3)) return;
    for (size_t i = 0; i < terms.size(); ++i)
        if (equal(terms[i], candidate)) return;
    terms.push_back(candidate);
}

} // namespace detail

inline bool appendCandidates(const Json &response, const std::wstring &requested,
                             std::vector<Candidate> &candidates) {
    const Json &data = response.get("data");
    if (data.type != Json::ARRAY) return false;
    std::vector<Candidate> collected = candidates;
    for (size_t i = 0; i < data.arr.size(); ++i) {
        const Json &entry = data.arr[i];
        const Json &id = entry.get("id"), &name = entry.get("name");
        if (entry.type != Json::OBJECT || id.type != Json::PRIMITIVE ||
            name.type != Json::STRING) return false;
        Candidate candidate;
        if (!detail::validId(id.value, candidate.id) ||
            !detail::validName(name.value, candidate.name)) return false;
        candidate.score = detail::score(requested, candidate.name);
        bool exists = false;
        for (size_t j = 0; j < collected.size(); ++j) {
            if (collected[j].id == candidate.id) {
                if (candidate.score > collected[j].score) collected[j] = candidate;
                exists = true;
                break;
            }
        }
        if (!exists) collected.push_back(candidate);
    }
    candidates.swap(collected);
    return true;
}

inline Choice chooseCandidate(const std::wstring &requested,
                              const std::vector<Candidate> &candidates,
                              Candidate &best, std::vector<Candidate> &suggestions) {
    best = Candidate();
    suggestions.clear();
    std::vector<Candidate> ranked;
    for (size_t i = 0; i < candidates.size(); ++i) {
        Candidate candidate = candidates[i];
        candidate.score = detail::score(requested, candidate.name);
        if (candidate.score >= 600) ranked.push_back(candidate);
    }
    if (ranked.empty()) return NO_MATCH;
    std::sort(ranked.begin(), ranked.end(), detail::candidateBefore);
    best = ranked.front();
    const int runner = ranked.size() > 1 ? ranked[1].score : 0;
    std::wstring number;
    const bool numberedAlias = best.score >= 950 &&
        detail::numericToken(detail::normalize(requested), number);
    if ((best.score >= 970 && best.score > runner) ||
        (best.score >= 930 && (detail::normalize(requested).size() >= 7 || numberedAlias) &&
         best.score - runner >= 90)) return CLEAR_MATCH;
    for (size_t i = 0; i < ranked.size() && suggestions.size() < 3; ++i)
        suggestions.push_back(ranked[i]);
    return NEEDS_CHOICE;
}

inline std::vector<std::wstring> searchTerms(const std::wstring &requested) {
    std::vector<std::wstring> terms;
    detail::appendTerm(terms, requested);
    const std::wstring query = detail::normalize(requested);
    const std::wstring number = detail::gtaNumber(query);
    if (!number.empty() && number.size() <= 2 && detail::digit(number[0])) {
        detail::appendTerm(terms, L"Grand Theft Auto " + number);
        detail::appendTerm(terms, L"GTA " + number);
        if (number == L"5") detail::appendTerm(terms, L"Grand Theft Auto V");
        else if (number == L"4") detail::appendTerm(terms, L"Grand Theft Auto IV");
    } else if (query == L"gta") {
        detail::appendTerm(terms, L"Grand Theft Auto");
    } else if (query.compare(0, 17, L"grand theft auto ") == 0) {
        std::wstring suffix = query.substr(17);
        if (suffix.size() <= 2 && !suffix.empty() && detail::digit(suffix[0])) {
            detail::appendTerm(terms, L"GTA " + suffix);
            detail::appendTerm(terms, L"GTA" + suffix);
            detail::appendTerm(terms, L"Grand Theft Auto " + suffix);
        }
    } else if (query == L"cs 2") {
        detail::appendTerm(terms, L"Counter-Strike 2");
    }
    if (terms.size() < 4) {
        const size_t first = query.find(L' ');
        if (first != std::wstring::npos) {
            const size_t second = query.find(L' ', first + 1);
            const size_t prefixEnd = second == std::wstring::npos ? first : second;
            if (prefixEnd >= 4) detail::appendTerm(terms, query.substr(0, prefixEnd));
        } else if (query.size() >= 6) {
            detail::appendTerm(terms, query.substr(0, 4));
        }
    }
    return terms;
}
} // namespace kickcategory

#endif
