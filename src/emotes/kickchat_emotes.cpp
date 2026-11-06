#include "kickchat_emotes.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_http.hpp"
#include "kickchat_json.hpp"
#include <propidl.h>
#include <gdiplus.h>
#include <objidl.h>
#include <set>

struct EmoteRequest {
    std::string key;
    std::string url;
    std::string channel;
};

static CRITICAL_SECTION g_lock;
static HANDLE g_wake = NULL;
static HANDLE g_worker = NULL;
static HWND g_target = NULL;
static bool g_started = false;
static std::deque<EmoteRequest> g_requests;
static std::set<std::string> g_requested;
static EmoteCatalog g_catalog;
static EmoteCatalog g_globalCatalog;
static std::map<std::string, EmoteCatalog> g_channelCatalogs;
static std::string g_activeChannel;

static void rebuildCatalog() {
    g_catalog = g_globalCatalog;
    std::map<std::string, EmoteCatalog>::const_iterator channel = g_channelCatalogs.find(g_activeChannel);
    if (channel != g_channelCatalogs.end())
        for (EmoteCatalog::const_iterator it = channel->second.begin(); it != channel->second.end(); ++it)
            g_catalog[it->first] = it->second;
}
struct CachedEmote {
    Gdiplus::Bitmap *bitmap;
    IStream *stream;
    unsigned frame;
    unsigned frameCount;
    DWORD nextFrame;
    std::vector<DWORD> delays;
    CachedEmote() : bitmap(NULL), stream(NULL), frame(0), frameCount(0), nextFrame(0) {}
};
static std::map<std::string, CachedEmote> g_images;
static std::set<std::string> g_visibleEmotes;
static bool g_animatedEmotesEnabled = true;

static bool postPayload(UINT event, void *payload) {
    if (g_target && PostMessageW(g_target, event, 0, (LPARAM)payload)) return true;
    return false;
}

static bool validDigits(const std::string &text) {
    if (text.empty() || text.size() > 20) return false;
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    return text != "0";
}

static void loadCatalog() {
    EmoteCatalogDownload *download = new EmoteCatalogDownload();
    std::string response;
    if (httpGet(L"https://7tv.io/v3/emote-sets/global", response, 1024 * 1024))
        parseSevenTvSet(response, download->catalog);
    if (!postPayload(WM_EMOTE_CATALOG, download)) delete download;
}

static DWORD WINAPI emoteWorker(LPVOID) {
    loadCatalog();
    while (running()) {
        EmoteRequest request;
        EnterCriticalSection(&g_lock);
        if (!g_requests.empty()) {
            request = g_requests.front();
            g_requests.pop_front();
        }
        LeaveCriticalSection(&g_lock);
        if (request.key.empty()) {
            WaitForSingleObject(g_wake, 1000);
            continue;
        }
        if (request.key.compare(0, 8, "channel:") == 0) {
            std::string response;
            if (httpGet(toWide(request.url), response, 1024 * 1024)) {
                EmoteCatalogDownload *download = new EmoteCatalogDownload();
                download->channel = request.channel;
                if (parseSevenTvSet(response, download->catalog) &&
                    postPayload(WM_EMOTE_CATALOG, download)) continue;
                delete download;
            }
            continue;
        }
        EmoteDownload *result = new EmoteDownload();
        result->key = request.key;
        const size_t maximum = request.key.compare(0, 5, "kick:") == 0 ?
            1024 * 1024 : 768 * 1024;
        httpGet(toWide(request.url), result->bytes, maximum);
        if (!postPayload(WM_EMOTE_IMAGE, result)) delete result;
    }
    return 0;
}

void startEmoteService(HWND window) {
    if (g_started) return;
    g_target = window;
    InitializeCriticalSection(&g_lock);
    g_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (g_wake) g_worker = CreateThread(NULL, 0, emoteWorker, NULL, 0, NULL);
    g_started = g_wake && g_worker;
    if (!g_started) {
        if (g_wake) CloseHandle(g_wake);
        g_wake = NULL;
        DeleteCriticalSection(&g_lock);
    }
}

void stopEmoteService() {
    if (g_started) {
        SetEvent(g_wake);
        // Process shutdown stops WinINet calls after their configured timeout.
        const DWORD stopped = WaitForSingleObject(g_worker, 250);
        if (stopped == WAIT_OBJECT_0) {
            DeleteCriticalSection(&g_lock);
            CloseHandle(g_wake);
        }
        // A blocked WinINet worker retains its synchronization objects until process exit.
        CloseHandle(g_worker);
        g_worker = NULL;
        g_wake = NULL;
        g_started = false;
    }
    for (std::map<std::string, CachedEmote>::iterator it = g_images.begin();
         it != g_images.end(); ++it) {
        delete it->second.bitmap;
        if (it->second.stream) it->second.stream->Release();
    }
    g_images.clear();
    g_visibleEmotes.clear();
}

const EmoteCatalog &currentEmoteCatalog() { return g_catalog; }

void requestSevenTvChannel(unsigned long long kickUserId, const std::string &slug) {
    if (!g_started || !kickUserId || slug.empty()) return;
    char id[32];
    sprintf(id, "%llu", kickUserId);
    const std::string key = std::string("channel:") + id;
    EnterCriticalSection(&g_lock);
    if (g_requested.insert(key).second) {
        EmoteRequest request;
        request.key = key;
        request.channel = slug;
        request.url = std::string("https://7tv.io/v3/users/kick/") + id;
        g_requests.push_back(request);
        SetEvent(g_wake);
    }
    LeaveCriticalSection(&g_lock);
}

