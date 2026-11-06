#include "kickchat_badges.hpp"

static const int BADGE_ICON_WIDTH = 18;
static const int BADGE_HEIGHT = 17;
static const int BADGE_SPACING = 4;

static COLORREF badgeColor(const std::string &type) {
    if (type == "moderator") return RGB(30, 176, 217);
    if (type == "broadcaster" || type == "host") return RGB(143, 83, 204);
    if (type == "subscriber") return RGB(57, 169, 71);
    if (type == "vip") return RGB(221, 167, 31);
    if (type == "og") return RGB(29, 174, 211);
    if (type == "sub_gifter") return RGB(34, 126, 158);
    if (type == "verified") return RGB(47, 178, 72);
    return RGB(74, 85, 99);
}

static bool hasBadgeCount(const ChatBadge &badge) {
    return badge.count && (badge.type == "subscriber" || badge.type == "sub_gifter");
}

static int badgeWidth(HDC dc, const ChatBadge &badge) {
    if (!hasBadgeCount(badge)) return BADGE_ICON_WIDTH;
    const std::wstring count = std::to_wstring(badge.count);
    SIZE size = {0, 0};
    GetTextExtentPoint32W(dc, count.c_str(), static_cast<int>(count.size()), &size);
    return BADGE_ICON_WIDTH + size.cx + 5;
}

static void drawBadgeIcon(HDC dc, const std::string &type, int x, int top,
                          COLORREF background) {
    if (type == "moderator") {
        POINT blade[] = {{x + 8, top + 9}, {x + 12, top + 3}, {x + 15, top + 2},
                         {x + 14, top + 5}, {x + 10, top + 10}};
        Polygon(dc, blade, 5);
        POINT guard[] = {{x + 5, top + 7}, {x + 7, top + 7}, {x + 11, top + 11},
                         {x + 11, top + 13}, {x + 9, top + 13}, {x + 5, top + 9}};
        Polygon(dc, guard, 6);
        POINT handle[] = {{x + 5, top + 10}, {x + 8, top + 13}, {x + 6, top + 15},
                          {x + 3, top + 12}};
        Polygon(dc, handle, 4);
    } else if (type == "broadcaster" || type == "host") {
        RoundRect(dc, x + 7, top + 2, x + 12, top + 10, 4, 4);
        MoveToEx(dc, x + 5, top + 7, NULL);
        LineTo(dc, x + 5, top + 10);
        LineTo(dc, x + 7, top + 12);
        LineTo(dc, x + 12, top + 12);
        LineTo(dc, x + 14, top + 10);
        LineTo(dc, x + 14, top + 7);
        MoveToEx(dc, x + 9, top + 12, NULL);
        LineTo(dc, x + 9, top + 15);
        MoveToEx(dc, x + 6, top + 15, NULL);
        LineTo(dc, x + 13, top + 15);
    } else if (type == "subscriber") {
        POINT star[] = {{x + 9, top + 2}, {x + 11, top + 6}, {x + 15, top + 7},
                        {x + 12, top + 10}, {x + 13, top + 14}, {x + 9, top + 12},
                        {x + 5, top + 14}, {x + 6, top + 10}, {x + 3, top + 7},
                        {x + 7, top + 6}};
        Polygon(dc, star, 10);
    } else if (type == "vip") {
        POINT crown[] = {{x + 3, top + 5}, {x + 6, top + 8}, {x + 9, top + 3},
                         {x + 12, top + 8}, {x + 15, top + 5},
                         {x + 13, top + 13}, {x + 5, top + 13}};
        Polygon(dc, crown, 7);
    } else if (type == "og") {
        HFONT font = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
        HGDIOBJ previous = font ? SelectObject(dc, font) : NULL;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT caption = {x + 1, top + 1, x + BADGE_ICON_WIDTH - 1, top + BADGE_HEIGHT - 1};
        DrawTextW(dc, L"OG", 2, &caption, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        if (font) {
            SelectObject(dc, previous);
            DeleteObject(font);
        }
    } else if (type == "sub_gifter") {
        Ellipse(dc, x + 5, top + 3, x + 10, top + 8);
        Ellipse(dc, x + 9, top + 3, x + 14, top + 8);
        Rectangle(dc, x + 4, top + 8, x + 14, top + 10);
        Rectangle(dc, x + 5, top + 10, x + 13, top + 14);
        SetDCBrushColor(dc, background);
        SetDCPenColor(dc, background);
        Rectangle(dc, x + 8, top + 8, x + 10, top + 14);
    } else if (type == "verified") {
        Ellipse(dc, x + 3, top + 2, x + 15, top + 15);
        SetDCBrushColor(dc, background);
        SetDCPenColor(dc, background);
        POINT check[] = {{x + 5, top + 8}, {x + 8, top + 11}, {x + 13, top + 5},
                         {x + 14, top + 7}, {x + 8, top + 13}, {x + 4, top + 9}};
        Polygon(dc, check, 6);
    } else {
        POINT shield[] = {{x + 9, top + 2}, {x + 14, top + 5}, {x + 13, top + 11},
                          {x + 9, top + 15}, {x + 5, top + 11}, {x + 4, top + 5}};
        Polygon(dc, shield, 6);
        SetDCBrushColor(dc, background);
        SetDCPenColor(dc, background);
        Ellipse(dc, x + 8, top + 6, x + 11, top + 9);
    }
}

int badgeRowWidth(HDC dc, const std::vector<ChatBadge> &badges) {
    int width = 0;
    for (size_t i = 0; i < badges.size() && i < 8; ++i)
        width += badgeWidth(dc, badges[i]) + BADGE_SPACING;
    return width;
}

void drawBadgeRow(HDC dc, const std::vector<ChatBadge> &badges, int x, int y, int right) {
    for (size_t i = 0; i < badges.size() && i < 8; ++i) {
        const ChatBadge &badge = badges[i];
        const int width = badgeWidth(dc, badge);
        if (x + width > right) break;

        const int saved = SaveDC(dc);
        const COLORREF background = badgeColor(badge.type);
        SelectObject(dc, GetStockObject(DC_BRUSH));
        SelectObject(dc, GetStockObject(DC_PEN));
        SetDCBrushColor(dc, background);
        SetDCPenColor(dc, background);
        RoundRect(dc, x, y + 2, x + width, y + 2 + BADGE_HEIGHT, 6, 6);
        SetDCBrushColor(dc, RGB(255, 255, 255));
        SetDCPenColor(dc, RGB(255, 255, 255));
        drawBadgeIcon(dc, badge.type, x, y + 2, background);

        if (hasBadgeCount(badge)) {
            const std::wstring count = std::to_wstring(badge.count);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            RECT rc = {x + BADGE_ICON_WIDTH, y + 2, x + width - 3, y + 2 + BADGE_HEIGHT};
            DrawTextW(dc, count.c_str(), -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
        RestoreDC(dc, saved);
        x += width + BADGE_SPACING;
    }
}
