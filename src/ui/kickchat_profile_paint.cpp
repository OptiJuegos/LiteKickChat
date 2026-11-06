#include "kickchat_profile_paint.hpp"
#include "kickchat_profile_strings.hpp"
#include "kickchat_badges.hpp"
#include "kickchat_platform.hpp"
#include <algorithm>
#include <cwchar>

namespace {

void drawText(HDC dc, const std::wstring &text, RECT rect, COLORREF color, UINT flags) {
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, flags | DT_NOPREFIX);
}

int messageHeight(HDC dc, const ChatRecord &record, int width) {
    RECT rect = {0, 0, width, 0};
    const std::wstring content = toWide(record.content);
    DrawTextW(dc, content.c_str(), -1, &rect, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    return 27 + std::max(20L, rect.bottom) + 12;
}

} // namespace

void paintUserProfile(HWND window, const ChatRecord &person,
                      const std::vector<ChatRecord> &messages,
                      const std::wstring &status, HFONT font, HFONT bold,
                      int &scrollOffset) {
    PAINTSTRUCT paint = {};
    HDC screen = BeginPaint(window, &paint);
    RECT client = {};
    GetClientRect(window, &client);
    const int width = client.right;
    const int height = client.bottom;
    if (width <= 0 || height <= 0) {
        EndPaint(window, &paint);
        return;
    }

    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    HBRUSH background = CreateSolidBrush(RGB(23, 26, 30));
    HBRUSH banner = CreateSolidBrush(RGB(46, 35, 67));
    HBRUSH panel = CreateSolidBrush(RGB(31, 35, 41));
    FillRect(dc, &client, background);
    RECT header = {0, 0, width, 84};
    FillRect(dc, &header, banner);
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, bold);
    const std::wstring name = toWide(person.user);
    RECT username = {22, 18, width - 18, 53};
    drawText(dc, name, username, RGB(245, 246, 250), DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, font);
    RECT id = {22, 53, width - 18, 75};
    drawText(dc, std::wstring(kickchatProfileText::get(kickchatProfileText::KickIdPrefix)) +
             (person.id.empty() ? kickchatProfileText::get(kickchatProfileText::Unverified) :
              toWide(person.id)),
             id, RGB(205, 203, 217), DT_SINGLELINE);
    drawBadgeRow(dc, person.badges, 20, 95, width - 12);
    RECT role = {20, 125, width - 20, 148};
    if (person.badges.empty())
        drawText(dc, kickchatProfileText::get(kickchatProfileText::MissingBadges),
                 role, RGB(156, 167, 177), DT_SINGLELINE);
    RECT timestamp = {20, 152, width - 18, 174};
    drawText(dc, std::wstring(kickchatProfileText::get(kickchatProfileText::LastReceived)) +
             (person.receivedAt.empty() ? kickchatProfileText::get(kickchatProfileText::NoLocalMessages) :
              toWide(person.receivedAt)),
             timestamp, RGB(196, 201, 208), DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT availability = {20, 179, width - 18, 199};
    drawText(dc, status.empty() ?
             kickchatProfileText::get(kickchatProfileText::PublicProfileAvailable) : status,
             availability, RGB(151, 158, 169), DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT divider = {12, 257, width - 12, 259};
    FillRect(dc, &divider, panel);
    RECT subtitle = {20, 267, width - 20, 292};
    wchar_t count[96];
    swprintf(count, 96, kickchatProfileText::get(kickchatProfileText::RecentMessagesFormat),
             static_cast<unsigned>(messages.size()));
    SelectObject(dc, bold);
    drawText(dc, count, subtitle, RGB(237, 239, 243), DT_SINGLELINE);
    SelectObject(dc, font);

    const int top = 302;
    const int bottom = height - 12;
    int total = 0;
    for (size_t i = 0; i < messages.size(); ++i)
        total += messageHeight(dc, messages[i], width - 44);
    scrollOffset = std::min(scrollOffset, std::max(0, total - (bottom - top)));
    int y = top - scrollOffset;
    HRGN clip = CreateRectRgn(0, top, width, bottom);
    SelectClipRgn(dc, clip);
    if (messages.empty()) {
        RECT empty = {20, top + 12, width - 20, top + 65};
        drawText(dc, kickchatProfileText::get(kickchatProfileText::EmptyMessages),
                 empty, RGB(174, 183, 194), DT_WORDBREAK);
    }
    for (size_t i = 0; i < messages.size(); ++i) {
        const ChatRecord &message = messages[i];
        const int rowHeight = messageHeight(dc, message, width - 44);
        if (y < bottom && y + rowHeight > top) {
            RECT timeRect = {20, y + 4, width - 20, y + 26};
            drawText(dc, toWide(message.receivedAt), timeRect, RGB(138, 204, 166), DT_SINGLELINE);
            RECT messageRect = {20, y + 27, width - 20, y + rowHeight - 4};
            drawText(dc, toWide(message.content), messageRect, RGB(230, 232, 235), DT_WORDBREAK);
            RECT separator = {20, y + rowHeight - 2, width - 20, y + rowHeight - 1};
            FillRect(dc, &separator, panel);
        }
        y += rowHeight;
    }
    SelectClipRgn(dc, NULL);
    DeleteObject(clip);

    BitBlt(screen, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    DeleteObject(background);
    DeleteObject(banner);
    DeleteObject(panel);
    EndPaint(window, &paint);
}
