#include "kickchat_link_metadata.hpp"
#include "kickchat_link_url.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cwctype>
#include <map>

namespace {

const size_t MAX_HTML_BYTES = 512 * 1024;

std::string asciiLower(std::string text) {
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

std::string trimAscii(const std::string &text) {
    size_t start = 0, end = text.size();
    while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(start, end - start);
}

std::wstring trimWide(const std::wstring &text) {
    size_t start = 0, end = text.size();
    while (start < end && (text[start] <= 32 || text[start] == 0x00a0)) ++start;
    while (end > start && (text[end - 1] <= 32 || text[end - 1] == 0x00a0)) --end;
    return text.substr(start, end - start);
}

bool utf8Wide(const std::string &text, std::wstring &out) {
    out.clear();
    if (text.empty()) return true;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                    static_cast<int>(text.size()), NULL, 0);
    if (count <= 0) return false;
    out.resize(count);
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                               static_cast<int>(text.size()), &out[0], count) == count;
}

void appendUtf8(std::string &text, unsigned cp) {
    if (cp <= 0x7f) text += static_cast<char>(cp);
    else if (cp <= 0x7ff) {
        text += static_cast<char>(0xc0 | (cp >> 6));
        text += static_cast<char>(0x80 | (cp & 63));
    } else if (cp <= 0xffff) {
        text += static_cast<char>(0xe0 | (cp >> 12));
        text += static_cast<char>(0x80 | ((cp >> 6) & 63));
        text += static_cast<char>(0x80 | (cp & 63));
    } else {
        text += static_cast<char>(0xf0 | (cp >> 18));
        text += static_cast<char>(0x80 | ((cp >> 12) & 63));
        text += static_cast<char>(0x80 | ((cp >> 6) & 63));
        text += static_cast<char>(0x80 | (cp & 63));
    }
}

std::string decodeEntities(const std::string &text) {
    static const struct { const char *name; unsigned value; } named[] = {
        {"amp",38},{"lt",60},{"gt",62},{"quot",34},{"apos",39},{"nbsp",160},
        {"copy",169},{"reg",174},{"trade",8482},{"hellip",8230},{"ndash",8211},
        {"mdash",8212},{"lsquo",8216},{"rsquo",8217},{"ldquo",8220},
        {"rdquo",8221},{"bull",8226},{"euro",8364},{"laquo",171},{"raquo",187},
        {"iexcl",161},{"iquest",191},{"middot",183},{"times",215},
        {"divide",247},{"para",182},{"sect",167},{"deg",176},
        {"aacute",225},{"eacute",233},{"iacute",237},{"oacute",243},
        {"uacute",250},{"ntilde",241},{"uuml",252},{"agrave",224},
        {"egrave",232},{"igrave",236},{"ograve",242},{"ugrave",249},
        {"acirc",226},{"ecirc",234},{"icirc",238},{"ocirc",244},
        {"ucirc",251},{"auml",228},{"euml",235},{"iuml",239},
        {"ouml",246},{"ccedil",231},{"Aacute",193},{"Eacute",201},
        {"Iacute",205},{"Oacute",211},{"Uacute",218},{"Ntilde",209},
        {"Uuml",220},{"Agrave",192},{"Egrave",200},{"Igrave",204},
        {"Ograve",210},{"Ugrave",217},{"Acirc",194},{"Ecirc",202},
        {"Icirc",206},{"Ocirc",212},{"Ucirc",219},{"Auml",196},
        {"Euml",203},{"Iuml",207},{"Ouml",214},{"Ccedil",199}
    };
    std::string decoded;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') { decoded += text[i]; continue; }
        size_t semi = text.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 16) { decoded += '&'; continue; }
        std::string entity = text.substr(i + 1, semi - i - 1);
        unsigned cp = 0;
        bool matched = false;
        if (entity.size() > 1 && entity[0] == '#') {
            int base = 10;
            size_t start = 1;
            if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
                base = 16; start = 2;
            }
            if (start < entity.size()) {
                matched = true;
                for (size_t j = start; j < entity.size(); ++j) {
                    unsigned char ch = static_cast<unsigned char>(entity[j]);
                    unsigned digit = ch >= '0' && ch <= '9' ? ch - '0' :
                                     ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                                     ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : 99;
                    if (digit >= static_cast<unsigned>(base) ||
                        cp > (0x10ffffu - digit) / static_cast<unsigned>(base)) {
                        matched = false; break;
                    }
                    cp = cp * base + digit;
                }
            }
        } else {
            for (size_t j = 0; j < sizeof(named) / sizeof(named[0]); ++j) {
                if (entity == named[j].name) { cp = named[j].value; matched = true; break; }
            }
        }
        if (!matched || cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            decoded += '&'; continue;
        }
        appendUtf8(decoded, cp);
        i = semi;
    }
    return decoded;
}

std::wstring metadataText(const std::string &text, size_t limit) {
    std::wstring result;
    if (!utf8Wide(decodeEntities(trimAscii(text)), result)) return L"";
    result = trimWide(result);
    if (result.size() > limit) result.resize(limit);
    return result;
}

