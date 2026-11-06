#include "kickchat_link_paint.hpp"
#include "kickchat_link_preview.hpp"

static int measureWidth(HDC dc, const std::wstring &text) {
    SIZE size = {0, 0};
    if (!text.empty())
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return static_cast<int>(size.cx);
}

void appendChatTextGlyphs(HDC dc, const std::wstring &text, int x, int y,
                          int height, size_t offset, const RECT *viewport,
                          std::vector<ChatTextGlyph> *glyphs) {
    if (!glyphs || text.empty()) return;
    std::vector<int> widths(text.size());
    SIZE size = {};
    if (!GetTextExtentExPointW(dc, text.c_str(), static_cast<int>(text.size()),
                               0x7fffffff, NULL, &widths[0], &size)) return;
    for (size_t i = 0; i < text.size();) {
        size_t length = 1;
        if (text[i] >= 0xd800 && text[i] <= 0xdbff && i + 1 < text.size() &&
            text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) length = 2;
        RECT area = {x + (i ? widths[i - 1] : 0), y,
                     x + widths[i + length - 1], y + height};
        RECT visible = {};
        if (area.right > area.left && area.bottom > area.top &&
            (!viewport || IntersectRect(&visible, &area, viewport))) {
            ChatTextGlyph glyph = {viewport ? visible : area, offset + i, length};
            glyphs->push_back(glyph);
        }
        i += length;
    }
}

int renderLinkedText(HDC dc, const std::string &content, int left, int top,
                     int right, bool draw, const RECT &viewport,
                     std::vector<TextLinkHit> *hits,
                     std::vector<ChatTextGlyph> *glyphs) {
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    const int lineHeight = std::max(19L, metrics.tmHeight);
    const int maxRight = std::max(left + 1, right);
    const COLORREF normal = GetTextColor(dc);
    const COLORREF linkColor = RGB(110, 190, 255);
    int x = left, y = top;
    size_t from = 0;
    size_t textOffset = 0;

    while (from < content.size()) {
        LinkSpan link;
        const bool found = findNextLink(content, from, link);
        for (int part = 0; part < (found ? 2 : 1); ++part) {
            const bool isLink = found && part == 1;
            const std::string bytes = isLink ? content.substr(link.start, link.length) :
                content.substr(from, (found ? link.start : content.size()) - from);
            const std::wstring text = toWide(bytes);
            const size_t partOffset = textOffset;
            textOffset += text.size();
            size_t pos = 0;
            while (pos < text.size()) {
                if (text[pos] == L'\r') { ++pos; continue; }
                if (text[pos] == L'\n') { x = left; y += lineHeight; ++pos; continue; }
                const bool space = text[pos] == L' ' || text[pos] == L'\t';
                size_t end = pos + 1;
                while (end < text.size() && text[end] != L'\n' && text[end] != L'\r' &&
                       (text[end] == L' ' || text[end] == L'\t') == space) ++end;
                std::wstring token = text.substr(pos, end - pos);
                if (space) {
                    if (x != left) {
                        const int width = measureWidth(dc, token);
                        if (x + width > maxRight) { x = left; y += lineHeight; }
                        else {
                            appendChatTextGlyphs(dc, token, x, y, lineHeight,
                                                 partOffset + pos, &viewport, glyphs);
                            x += width;
                        }
                    }
                    pos = end;
                    continue;
                }
                if (x > left && x + measureWidth(dc, token) > maxRight) {
                    x = left;
                    y += lineHeight;
                }
                size_t segment = 0;
                while (segment < token.size()) {
                    const int remaining = maxRight - x;
                    int count = 0;
                    SIZE measured = {};
                    GetTextExtentExPointW(dc, token.c_str() + segment,
                        static_cast<int>(token.size() - segment), remaining,
                        &count, NULL, &measured);
                    if (count < 1 && x > left) {
                        x = left;
                        y += lineHeight;
                        continue;
                    }
                    if (count < 1) count = 1;
                    if (segment + count < token.size() &&
                        token[segment + count - 1] >= 0xd800 &&
                        token[segment + count - 1] <= 0xdbff &&
                        token[segment + count] >= 0xdc00 &&
                        token[segment + count] <= 0xdfff) ++count;
                    const std::wstring piece = token.substr(segment, count);
                    const int width = measureWidth(dc, piece);
                    appendChatTextGlyphs(dc, piece, x, y, lineHeight,
                                         partOffset + pos + segment, &viewport, glyphs);
                    if (draw) {
                        SetTextColor(dc, isLink ? linkColor : normal);
                        TextOutW(dc, x, y, piece.c_str(), static_cast<int>(piece.size()));
                        if (isLink) {
                            HPEN old = static_cast<HPEN>(SelectObject(dc, GetStockObject(DC_PEN)));
                            SetDCPenColor(dc, linkColor);
                            MoveToEx(dc, x, y + lineHeight - 2, NULL);
                            LineTo(dc, x + width, y + lineHeight - 2);
                            SelectObject(dc, old);
                            if (hits) {
                                RECT area = {x, y, x + width, y + lineHeight};
                                RECT visible = {};
                                if (IntersectRect(&visible, &area, &viewport)) {
                                    TextLinkHit hit = {visible, toWide(link.preview.url)};
                                    hits->push_back(hit);
                                }
                            }
                        }
                    }
                    x += width;
                    segment += count;
                    if (segment < token.size()) { x = left; y += lineHeight; }
                }
                pos = end;
            }
        }
        if (!found) break;
        from = link.start + link.length;
    }
    if (draw) SetTextColor(dc, normal);
    return y - top + lineHeight;
}
