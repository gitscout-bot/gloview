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
// Paths (plugin:gloview:noshare_cover_compat, default on):
//   C_CoverApi — cover loaded + API bound: leave renderMonitor alone; register tile
//                boxes each overview frame; local tiles stay live (dual-view).
//   B_Coexist  — cover loaded but API missing (old cover): Option B local-black while
//                isOutputBeingSSd (mirror carries black). Never fight for the hook.
//   A_DualView — cover absent (or compat off): gloview owns renderMonitor when free.
//
// Prefer loading noshare-cover before gloview so detection/bind succeed at init.
// Rects persist until clear (not one-frame). clear on overview close / plugin unload.

#include <hyprland/src/plugins/PluginSystem.hpp>

#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <string>
#include <string_view>

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

inline ClearExtraRectsFn g_clearExtraRects = nullptr;
inline AddExtraRectFn    g_addExtraRect    = nullptr;
inline void*             g_apiHandle       = nullptr;

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
            if (!p->m_path.empty() && soPathLooksLikeCover(p->m_path.c_str()))
                return p->m_path;
            if (!p->m_path.empty())
                return p->m_path; // name matched; path may still be the .so
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

inline bool mapsContainCoverSo() {
    return !findMappedSoPath().empty();
}

// True if noshare-cover appears loaded (plugin list name/path, or mapped .so).
inline bool isLoaded() {
    if (g_pPluginSystem) {
        for (CPlugin* p : g_pPluginSystem->getAllPlugins()) {
            if (!p)
                continue;
            if (nameLooksLikeCover(p->m_name) || nameLooksLikeCover(p->m_path))
                return true;
        }
    }
    return mapsContainCoverSo();
}

// Bind clear/add via RTLD_NOLOAD. Safe to call repeatedly; no-ops once bound.
// Returns true only when both symbols resolve. Never crashes on missing .so / symbols.
inline bool tryBind() {
    if (g_clearExtraRects && g_addExtraRect)
        return true;

    const std::string path = findMappedSoPath();
    if (path.empty())
        return false;

    void* h = dlopen(path.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (!h)
        return false;

    auto clear = reinterpret_cast<ClearExtraRectsFn>(dlsym(h, "noshare_cover_clear_extra_rects"));
    auto add   = reinterpret_cast<AddExtraRectFn>(dlsym(h, "noshare_cover_add_extra_rect"));
    if (!clear || !add) {
        dlclose(h);
        return false;
    }

    g_apiHandle       = h;
    g_clearExtraRects = clear;
    g_addExtraRect    = add;
    return true;
}

inline bool apiAvailable() {
    return g_clearExtraRects != nullptr && g_addExtraRect != nullptr;
}

inline void clearExtraRects() {
    if (g_clearExtraRects)
        g_clearExtraRects();
}

inline void addExtraRect(int monitorId, double x, double y, double w, double h, double rounding) {
    if (!g_addExtraRect)
        return;
    if (!(w > 0.0) || !(h > 0.0))
        return;
    g_addExtraRect(monitorId, x, y, w, h, rounding);
}

// Drop function pointers + our RTLD_NOLOAD ref. Call clear first so cover's list is empty
// even if the .so stays mapped. Safe if never bound.
inline void unbind() {
    clearExtraRects();
    g_clearExtraRects = nullptr;
    g_addExtraRect    = nullptr;
    if (g_apiHandle) {
        dlclose(g_apiHandle);
        g_apiHandle = nullptr;
    }
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