void applyEmoteCatalog(EmoteCatalogDownload *download) {
    if (!download) return;
    if (download->channel.empty()) g_globalCatalog.swap(download->catalog);
    else g_channelCatalogs[download->channel].swap(download->catalog);
    rebuildCatalog();
    delete download;
}

void activateEmoteChannel(const std::string &slug) {
    if (g_activeChannel != slug) {
        g_activeChannel = slug;
        rebuildCatalog();
    }
}

void applyEmoteDownload(EmoteDownload *download) {
    if (!download) return;
    CachedEmote entry;
    if (!download->bytes.empty() && g_images.size() < 160) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, download->bytes.size());
        if (memory) {
            void *buffer = GlobalLock(memory);
            if (buffer) {
                memcpy(buffer, download->bytes.data(), download->bytes.size());
                GlobalUnlock(memory);
                if (CreateStreamOnHGlobal(memory, TRUE, &entry.stream) != S_OK)
                    GlobalFree(memory);
            } else GlobalFree(memory);
        }
        if (entry.stream) {
            entry.bitmap = new Gdiplus::Bitmap(entry.stream);
            if (entry.bitmap->GetLastStatus() != Gdiplus::Ok ||
                entry.bitmap->GetWidth() == 0 || entry.bitmap->GetWidth() > 256 ||
                entry.bitmap->GetHeight() == 0 || entry.bitmap->GetHeight() > 256) {
                delete entry.bitmap;
                entry.bitmap = NULL;
            }
        }
        if (entry.bitmap) {
            const UINT count = entry.bitmap->GetFrameCount(&Gdiplus::FrameDimensionTime);
            if (count > 1 && count <= 100) {
                entry.frameCount = count;
                entry.delays.assign(count, 100);
                const UINT size = entry.bitmap->GetPropertyItemSize(PropertyTagFrameDelay);
                if (size >= sizeof(Gdiplus::PropertyItem) && size <= 4096) {
                    std::vector<BYTE> memory(size);
                    Gdiplus::PropertyItem *item = reinterpret_cast<Gdiplus::PropertyItem *>(&memory[0]);
                    if (entry.bitmap->GetPropertyItem(PropertyTagFrameDelay, size, item) == Gdiplus::Ok &&
                        item->length >= count * sizeof(DWORD) && item->value) {
                        const DWORD *centiseconds = static_cast<const DWORD *>(item->value);
                        for (UINT i = 0; i < count; ++i)
                            entry.delays[i] = std::max<DWORD>(40, std::min<DWORD>(1000, centiseconds[i] * 10));
                    }
                }
                entry.nextFrame = GetTickCount() + entry.delays[0];
            }
        }
    }
    if (entry.bitmap && g_images.size() < 160) {
        g_images[download->key] = entry;
    } else {
        delete entry.bitmap;
        if (entry.stream) entry.stream->Release();
        // Remember failed decodes so they are not repeatedly downloaded.
        if (g_images.size() < 160) g_images[download->key] = CachedEmote();
    }
    delete download;
}

Gdiplus::Bitmap *cachedEmoteImage(const std::string &key) {
    std::map<std::string, CachedEmote>::const_iterator found = g_images.find(key);
    return found == g_images.end() ? NULL : found->second.bitmap;
}

void clearVisibleEmotes() { g_visibleEmotes.clear(); }
void markVisibleEmote(const std::string &key) { g_visibleEmotes.insert(key); }

void setAnimatedEmotesEnabled(bool enabled) {
    if (g_animatedEmotesEnabled == enabled) return;
    g_animatedEmotesEnabled = enabled;
    const DWORD now = GetTickCount();
    for (std::map<std::string, CachedEmote>::iterator it = g_images.begin();
         it != g_images.end(); ++it) {
        CachedEmote &entry = it->second;
        if (!entry.bitmap || entry.frameCount < 2) continue;
        if (!enabled && entry.frame != 0) {
            if (entry.bitmap->SelectActiveFrame(&Gdiplus::FrameDimensionTime, 0) ==
                Gdiplus::Ok) entry.frame = 0;
        }
        if (enabled && !entry.delays.empty())
            entry.nextFrame = now + entry.delays[entry.frame];
    }
}

bool advanceAnimatedEmotes(DWORD now) {
    if (!g_animatedEmotesEnabled) return false;
    bool changed = false;
    for (std::map<std::string, CachedEmote>::iterator it = g_images.begin();
         it != g_images.end(); ++it) {
        CachedEmote &entry = it->second;
        if (g_visibleEmotes.find(it->first) == g_visibleEmotes.end() ||
            !entry.bitmap || entry.frameCount < 2) continue;
        if (static_cast<LONG>(now - entry.nextFrame) < 0) continue;
        entry.frame = (entry.frame + 1) % entry.frameCount;
        if (entry.bitmap->SelectActiveFrame(&Gdiplus::FrameDimensionTime, entry.frame) == Gdiplus::Ok)
            changed = true;
        entry.nextFrame = now + entry.delays[entry.frame];
    }
    return changed;
}

Gdiplus::Bitmap *findEmoteImage(const std::string &key, const std::string &url) {
    std::map<std::string, CachedEmote>::const_iterator it = g_images.find(key);
    if (it != g_images.end()) return it->second.bitmap;
    if (!g_started || g_requested.size() >= 160) return NULL;
    if (g_requested.find(key) != g_requested.end()) return NULL;
    if (url.compare(0, 8, "https://") != 0) return NULL;
    EnterCriticalSection(&g_lock);
    if (g_requests.size() < 48) {
        EmoteRequest request;
        request.key = key;
        request.url = url;
        g_requests.push_back(request);
        g_requested.insert(key);
        SetEvent(g_wake);
    }
    LeaveCriticalSection(&g_lock);
    return NULL;
}
