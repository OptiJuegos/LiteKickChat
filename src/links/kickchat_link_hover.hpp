#ifndef KICKCHAT_LINK_HOVER_HPP
#define KICKCHAT_LINK_HOVER_HPP

#include "kickchat_platform.hpp"
#include "kickchat_link_metadata.hpp"
#include <map>

struct LinkHoverDelivery;

class LinkHoverController {
public:
    enum { Message = WM_APP + 20, Timer = 2 };

    LinkHoverController();
    ~LinkHoverController();

    void clear(HWND hwnd);
    void track(HWND hwnd, int x, int y, bool enabled, bool selecting);
    void begin(HWND hwnd, bool enabled);
    void complete(HWND hwnd, LPARAM result, bool enabled);
    void validate(HWND hwnd);
    void destroy(HWND hwnd);

private:
    LinkHoverController(const LinkHoverController &);
    LinkHoverController &operator=(const LinkHoverController &);

    std::wstring url_;
    POINT point_;
    bool fetching_;
    std::map<std::wstring, LinkMetadata> cache_;
    LinkHoverDelivery *delivery_;
};

#endif
