#include "kickchat_buttons.hpp"

bool drawRoundedButton(const DRAWITEMSTRUCT *item, HFONT font,
                       COLORREF backgroundColor, bool primary) {
    if (!item || item->CtlType != ODT_BUTTON) return false;

    bool disabled = (item->itemState & ODS_DISABLED) != 0;
    bool pressed = (item->itemState & ODS_SELECTED) != 0;
    COLORREF fill = primary ? RGB(78, 215, 122) : RGB(47, 54, 62);
    COLORREF foreground = primary ? RGB(17, 26, 20) : RGB(239, 243, 245);
    if (pressed) fill = primary ? RGB(47, 174, 95) : RGB(67, 76, 86);
    if (disabled) {
        fill = RGB(48, 54, 58);
        foreground = RGB(128, 137, 144);
    }

    HDC dc = item->hDC;
    RECT bounds = item->rcItem;
    HBRUSH background = CreateSolidBrush(backgroundColor);
    FillRect(dc, &bounds, background);
    DeleteObject(background);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1, 13, 13);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t caption[80] = {0};
    GetWindowTextW(item->hwndItem, caption, 80);
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, foreground);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, caption, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    if (item->itemState & ODS_FOCUS) {
        RECT focus = bounds;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(dc, &focus);
    }
    return true;
}

bool drawActionButton(const DRAWITEMSTRUCT *item, HFONT font) {
    if (!item || (item->CtlID != ID_SEND && item->CtlID != ID_SETTINGS &&
                  item->CtlID != ID_ADD_CHANNEL)) return false;
    return drawRoundedButton(item, font, RGB(28, 30, 34),
                             item->CtlID == ID_SEND || item->CtlID == ID_ADD_CHANNEL);
}
