#include "kickchat_chat_paint.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_badges.hpp"
#include "kickchat_emotes.hpp"
#include "kickchat_emote_render.hpp"
#include "kickchat_link_paint.hpp"
#include "kickchat_scroll.hpp"

static std::vector<TextLinkHit> g_linkHits;
struct UserHit { RECT area; ChatRecord record; };
struct RowHit { RECT area; int index; };
struct TextRowHit { RECT area; int index; std::vector<ChatTextGlyph> glyphs; };
static std::vector<UserHit> g_userHits;
static std::vector<RowHit> g_rowHits;
static std::vector<TextRowHit> g_textRows;

std::wstring chatMessageText(const ChatLine &line) {
    const std::string raw = line.record.content.empty() ?
        utf8FromWide(line.content) : line.record.content;
    const std::vector<EmoteToken> tokens = splitEmoteMessage(raw, currentEmoteCatalog());
    std::wstring visible;
    for (size_t i = 0; i < tokens.size(); ++i) visible += toWide(tokens[i].text);
    return visible;
}

bool chatTextAt(int x, int y, ChatTextPosition &position, bool clamp) {
    if (g_textRows.empty()) return false;
    const TextRowHit *row = NULL;
    for (size_t i = 0; i < g_textRows.size(); ++i) {
        if (y >= g_textRows[i].area.top && y < g_textRows[i].area.bottom) {
            row = &g_textRows[i];
            break;
        }
    }
    if (!row && clamp) {
        int distance = INT_MAX;
        for (size_t i = 0; i < g_textRows.size(); ++i) {
            const RECT &area = g_textRows[i].area;
            const int current = y < area.top ? area.top - y :
                                y >= area.bottom ? y - area.bottom + 1 : 0;
            if (current < distance) { distance = current; row = &g_textRows[i]; }
        }
    }
    if (!row || row->glyphs.empty()) return false;
    if (!clamp && (y < row->area.top || y >= row->area.bottom ||
                   x < row->area.left || x >= row->area.right)) return false;

    const ChatTextGlyph *closest = NULL;
    long best = LONG_MAX;
    for (size_t i = 0; i < row->glyphs.size(); ++i) {
        const ChatTextGlyph &glyph = row->glyphs[i];
        const int dx = x < glyph.area.left ? glyph.area.left - x :
                       x > glyph.area.right ? x - glyph.area.right : 0;
        const int dy = y < glyph.area.top ? glyph.area.top - y :
                       y >= glyph.area.bottom ? y - glyph.area.bottom + 1 : 0;
        const long distance = static_cast<long>(dy) * 4096 + dx;
        if (!closest || distance < best) { closest = &glyph; best = distance; }
    }
    if (!closest) return false;
    position.row = row->index;
    position.offset = closest->offset +
        (x >= (closest->area.left + closest->area.right) / 2 ? closest->length : 0);
    return true;
}

static void paintTextHighlight(HDC dc, const std::vector<ChatTextGlyph> &glyphs,
                               int row, const ChatTextSelection &selection,
                               const RECT &viewport) {
    if (!hasSelectedText(selection)) return;
    ChatTextPosition first = selection.anchor, last = selection.focus;
    if (first.row > last.row || (first.row == last.row && first.offset > last.offset))
        std::swap(first, last);
    if (row < first.row || row > last.row) return;
    const size_t start = row == first.row ? first.offset : 0;
    const size_t end = row == last.row ? last.offset : static_cast<size_t>(-1);
    HBRUSH color = CreateSolidBrush(RGB(57, 92, 126));
    for (size_t i = 0; i < glyphs.size(); ++i) {
        const ChatTextGlyph &glyph = glyphs[i];
        if (glyph.offset < end && glyph.offset + glyph.length > start) {
            RECT visible = {};
            if (IntersectRect(&visible, &glyph.area, &viewport)) FillRect(dc, &visible, color);
        }
    }
    DeleteObject(color);
}

int chatRowAt(int x, int y) {
    for (size_t i = 0; i < g_rowHits.size(); ++i) {
        const RECT &r = g_rowHits[i].area;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return g_rowHits[i].index;
    }
    return -1;
}

bool chatUserAt(int x, int y, ChatRecord &record) {
    for (size_t i = 0; i < g_userHits.size(); ++i) {
        const RECT &r = g_userHits[i].area;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
            record = g_userHits[i].record;
            return true;
        }
    }
    return false;
}

static const std::wstring *chatLinkAt(int x, int y) {
    for (size_t i = 0; i < g_linkHits.size(); ++i) {
        const RECT &area = g_linkHits[i].area;
        if (x >= area.left && x < area.right && y >= area.top && y < area.bottom)
            return &g_linkHits[i].url;
    }
    return NULL;
}

bool hasChatLinkAt(int x, int y) {
    return chatLinkAt(x, y) != NULL;
}

bool chatLinkUrlAt(int x, int y, std::wstring &url) {
    const std::wstring *found = chatLinkAt(x, y);
    if (!found) return false;
    url = *found;
    return true;
}

