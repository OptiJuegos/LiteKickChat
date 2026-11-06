#include "kickchat_chat_paint.hpp"
#include "kickchat_link_hover.hpp"
#include "kickchat_link_tooltip.hpp"

struct LinkHoverDelivery {
    SRWLOCK lock;
    volatile LONG references;
    bool closing;

    LinkHoverDelivery() : references(1), closing(false) { InitializeSRWLock(&lock); }
};

struct PreviewJob {
    HWND owner;
    LinkHoverDelivery *delivery;
    std::wstring url;
    LinkMetadata metadata;
};

static void releaseDelivery(LinkHoverDelivery *delivery) {
    if (InterlockedDecrement(&delivery->references) == 0) delete delivery;
}

static DWORD WINAPI previewWorker(LPVOID parameter) {
    PreviewJob *job = static_cast<PreviewJob *>(parameter);
    fetchLinkMetadata(job->url, job->metadata);
    LinkHoverDelivery *delivery = job->delivery;
    AcquireSRWLockExclusive(&delivery->lock);
    const bool posted = !delivery->closing &&
        PostMessageW(job->owner, LinkHoverController::Message, 0,
                     reinterpret_cast<LPARAM>(job)) != 0;
    ReleaseSRWLockExclusive(&delivery->lock);
    if (!posted) delete job;
    releaseDelivery(delivery);
    return 0;
}

LinkHoverController::LinkHoverController()
    : point_(), fetching_(false), delivery_(new LinkHoverDelivery()) {}

LinkHoverController::~LinkHoverController() {
    if (delivery_) {
        AcquireSRWLockExclusive(&delivery_->lock);
        delivery_->closing = true;
        ReleaseSRWLockExclusive(&delivery_->lock);
        releaseDelivery(delivery_);
    }
}

void LinkHoverController::clear(HWND hwnd) {
    KillTimer(hwnd, Timer);
    url_.clear();
    hideLinkTooltip();
}

void LinkHoverController::track(HWND hwnd, int x, int y, bool enabled, bool selecting) {
    std::wstring url;
    if (!enabled || selecting || !chatLinkUrlAt(x, y, url)) {
        if (!url_.empty()) clear(hwnd);
        return;
    }
    point_.x = x;
    point_.y = y;
    if (url == url_) return;
    clear(hwnd);
    url_ = url;
    TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&track);
    SetTimer(hwnd, Timer, 240, NULL);
}

void LinkHoverController::begin(HWND hwnd, bool enabled) {
    KillTimer(hwnd, Timer);
    if (url_.empty() || !enabled || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return;
    POINT mouse = {};
    std::wstring current;
    if (!GetCursorPos(&mouse) || !ScreenToClient(hwnd, &mouse) ||
        !chatLinkUrlAt(mouse.x, mouse.y, current) || current != url_) {
        clear(hwnd);
        return;
    }
    point_ = mouse;
    std::map<std::wstring, LinkMetadata>::const_iterator cached = cache_.find(url_);
    if (cached != cache_.end()) {
        showLinkTooltip(hwnd, point_, url_, &cached->second, false);
        return;
    }
    showLinkTooltip(hwnd, point_, url_, NULL, true);
    if (fetching_) return;
    PreviewJob *job = new PreviewJob();
    job->owner = hwnd;
    job->delivery = delivery_;
    job->url = url_;
    InterlockedIncrement(&delivery_->references);
    fetching_ = true;
    HANDLE thread = CreateThread(NULL, 0, previewWorker, job, 0, NULL);
    if (!thread) {
        fetching_ = false;
        releaseDelivery(delivery_);
        delete job;
        hideLinkTooltip();
        return;
    }
    CloseHandle(thread);
}

void LinkHoverController::complete(HWND hwnd, LPARAM result, bool enabled) {
    PreviewJob *job = reinterpret_cast<PreviewJob *>(result);
    fetching_ = false;
    if (!job) return;
    if (cache_.size() >= 20) cache_.erase(cache_.begin());
    cache_[job->url] = job->metadata;
    if (enabled && url_ == job->url)
        begin(hwnd, enabled);
    else if (!url_.empty() && enabled)
        SetTimer(hwnd, Timer, 240, NULL);
    delete job;
}

void LinkHoverController::validate(HWND hwnd) {
    if (url_.empty()) return;
    POINT point = {};
    std::wstring underMouse;
    if (!GetCursorPos(&point) || !ScreenToClient(hwnd, &point) ||
        !chatLinkUrlAt(point.x, point.y, underMouse) || underMouse != url_)
        clear(hwnd);
}

void LinkHoverController::destroy(HWND hwnd) {
    clear(hwnd);
    destroyLinkTooltip();
    if (!delivery_) return;
    AcquireSRWLockExclusive(&delivery_->lock);
    delivery_->closing = true;
    // Drain queued results while posting is locked to avoid a dangling HWND/job.
    MSG pending = {};
    while (PeekMessageW(&pending, hwnd, Message, Message, PM_REMOVE))
        delete reinterpret_cast<PreviewJob *>(pending.lParam);
    ReleaseSRWLockExclusive(&delivery_->lock);
    releaseDelivery(delivery_);
    delivery_ = NULL;
}