bool findTagEnd(const std::string &html, size_t start, size_t &end) {
    char quote = 0;
    for (size_t i = start; i < html.size() && i - start < 16384; ++i) {
        if (quote) { if (html[i] == quote) quote = 0; }
        else if (html[i] == '"' || html[i] == '\'') quote = html[i];
        else if (html[i] == '>') { end = i; return true; }
    }
    return false;
}

std::map<std::string, std::string> attributes(const std::string &tag, size_t start) {
    std::map<std::string, std::string> result;
    while (start < tag.size()) {
        while (start < tag.size() && (std::isspace(static_cast<unsigned char>(tag[start])) ||
                                      tag[start] == '/')) ++start;
        size_t keyStart = start;
        while (start < tag.size() && !std::isspace(static_cast<unsigned char>(tag[start])) &&
               tag[start] != '=' && tag[start] != '/') ++start;
        if (start == keyStart) { ++start; continue; }
        std::string key = asciiLower(tag.substr(keyStart, start - keyStart));
        while (start < tag.size() && std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
        std::string value;
        if (start < tag.size() && tag[start] == '=') {
            ++start;
            while (start < tag.size() && std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
            if (start < tag.size() && (tag[start] == '"' || tag[start] == '\'')) {
                char quote = tag[start++];
                size_t valueStart = start;
                while (start < tag.size() && tag[start] != quote) ++start;
                value = tag.substr(valueStart, start - valueStart);
                if (start < tag.size()) ++start;
            } else {
                size_t valueStart = start;
                while (start < tag.size() && !std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
                value = tag.substr(valueStart, start - valueStart);
                if (!value.empty() && value.back() == '/') value.pop_back();
            }
        }
        if (result.find(key) == result.end()) result[key] = value;
    }
    return result;
}

void keepFirst(std::map<std::string, std::string> &values, const std::string &key,
               const std::string &value) {
    if (!key.empty() && !trimAscii(value).empty() && values.find(key) == values.end())
        values[key] = value;
}

std::string firstValue(const std::map<std::string, std::string> &values,
                       const char *first, const char *second = "", const char *third = "") {
    const char *keys[] = {first, second, third};
    for (size_t i = 0; i < 3; ++i) {
        std::map<std::string, std::string>::const_iterator item = values.find(keys[i]);
        if (item != values.end() && !trimAscii(item->second).empty()) return item->second;
    }
    return "";
}

} // namespace

bool parseLinkMetadata(const std::string &html, const std::wstring &url, LinkMetadata &out) {
    out = LinkMetadata();
    out.url = url;
    if (html.size() > MAX_HTML_BYTES || html.empty()) return false;
    std::map<std::string, std::string> values;
    std::string titleElement;
    size_t pos = 0;
    while ((pos = html.find('<', pos)) != std::string::npos) {
        if (html.compare(pos, 4, "<!--") == 0) {
            size_t close = html.find("-->", pos + 4);
            if (close == std::string::npos) break;
            pos = close + 3;
            continue;
        }
        size_t end = 0;
        if (!findTagEnd(html, pos + 1, end)) { ++pos; continue; }
        size_t start = pos + 1;
        while (start < end && std::isspace(static_cast<unsigned char>(html[start]))) ++start;
        if (start >= end || html[start] == '/' || html[start] == '!' || html[start] == '?') {
            pos = end + 1; continue;
        }
        size_t nameEnd = start;
        while (nameEnd < end && std::isalpha(static_cast<unsigned char>(html[nameEnd]))) ++nameEnd;
        std::string tag = asciiLower(html.substr(start, nameEnd - start));
        if (tag == "meta") {
            std::map<std::string, std::string> attrs =
                attributes(html.substr(nameEnd, end - nameEnd), 0);
            std::string key = asciiLower(trimAscii(attrs.count("property") ?
                attrs["property"] : attrs["name"]));
            if (key.empty() && attrs.count("name")) key = asciiLower(trimAscii(attrs["name"]));
            if (attrs.count("content")) keepFirst(values, key, attrs["content"]);
        } else if (tag == "title" || tag == "script" || tag == "style") {
            std::string tail = asciiLower(html.substr(end + 1));
            size_t close = tail.find("</" + tag);
            if (close == std::string::npos) { pos = end + 1; continue; }
            if (tag == "title" && titleElement.empty())
                titleElement = html.substr(end + 1, close);
            size_t closeEnd = html.find('>', end + 1 + close);
            pos = closeEnd == std::string::npos ? html.size() : closeEnd + 1;
            continue;
        }
        pos = end + 1;
    }
    out.title = metadataText(firstValue(values, "og:title", "twitter:title"), 300);
    if (out.title.empty()) out.title = metadataText(titleElement, 300);
    out.description = metadataText(
        firstValue(values, "og:description", "twitter:description", "description"), 1000);
    out.siteName = metadataText(firstValue(values, "og:site_name"), 200);
    std::string imageValue = firstValue(values, "og:image:secure_url",
                                        "og:image", "og:image:url");
    if (imageValue.empty())
        imageValue = firstValue(values, "twitter:image", "twitter:image:src");
    std::wstring image = metadataText(imageValue, linkUrl::MaxUrlLength);
    out.imageUrl = linkUrl::resolve(url, image);
    out.success = !out.title.empty() || !out.description.empty() ||
                  !out.siteName.empty() || !out.imageUrl.empty();
    return out.success;
}

