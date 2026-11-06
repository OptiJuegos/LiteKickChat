#include "kickchat_emote_render.hpp"
#include "kickchat_emotes.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_link_preview.hpp"
#include <propidl.h>
#include <gdiplus.h>

static const int LINE_HEIGHT = 27;
static const int EMOTE_SIZE = 24;

static int tokenWidth(HDC dc, const std::wstring &text) {
    if (text.empty()) return 0;
    SIZE size = {0, 0};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return static_cast<int>(size.cx);
}

static void renderTextRun(HDC dc, const std::wstring &text, size_t offset,
                          int &x, int &y, int left, int right, bool draw,
                          const std::wstring &url, const RECT *viewport,
                          std::vector<TextLinkHit> *links,
                          std::vector<ChatTextGlyph> *glyphs) {
    if (text.empty()) return;
    const bool isLink = !url.empty();
    const COLORREF original = GetTextColor(dc);
    const COLORREF linkColor = RGB(110, 190, 255);
    if (draw && isLink) SetTextColor(dc, linkColor);
    size_t pos = 0;
    while (pos < text.size()) {
        int fit = 0;
        SIZE measured = {};
        GetTextExtentExPointW(dc, text.c_str() + pos,
                              static_cast<int>(text.size() - pos), std::max(0, right - x),
                              &fit, NULL, &measured);
        if (fit == 0 && x > left) {
            x = left;
            y += LINE_HEIGHT;
            continue;
        }
        if (fit < 1) fit = 1;
        if (pos + fit < text.size() && text[pos + fit - 1] >= 0xd800 &&
            text[pos + fit - 1] <= 0xdbff && text[pos + fit] >= 0xdc00 &&
            text[pos + fit] <= 0xdfff) ++fit;
        const std::wstring segment = text.substr(pos, fit);
        const int width = tokenWidth(dc, segment);
        appendChatTextGlyphs(dc, segment, x, y, LINE_HEIGHT, offset + pos,
                             viewport, glyphs);
        if (draw) {
            TextOutW(dc, x, y, segment.c_str(), static_cast<int>(segment.size()));
            if (isLink) {
                HPEN old = static_cast<HPEN>(SelectObject(dc, GetStockObject(DC_PEN)));
                SetDCPenColor(dc, linkColor);
                MoveToEx(dc, x, y + LINE_HEIGHT - 3, NULL);
                LineTo(dc, x + width, y + LINE_HEIGHT - 3);
                SelectObject(dc, old);
                if (links && viewport) {
                    RECT area = {x, y, x + width, y + LINE_HEIGHT};
                    RECT visible = {};
                    if (IntersectRect(&visible, &area, viewport)) {
                        TextLinkHit hit = {visible, url};
                        links->push_back(hit);
                    }
                }
            }
        }
        x += width;
        pos += fit;
        if (pos < text.size()) {
            x = left;
            y += LINE_HEIGHT;
        }
    }
    if (draw && isLink) SetTextColor(dc, original);
}

static void drawTextWithLinks(HDC dc, const std::wstring &piece, int &x, int &y,
                              int left, int right, bool draw, const RECT *viewport,
                              std::vector<TextLinkHit> *links,
                              std::vector<ChatTextGlyph> *glyphs, size_t offset) {
    const std::string content = utf8FromWide(piece);
    LinkSpan link;
    size_t from = 0;
    size_t pieceOffset = offset;
    while (findNextLink(content, from, link)) {
        const std::wstring prefix = toWide(content.substr(from, link.start - from));
        renderTextRun(dc, prefix, pieceOffset, x, y, left, right, draw,
                      L"", viewport, links, glyphs);
        pieceOffset += prefix.size();
        const std::wstring address = toWide(content.substr(link.start, link.length));
        renderTextRun(dc, address, pieceOffset, x, y, left, right, draw,
                      toWide(link.preview.url), viewport, links, glyphs);
        pieceOffset += address.size();
        from = link.start + link.length;
    }
    renderTextRun(dc, toWide(content.substr(from)), pieceOffset, x, y, left, right,
                  draw, L"", viewport, links, glyphs);
}

int renderChatContent(HDC dc, const std::string &content, int left, int top,
                      int right, bool draw, const RECT *viewport,
                      std::vector<TextLinkHit> *links,
                      std::vector<ChatTextGlyph> *glyphs) {
    const std::vector<EmoteToken> tokens = splitEmoteMessage(content, currentEmoteCatalog());
    int x = left;
    int y = top;
    int maxRight = std::max(left + 1, right);
    size_t visibleOffset = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const EmoteToken &token = tokens[i];
        const std::wstring text = toWide(token.text);
        if (!token.imageKey.empty()) {
            Gdiplus::Bitmap *image = cachedEmoteImage(token.imageKey);
            int imageWidth = EMOTE_SIZE;
            if (image && image->GetHeight())
                imageWidth = std::min(64, std::max(EMOTE_SIZE,
                    static_cast<int>(image->GetWidth() * EMOTE_SIZE / image->GetHeight())));
            if (x + imageWidth > maxRight && x > left) { x = left; y += LINE_HEIGHT; }
            if (glyphs && !text.empty()) {
                RECT area = {x, y, x + imageWidth, y + EMOTE_SIZE};
                RECT visible = {};
                if (!viewport || IntersectRect(&visible, &area, viewport)) {
                    ChatTextGlyph glyph = {viewport ? visible : area, visibleOffset, text.size()};
                    glyphs->push_back(glyph);
                }
            }
            if (draw) {
                markVisibleEmote(token.imageKey);
                if (!image) image = findEmoteImage(token.imageKey, token.imageUrl);
                if (image) {
                    Gdiplus::Graphics graphics(dc);
                    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    graphics.DrawImage(image, x, y, imageWidth, EMOTE_SIZE);
                } else {
                    RECT placeholder = {x, y + 1, x + imageWidth, y + EMOTE_SIZE};
                    const std::wstring label = toWide(token.text);
                    DrawTextW(dc, label.c_str(), -1, &placeholder,
                              DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                }
            }
            x += imageWidth + 2;
            visibleOffset += text.size();
            continue;
        }
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find_first_of(L"\r\n", start);
            bool newline = end != std::wstring::npos;
            if (!newline) end = text.size();
            std::wstring piece = text.substr(start, end - start);
            const int width = tokenWidth(dc, piece);
            if (x > left && x + width > maxRight &&
                piece.find_first_not_of(L" \t\r") != std::wstring::npos) {
                x = left;
                y += LINE_HEIGHT;
            }
            if (!piece.empty()) {
                drawTextWithLinks(dc, piece, x, y, left, maxRight, draw, viewport, links,
                                  glyphs, visibleOffset + start);
            }
            if (newline) {
                x = left;
                y += LINE_HEIGHT;
                start = end + 1;
                if (text[end] == L'\r' && start < text.size() && text[start] == L'\n')
                    ++start;
            } else start = end;
        }
        visibleOffset += text.size();
    }
    return y - top + LINE_HEIGHT;
}
