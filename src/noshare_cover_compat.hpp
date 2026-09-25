#pragma once

// Compatibility with https://github.com/gitscout-bot/noshare-cover
//
// noshare-cover hooks Screenshare::CScreenshareFrame::renderMonitor (exclusive
// trampoline) and paints covers on real noscreenshare window boxes. Since f74d67f it
// also exports a C API for *extra* cover rectangles (overview tile boxes):
//
//   void noshare_cover_clear_extra_rects(void);
//   void noshare_cover_add_extra_rect(int monitor_id, double x, double y,
//                                    double w, double h, double rounding);
//
// Hyprland loads plugins with RTLD_LOCAL, so dlsym(RTLD_DEFAULT, ...) will NOT see
// those symbols. Resolve the mapped .so via dl_iterate_phdr (needle "noshare-cover" /
// "libnoshare-cover.so"), then dlopen(path, RTLD_LAZY | RTLD_NOLOAD) + dlsym.
//
// API v2 (noshare-cover 0.2, Rust): per-client rect lists replaced atomically per monitor,
// rect fill = black or the window's own cover (NOSHARE_COVER_FILL_WINDOW), and an optional
// "gone" callback fired from noshare-cover's PLUGIN_EXIT after it removed its renderMonitor
// hook. With the callback we drop our RTLD_NOLOAD handle right after binding, so we never
// keep noshare-cover mapped after it is unloaded (a held handle makes dlclose a no-op and a
// same-path reload would then serve the stale image).
//
// Paths (plugin:gloview:noshare_cover_compat, default on):
//   C_CoverApi - cover loaded + API bound: leave renderMonitor alone; register tile
//                boxes each overview frame; local tiles stay live (dual-view).
//   B_Coexist  - cover loaded but API missing (old cover): Option B local-black while
//                isOutputBeingSSd (mirror carries black). Never fight for the hook.
//   A_DualView - cover absent (or compat off): gloview owns renderMonitor when free.
//
// Load order no longer matters: Overview re-picks the path on every config reload (Hyprland
// reloads config after each plugin load/unload) and on the gone callback. If gloview holds
// renderMonitor when noshare-cover appears, gloview releases it and noshare-cover (which
// retries its hook) takes over.

#include <hyprland/src/plugins/PluginSystem.hpp>

#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <string>
#include <string_view>
#include <vector>

