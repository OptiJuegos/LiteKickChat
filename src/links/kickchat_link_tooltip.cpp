#include "kickchat_link_tooltip.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_link_metadata.hpp"

#include <gdiplus.h>
#include <objidl.h>
#include <algorithm>
#include <cstring>

namespace {

const wchar_t *TOOLTIP_CLASS = L"KickChatLinkTooltip";
const size_t MAX_IMAGE_BYTES = 8 * 1024 * 1024;

struct TooltipLayout {
    RECT image;
    RECT site;
    RECT title;
    RECT description;
    RECT url;
    int height;
    TooltipLayout() : height(0) {
        SetRectEmpty(&image);
        SetRectEmpty(&site);
        SetRectEmpty(&title);
        SetRectEmpty(&description);
        SetRectEmpty(&url);
    }
};

HWND g_tooltip = NULL;
HWND g_owner = NULL;
bool g_classRegistered = false;
bool g_hasMetadata = false;
bool g_loading = false;
int g_dpi = 96;
int g_width = 380;
int g_maxHeight = 0;
TooltipLayout g_layout;
std::wstring g_url;
std::wstring g_title;
std::wstring g_description;
std::wstring g_site;
LinkMetadata g_metadata = {};
HFONT g_font = NULL;
HFONT g_titleFont = NULL;
HFONT g_smallFont = NULL;
Gdiplus::Bitmap *g_image = NULL;
IStream *g_imageStream = NULL;

int scaled(int value) {
    return MulDiv(value, g_dpi, 96);
}

HFONT selectFont(HDC dc, HFONT font) {
    HFONT selected = font ? font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    return static_cast<HFONT>(SelectObject(dc, selected));
}

void releaseImage() {
    delete g_image;
    g_image = NULL;
    if (g_imageStream) {
        g_imageStream->Release();
        g_imageStream = NULL;
    }
}

void releaseFonts() {
    if (g_font) DeleteObject(g_font);
    if (g_titleFont) DeleteObject(g_titleFont);
    if (g_smallFont) DeleteObject(g_smallFont);
    g_font = NULL;
    g_titleFont = NULL;
    g_smallFont = NULL;
}

void createFonts() {
    releaseFonts();
    g_font = CreateFontW(-scaled(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_titleFont = CreateFontW(-scaled(16), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_smallFont = CreateFontW(-scaled(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

std::wstring singleLine(const std::wstring &source, size_t limit) {
    std::wstring text = source.substr(0, limit);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] < 0x20) text[i] = L' ';
    }
    return text;
}

std::wstring hostFromUrl(const std::wstring &url) {
    size_t start = url.find(L"://");
    start = start == std::wstring::npos ? 0 : start + 3;
    size_t end = url.find_first_of(L"/?#", start);
    std::wstring host = url.substr(start, end == std::wstring::npos ?
                                          std::wstring::npos : end - start);
    size_t at = host.rfind(L'@');
    if (at != std::wstring::npos) host = host.substr(at + 1);
    return singleLine(host, 160);
}

void decodeImage(const std::vector<BYTE> &bytes) {
    releaseImage();
    if (bytes.empty() || bytes.size() > MAX_IMAGE_BYTES) return;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return;
    void *address = GlobalLock(memory);
    if (!address) {
        GlobalFree(memory);
        return;
    }
    std::memcpy(address, &bytes[0], bytes.size());
    GlobalUnlock(memory);

    if (CreateStreamOnHGlobal(memory, TRUE, &g_imageStream) != S_OK) {
        GlobalFree(memory);
        g_imageStream = NULL;
        return;
    }
    g_image = Gdiplus::Bitmap::FromStream(g_imageStream, FALSE);
    if (!g_image || g_image->GetLastStatus() != Gdiplus::Ok ||
        !g_image->GetWidth() || !g_image->GetHeight() ||
        g_image->GetWidth() > 8192 || g_image->GetHeight() > 8192) {
        releaseImage();
    }
}

bool modelMatches(const std::wstring &url, const LinkMetadata *metadata, bool loading) {
    if (g_url != url || g_loading != loading || g_hasMetadata != (metadata != NULL))
        return false;
    if (!metadata) return true;
    return g_metadata.title == metadata->title &&
           g_metadata.description == metadata->description &&
           g_metadata.siteName == metadata->siteName &&
           g_metadata.imageUrl == metadata->imageUrl &&
           g_metadata.url == metadata->url &&
           g_metadata.success == metadata->success &&
           g_metadata.imageBytes == metadata->imageBytes;
}

int textHeight(HDC dc, const std::wstring &text, HFONT font,
               int width, int maxLines) {
    if (text.empty()) return 0;
    HFONT old = selectFont(dc, font);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    RECT rect = {0, 0, width, 0};
    DrawTextW(dc, text.c_str(), -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    const int lineHeight = static_cast<int>(metrics.tmHeight);
    return std::min(std::max(lineHeight, static_cast<int>(rect.bottom)),
                    lineHeight * maxLines);
}

void measureLayout(int maxHeight) {
    g_layout = TooltipLayout();
    HDC dc = GetDC(g_tooltip);
    if (!dc) return;

    const int pad = scaled(14);
    const int inner = g_width - 2 * pad;
    const int gap = scaled(8);
    int cursor = pad;

    if (g_image) {
        const int available = std::max(scaled(60), maxHeight / 3);
        const int imageHeight = std::min(scaled(160), available);
        g_layout.image = RECT{pad, cursor, g_width - pad, cursor + imageHeight};
        cursor += imageHeight + gap;
    }

    if (!g_site.empty()) {
        const int height = textHeight(dc, g_site, g_smallFont, inner, 1);
        g_layout.site = RECT{pad, cursor, g_width - pad, cursor + height};
        cursor += height + scaled(5);
    }

    const int titleHeight = textHeight(dc, g_title, g_titleFont, inner, 2);
    g_layout.title = RECT{pad, cursor, g_width - pad, cursor + titleHeight};
    cursor += titleHeight + gap;

    if (!g_description.empty()) {
        const int available = std::max(0, maxHeight - cursor - scaled(52));
        const int height = std::min(textHeight(dc, g_description, g_font, inner, 5),
                                    available);
        if (height > 0) {
            g_layout.description = RECT{pad, cursor, g_width - pad, cursor + height};
            cursor += height + gap;
        }
    }

    cursor += scaled(5);
    const int urlHeight = textHeight(dc, g_url, g_smallFont, inner, 1);
    g_layout.url = RECT{pad, cursor, g_width - pad, cursor + urlHeight};
    cursor += urlHeight + pad;
    g_layout.height = std::min(cursor, maxHeight);
    ReleaseDC(g_tooltip, dc);
}

void drawText(HDC dc, const std::wstring &text, RECT rect, HFONT font,
              COLORREF color, UINT flags) {
    if (text.empty() || IsRectEmpty(&rect)) return;
    HFONT old = selectFont(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}

void paintTooltip(HDC screen) {
    RECT rect;
    GetClientRect(g_tooltip, &rect);
    if (IsRectEmpty(&rect)) return;

    const int width = rect.right;
    const int height = rect.bottom;
    HDC buffer = CreateCompatibleDC(screen);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(screen, width, height) : NULL;
    HGDIOBJ original = bitmap ? SelectObject(buffer, bitmap) : NULL;
    HDC dc = bitmap ? buffer : screen;
    const int saved = SaveDC(dc);

    HBRUSH background = CreateSolidBrush(RGB(34, 38, 43));
    HBRUSH border = CreateSolidBrush(RGB(74, 82, 87));
    FillRect(dc, &rect, background);
    FrameRect(dc, &rect, border);
    SetBkMode(dc, TRANSPARENT);

    if (g_image && !IsRectEmpty(&g_layout.image)) {
        const RECT &area = g_layout.image;
        HBRUSH matte = CreateSolidBrush(RGB(25, 29, 33));
        FillRect(dc, &area, matte);
        DeleteObject(matte);
        const double scale = std::min(
            static_cast<double>(area.right - area.left) / g_image->GetWidth(),
            static_cast<double>(area.bottom - area.top) / g_image->GetHeight());
        const int drawWidth = std::max(1, static_cast<int>(g_image->GetWidth() * scale));
        const int drawHeight = std::max(1, static_cast<int>(g_image->GetHeight() * scale));
        const int drawLeft = area.left + (area.right - area.left - drawWidth) / 2;
        const int drawTop = area.top + (area.bottom - area.top - drawHeight) / 2;
        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(g_image, drawLeft, drawTop, drawWidth, drawHeight);
    }

    drawText(dc, g_site, g_layout.site, g_smallFont, RGB(111, 217, 145),
             DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
    drawText(dc, g_title, g_layout.title, g_titleFont, RGB(246, 248, 250),
             DT_WORDBREAK | DT_END_ELLIPSIS);
    drawText(dc, g_description, g_layout.description, g_font, RGB(202, 209, 215),
             DT_WORDBREAK | DT_END_ELLIPSIS);

    const int separatorY = g_layout.url.top - scaled(5);
    HPEN divider = CreatePen(PS_SOLID, 1, RGB(61, 67, 72));
    if (divider) {
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, divider));
        MoveToEx(dc, scaled(14), separatorY, NULL);
        LineTo(dc, width - scaled(14), separatorY);
        SelectObject(dc, oldPen);
        DeleteObject(divider);
    }
    drawText(dc, g_url, g_layout.url, g_smallFont, RGB(151, 165, 177),
             DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

    DeleteObject(background);
    DeleteObject(border);
    RestoreDC(dc, saved);
    if (bitmap) {
        BitBlt(screen, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, original);
        DeleteObject(bitmap);
    }
    if (buffer) DeleteDC(buffer);
}

LRESULT CALLBACK tooltipProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd, &paint);
            paintTooltip(dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_NCDESTROY:
            if (g_tooltip == hwnd) {
                g_tooltip = NULL;
                g_owner = NULL;
                releaseImage();
                releaseFonts();
            }
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool ensureTooltip(HWND parent) {
    if (g_tooltip && g_owner == parent) return true;
    if (g_tooltip) DestroyWindow(g_tooltip);
    if (!g_classRegistered) {
        WNDCLASSEXW cls = {};
        cls.cbSize = sizeof(cls);
        cls.hInstance = GetModuleHandleW(NULL);
        cls.lpfnWndProc = tooltipProcedure;
        cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
        cls.lpszClassName = TOOLTIP_CLASS;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        g_classRegistered = true;
    }
    g_tooltip = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                               TOOLTIP_CLASS, L"", WS_POPUP, 0, 0, 1, 1,
                               parent, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_tooltip) return false;
    g_owner = parent;
    g_url.clear();
    g_hasMetadata = false;
    g_maxHeight = 0;
    HDC dc = GetDC(parent);
    g_dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(parent, dc);
    if (g_dpi < 96) g_dpi = 96;
    createFonts();
    return true;
}

} // namespace

void showLinkTooltip(HWND parent, POINT clientAnchor, const std::wstring &url,
                     const LinkMetadata *metadata, bool loading) {
    if (!IsWindow(parent) || url.empty() || !ensureTooltip(parent)) return;

    POINT anchor = clientAnchor;
    ClientToScreen(parent, &anchor);
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &monitor)) {
        monitor.rcWork = RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    const RECT &work = monitor.rcWork;
    const int width = std::min(scaled(380),
                               std::max(1, static_cast<int>(work.right - work.left - scaled(12))));
    const int maxHeight = std::max(scaled(90),
                                  static_cast<int>(work.bottom - work.top - scaled(12)));

    const bool changed = !modelMatches(url, metadata, loading);
    if (changed) {
        g_url = singleLine(url, 2048);
        g_loading = loading;
        g_hasMetadata = metadata != NULL;
        g_metadata = metadata ? *metadata : LinkMetadata();
        g_site = singleLine(metadata && !metadata->siteName.empty() ?
                            metadata->siteName : hostFromUrl(url), 160);
        g_title = loading ? kickchatText::get(kickchatText::LinkPreviewLoading) :
                  metadata && !metadata->title.empty() ?
                      singleLine(metadata->title, 200) :
                  metadata && metadata->success ? g_site : kickchatText::get(kickchatText::LinkPreviewUnavailable);
        g_description = loading ? kickchatText::get(kickchatText::LinkMetadataLoading) :
                        metadata && metadata->success ?
                            singleLine(metadata->description, 700) :
                            kickchatText::get(kickchatText::LinkMetadataFailed);
        decodeImage(!loading && metadata ?
                    metadata->imageBytes : std::vector<BYTE>());
    }

    if (changed || width != g_width || maxHeight != g_maxHeight) {
        g_width = width;
        g_maxHeight = maxHeight;
        measureLayout(maxHeight);
        InvalidateRect(g_tooltip, NULL, FALSE);
    }
    const int height = g_layout.height;
    int x = anchor.x + scaled(20);
    int y = anchor.y + scaled(24);
    if (x + width > work.right - scaled(6)) x = anchor.x - width - scaled(20);
    if (y + height > work.bottom - scaled(6)) y = anchor.y - height - scaled(20);
    x = std::max(static_cast<int>(work.left + scaled(6)),
                 std::min(x, static_cast<int>(work.right - width - scaled(6))));
    y = std::max(static_cast<int>(work.top + scaled(6)),
                 std::min(y, static_cast<int>(work.bottom - height - scaled(6))));

    SetWindowPos(g_tooltip, HWND_TOP, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
}

void hideLinkTooltip() {
    if (g_tooltip) ShowWindow(g_tooltip, SW_HIDE);
}

void destroyLinkTooltip() {
    if (g_tooltip) DestroyWindow(g_tooltip);
    releaseImage();
    releaseFonts();
    g_metadata = LinkMetadata();
    g_url.clear();
    g_title.clear();
    g_description.clear();
    g_site.clear();
    g_hasMetadata = false;
    g_loading = false;
    g_maxHeight = 0;
    if (g_classRegistered) {
        UnregisterClassW(TOOLTIP_CLASS, GetModuleHandleW(NULL));
        g_classRegistered = false;
    }
}