bool openChatLink(HWND owner, int x, int y) {
    const std::wstring *url = chatLinkAt(x, y);
    if (url) {
        HINSTANCE result = ShellExecuteW(owner, L"open", url->c_str(),
                                         NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32)
            MessageBoxW(owner, kickchatText::get(kickchatText::OpenLinkFailed),
                        L"KickChat", MB_OK | MB_ICONERROR);
        return true;
    }
    return false;
}

static bool hasInlineEmote(const ChatLine &line) {
    if (line.record.content.empty()) return false;
    const std::vector<EmoteToken> tokens = splitEmoteMessage(line.record.content, currentEmoteCatalog());
    for (size_t i = 0; i < tokens.size(); ++i)
        if (!tokens[i].imageKey.empty()) return true;
    return false;
}

static int contentHeight(HDC dc, const ChatLine &line, int width, HFONT font) {
    SelectObject(dc, font);
    if (hasInlineEmote(line))
        return renderChatContent(dc, line.record.content, 0, 0, width, false);
    const std::string content = line.record.content.empty() ?
        utf8FromWide(line.content) : line.record.content;
    const RECT viewport = {0, 0, width, 32767};
    return renderLinkedText(dc, content, 0, 0, width, false, viewport, NULL);
}

void paintChat(HWND hwnd, HFONT font, HFONT bold,
               const std::wstring &statusText, const std::wstring &authText,
               const std::deque<ChatLine> &lines, int &scroll,
               const std::vector<std::string> &channels, int selected,
               int &maxScroll, int scrollbarWidth, const ChatTextSelection &selection) {
    clearVisibleEmotes();
    g_linkHits.clear();
    g_userHits.clear();
    g_rowHits.clear();
    g_textRows.clear();
    PAINTSTRUCT ps;HDC screen=BeginPaint(hwnd,&ps);
    RECT rc;GetClientRect(hwnd,&rc);
    int w=rc.right,h=rc.bottom;
    if (w<1||h<1) {EndPaint(hwnd,&ps);return;}
    HDC dc=CreateCompatibleDC(screen);
    HBITMAP bm=CreateCompatibleBitmap(screen,w,h);
    HGDIOBJ oldbmp=SelectObject(dc,bm);
    HBRUSH back=CreateSolidBrush(RGB(18,20,23)),top=CreateSolidBrush(RGB(28,30,34));
    FillRect(dc,&rc,back);RECT header={0,0,w,89};FillRect(dc,&header,top);
    SetBkMode(dc,TRANSPARENT);
    SelectObject(dc,bold);SetTextColor(dc,RGB(90,226,133));
    const int left = 145;
    RECT sidebar = {0, 0, left, h};
    HBRUSH sidebarBrush = CreateSolidBrush(RGB(23, 26, 31));
    FillRect(dc, &sidebar, sidebarBrush);
    for (size_t i = 0; i < channels.size() && i < 8; ++i) {
        int y = 10 + static_cast<int>(i) * 42;
        RECT tab = {7, y + 2, left - 6, y + 40};
        HBRUSH tabBrush = CreateSolidBrush(static_cast<int>(i) == selected ?
                                             RGB(43, 54, 49) : RGB(28, 33, 39));
        FillRect(dc, &tab, tabBrush);
        DeleteObject(tabBrush);
        SetTextColor(dc, static_cast<int>(i) == selected ? RGB(115, 232, 151) : RGB(204, 209, 215));
        std::wstring name = toWide(channels[i]);
        RECT tabName = {14, y + 5, left - 34, y + 38};
        DrawTextW(dc, name.c_str(), -1, &tabName, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        RECT close = {left - 28, y + 5, left - 9, y + 37};
        SetTextColor(dc, RGB(158, 163, 169));
        DrawTextW(dc, L"x", -1, &close, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    SelectObject(dc, bold);
    SetTextColor(dc, RGB(90, 226, 133));
    std::wstring title = selected >= 0 && selected < static_cast<int>(channels.size()) ?
        L"KICK   /   " + toWide(channels[selected]) : L"KICK   /   Agrega un canal con +";
    RECT channel = {left + 14, 10, w - 12, 38};
    DrawTextW(dc, title.c_str(), -1, &channel, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    HBRUSH bar=CreateSolidBrush(RGB(28,30,34));RECT bottom={left,h-106,w,h};FillRect(dc,&bottom,bar);
    SelectObject(dc,font);SetTextColor(dc,RGB(175,183,194));
    RECT status={left+12,h-53,w-12,h-32};DrawTextW(dc,statusText.c_str(),-1,&status,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    RECT auth={left+12,h-31,w-12,h-4};SetTextColor(dc,RGB(104,204,144));
    DrawTextW(dc,authText.c_str(),-1,&auth,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    int viewportTop=95, viewportBottom=std::max(52,h-108), available=viewportBottom-viewportTop;
    int textwidth=std::max(30,w-left-28-scrollbarWidth);
    std::vector<int> heights;heights.reserve(lines.size());
    int total=0;
    for (size_t i=0;i<lines.size();++i) {
        int rowheight = 21 + contentHeight(dc, lines[i], textwidth, font) + 9;
        heights.push_back(rowheight);total+=rowheight;
    }
    maxScroll = chatMaxScroll(total, available);
    scroll = std::max(0, std::min(scroll, maxScroll));
    if (selected < 0) {
        SelectObject(dc, font);
        SetTextColor(dc, RGB(164, 172, 183));
        RECT empty = {left + 20, viewportTop + 18, w - 14, viewportTop + 110};
        DrawTextW(dc, L"Presiona + Agregar canal para comenzar.", -1, &empty,
                  DT_WORDBREAK | DT_NOPREFIX);
    }
    int y=viewportBottom-total+scroll;
    HRGN clip=CreateRectRgn(left,viewportTop,w-scrollbarWidth,viewportBottom);
    SelectClipRgn(dc,clip);
    for (size_t i=0;i<lines.size();++i) {
        const ChatLine &line=lines[i];int rowH=heights[i];
        if (y+rowH>=viewportTop && y<viewportBottom) {
            RECT row = {left, y, w-scrollbarWidth, y+rowH};
            RECT viewport = {left, viewportTop, w-scrollbarWidth, viewportBottom};
            RECT visible = {};
            if (IntersectRect(&visible, &row, &viewport)) {
                RowHit hit = {visible, static_cast<int>(i)};
                g_rowHits.push_back(hit);
            }
            SelectObject(dc,bold);SetTextColor(dc,line.color);
            SIZE nameSize={0,0};
            GetTextExtentPoint32W(dc,line.user.c_str(),(int)line.user.size(),&nameSize);
            SelectObject(dc,font);
            int chipSpace = badgeRowWidth(dc,line.record.badges);
            int nameWidth = std::max(56,std::min(static_cast<int>(nameSize.cx),textwidth-chipSpace-12));
            SelectObject(dc,bold);SetTextColor(dc,line.color);
            RECT usr={left+14,y,left+14+nameWidth,y+21};
            DrawTextW(dc,line.user.c_str(),-1,&usr,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            if (!line.user.empty() && (line.userId || !line.record.id.empty())) {
                RECT userArea = {};
                if (IntersectRect(&userArea, &usr, &viewport)) {
                    UserHit hit = {userArea, line.record};
                    if (hit.record.id.empty()) {
                        char number[32];
                        sprintf(number, "%llu", line.userId);
                        hit.record.id = number;
                    }
                    if (hit.record.user.empty()) hit.record.user = utf8FromWide(line.user);
                    if (hit.record.channel.empty()) hit.record.channel = line.channel;
                    g_userHits.push_back(hit);
                }
            }
            SelectObject(dc,font);
            drawBadgeRow(dc,line.record.badges,usr.right+7,y,w-12-scrollbarWidth);
            SetTextColor(dc,RGB(236,237,240));
            RECT content={left+14,y+21,w-14-scrollbarWidth,y+rowH};
            const RECT contentViewport = {content.left, viewportTop, content.right, viewportBottom};
            RECT textArea = {};
            const bool contentVisible = IntersectRect(&textArea, &content, &contentViewport) != 0;
            TextRowHit textHit = {textArea, static_cast<int>(i), std::vector<ChatTextGlyph>()};
            if (hasInlineEmote(line)) {
                const RECT visible = {content.left, viewportTop, content.right, viewportBottom};
                renderChatContent(dc, line.record.content, content.left, content.top,
                                  content.right, false, &visible, NULL, &textHit.glyphs);
                paintTextHighlight(dc, textHit.glyphs, static_cast<int>(i), selection,
                                   contentViewport);
                renderChatContent(dc, line.record.content, content.left, content.top,
                                  content.right, true, &visible, &g_linkHits);
            } else {
                const std::string raw = line.record.content.empty() ?
                    utf8FromWide(line.content) : line.record.content;
                const RECT visible = {content.left, viewportTop, content.right, viewportBottom};
                renderLinkedText(dc, raw, content.left, content.top, content.right,
                                 false, visible, NULL, &textHit.glyphs);
                paintTextHighlight(dc, textHit.glyphs, static_cast<int>(i), selection,
                                   contentViewport);
                renderLinkedText(dc, raw, content.left, content.top, content.right,
                                 true, visible, &g_linkHits);
            }
            if (contentVisible && !textHit.glyphs.empty()) g_textRows.push_back(textHit);
        }
        y+=rowH;
    }
    SelectClipRgn(dc,NULL);DeleteObject(clip);
    const RECT &dirty = ps.rcPaint;
    BitBlt(screen, dirty.left, dirty.top, dirty.right - dirty.left,
           dirty.bottom - dirty.top, dc, dirty.left, dirty.top, SRCCOPY);
    SelectObject(dc,oldbmp);DeleteObject(bm);DeleteDC(dc);
    DeleteObject(back);DeleteObject(top);DeleteObject(bar);
    DeleteObject(sidebarBrush);
    EndPaint(hwnd,&ps);
}