namespace gloview::noshare_cover {

inline constexpr const char* kPluginName = "noshare-cover";
inline constexpr const char* kSoNeedle   = "noshare-cover";
inline constexpr const char* kSoLibName  = "libnoshare-cover.so";

enum class Path {
    A_DualView, // own ScreenshareFrame::renderMonitor; export-only tile blackout
    B_Coexist,  // leave renderMonitor to cover; Option B while SS (no extra-rect API)
    C_CoverApi, // leave renderMonitor to cover; extra-rect API → dual-view share blackout
};

using ClearExtraRectsFn = void (*)(void);
using AddExtraRectFn    = void (*)(int, double, double, double, double, double);

// v2 (noshare_cover_api.h in noshare-cover)
struct CoverRect {
    double   x, y, w, h;
    double   rounding;
    uint64_t window; // PHLWINDOW address (as in hyprctl clients -j), 0 = none
    uint32_t fill;   // 0 = black, 1 = the window's cover
};
static_assert(sizeof(CoverRect) == 56, "noshare_cover_rect layout");
inline constexpr uint32_t kFillBlack  = 0;
inline constexpr uint32_t kFillWindow = 1;

using ApiVersionFn  = uint32_t (*)(void);
using RegisterFn    = uint64_t (*)(const char*);
using UnregisterFn  = void (*)(uint64_t);
using SetRectsFn    = bool (*)(uint64_t, int, const CoverRect*, size_t);
using ClearClientFn = bool (*)(uint64_t);
using SetGoneFn     = bool (*)(uint64_t, void (*)(void*), void*);

inline ClearExtraRectsFn g_clearExtraRects = nullptr;
inline AddExtraRectFn    g_addExtraRect    = nullptr;
inline RegisterFn        g_register        = nullptr;
inline UnregisterFn      g_unregister      = nullptr;
inline SetRectsFn        g_setRects        = nullptr;
inline ClearClientFn     g_clearClient     = nullptr;
inline uint64_t          g_client          = 0;
inline void*             g_apiHandle       = nullptr;
inline bool              g_goneArmed       = false;
// tile buffer for the current frame (v2 replaces a monitor's list atomically)
inline std::vector<CoverRect> g_tiles;
inline int                    g_lastMonitor = -1;
// set from the gone callback; Overview picks it up off the render path
inline bool g_goneFlag = false;

inline bool nameLooksLikeCover(std::string_view s) {
    if (s.empty())
        return false;
    if (s == kPluginName)
        return true;
    return s.find(kSoNeedle) != std::string_view::npos;
}

inline bool soPathLooksLikeCover(const char* path) {
    if (!path || !path[0])
        return false;
    return std::strstr(path, kSoNeedle) != nullptr || std::strstr(path, kSoLibName) != nullptr;
}

// Absolute path of the mapped noshare-cover .so, or empty.
inline std::string findMappedSoPath() {
    if (g_pPluginSystem) {
        for (CPlugin* p : g_pPluginSystem->getAllPlugins()) {
            if (!p)
                continue;
            if (!nameLooksLikeCover(p->m_name) && !nameLooksLikeCover(p->m_path))
                continue;
            if (!p->m_path.empty())
                return p->m_path;
        }
    }

    struct Ctx {
        std::string path;
    } ctx;
    dl_iterate_phdr(
        [](struct dl_phdr_info* info, size_t /*size*/, void* data) -> int {
            auto* c = static_cast<Ctx*>(data);
            if (!info || !soPathLooksLikeCover(info->dlpi_name))
                return 0;
            c->path = info->dlpi_name;
            return 1; // stop
        },
        &ctx);
    return ctx.path;
}

// True if noshare-cover is a loaded plugin. Plugin list only: a stale mapping of an
// unloaded .so must not count.
inline bool isLoaded() {
    if (!g_pPluginSystem)
        return !findMappedSoPath().empty();
    for (CPlugin* p : g_pPluginSystem->getAllPlugins()) {
        if (p && (nameLooksLikeCover(p->m_name) || nameLooksLikeCover(p->m_path)))
            return true;
    }
    return false;
}

inline bool v2Bound() {
    return g_setRects && g_clearClient && g_client;
}

inline bool apiAvailable() {
    return v2Bound() || (g_clearExtraRects != nullptr && g_addExtraRect != nullptr);
}

// Forget every pointer into noshare-cover without calling into it.
inline void forget() {
    g_clearExtraRects = nullptr;
    g_addExtraRect    = nullptr;
    g_register        = nullptr;
    g_unregister      = nullptr;
    g_setRects        = nullptr;
    g_clearClient     = nullptr;
    g_client          = 0;
    g_goneArmed       = false;
    g_tiles.clear();
    g_lastMonitor = -1;
    if (g_apiHandle) {
        dlclose(g_apiHandle);
        g_apiHandle = nullptr;
    }
}

// noshare-cover is unloading (its PLUGIN_EXIT, renderMonitor already released).
inline void onGone(void*) {
    g_apiHandle = nullptr; // never held when the callback is armed
    forget();
    g_goneFlag = true;
}

// Bind v2 (preferred) and v1 via RTLD_NOLOAD. Safe to call repeatedly.
inline bool tryBind() {
    if (apiAvailable())
        return true;

    const std::string path = findMappedSoPath();
    if (path.empty())
        return false;

    void* h = dlopen(path.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (!h)
        return false;

    auto sym          = [&](const char* n) { return dlsym(h, n); };
    g_clearExtraRects = reinterpret_cast<ClearExtraRectsFn>(sym("noshare_cover_clear_extra_rects"));
    g_addExtraRect    = reinterpret_cast<AddExtraRectFn>(sym("noshare_cover_add_extra_rect"));

    const auto ver = reinterpret_cast<ApiVersionFn>(sym("noshare_cover_api_version"));
    if (ver && ver() >= 2) {
        g_register         = reinterpret_cast<RegisterFn>(sym("noshare_cover_register_client"));
        g_unregister       = reinterpret_cast<UnregisterFn>(sym("noshare_cover_unregister_client"));
        g_setRects         = reinterpret_cast<SetRectsFn>(sym("noshare_cover_set_rects"));
        g_clearClient      = reinterpret_cast<ClearClientFn>(sym("noshare_cover_clear_client_rects"));
        const auto setGone = reinterpret_cast<SetGoneFn>(sym("noshare_cover_set_gone_callback"));
        if (g_register && g_setRects && g_clearClient)
            g_client = g_register("gloview");
        if (g_client && setGone && setGone(g_client, &onGone, nullptr))
            g_goneArmed = true;
    }

    if (!apiAvailable()) {
        forget();
        dlclose(h);
        return false;
    }

    // With the gone callback we hear about unload before noshare-cover is unmapped, so the
    // handle is not needed; holding it would keep noshare-cover mapped forever.
    if (g_goneArmed)
        dlclose(h);
    else
        g_apiHandle = h;
    return true;
}

inline void clearExtraRects() {
    if (v2Bound())
        g_clearClient(g_client);
    else if (g_clearExtraRects)
        g_clearExtraRects();
    g_lastMonitor = -1;
}

// Frame protocol: beginTiles(); addExtraRect(...)*; commitTiles(monitor).
inline void beginTiles() {
    g_tiles.clear();
    if (!v2Bound() && g_clearExtraRects)
        g_clearExtraRects(); // v1 has no atomic replace
}

inline void addExtraRect(int monitorId, double x, double y, double w, double h, double rounding, uint64_t window = 0) {
    if (!(w > 0.0) || !(h > 0.0))
        return;
    if (v2Bound()) {
        g_tiles.push_back(CoverRect{x, y, w, h, rounding, window, window ? kFillWindow : kFillBlack});
        return;
    }
    if (g_addExtraRect)
        g_addExtraRect(monitorId, x, y, w, h, rounding);
}

inline void commitTiles(int monitorId) {
    if (!v2Bound())
        return;
    // the overview moved to another monitor: drop what we left on the old one
    if (g_lastMonitor >= 0 && g_lastMonitor != monitorId)
        g_setRects(g_client, g_lastMonitor, nullptr, 0);
    g_setRects(g_client, monitorId, g_tiles.data(), g_tiles.size());
    g_lastMonitor = monitorId;
}

// Clear our rects and let go of noshare-cover (plugin unload / path change).
inline void unbind() {
    clearExtraRects();
    if (v2Bound() && g_unregister)
        g_unregister(g_client);
    forget();
}

// compatOn = plugin:gloview:noshare_cover_compat (default true).
// apiReady = tryBind() succeeded (cover exports clear/add).
inline Path choosePath(bool coverLoaded, bool compatOn, bool apiReady) {
    if (compatOn && coverLoaded)
        return apiReady ? Path::C_CoverApi : Path::B_Coexist;
    return Path::A_DualView; // attempt; caller falls back to B if hook fails
}

inline const char* pathLabel(Path p) {
    switch (p) {
        case Path::A_DualView: return "Path A (dual-view / own renderMonitor)";
        case Path::B_Coexist:  return "Path B (coexist / Option B while sharing)";
        case Path::C_CoverApi: return "Path C (noshare-cover extra-rect API / dual-view)";
    }
    return "Path ?";
}

} // namespace gloview::noshare_cover
