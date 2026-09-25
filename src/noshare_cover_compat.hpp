#pragma once

// Compatibility with https://github.com/gitscout-bot/noshare-cover
//
// Research (noshare-cover main @ d4aae77, 2026-09):
// - Hooks Screenshare::CScreenshareFrame::renderMonitor via HyprlandAPI::createFunctionHook.
// - Chains the previous trampoline: hkRenderMonitor calls g_hook->m_original first, then
//   paintCovers() — so in theory a chain exists, but Hyprland's prologue trampoline is
//   exclusive: a second createFunctionHook on the same address typically fails (same reason
//   two gloview instances cannot both hook shouldRenderWindow).
// - Exports only the standard PLUGIN_* symbols; no public C API / IPC / config to register
//   extra cover rectangles (overview tile boxes). Covers are drawn only at real window
//   boxes for windows with no_screen_share (+ optional image/video media).
// - Plugin name from PLUGIN_INIT: "noshare-cover"; .so typically libnoshare-cover.so /
//   path containing "noshare-cover".
//
// Therefore dual-view (Path A: gloview owns renderMonitor, live local + black export tiles)
// cannot coexist with noshare-cover on the same trampoline. When cover is loaded we take
// Path B: never attempt renderMonitor; black noscreenshare overview tiles in the local
// overview pass while Screenshare::mgr()->isOutputBeingSSd (mirror carries black to share).
// Prefer loading noshare-cover before gloview so detection sees it at init.

#include <hyprland/src/plugins/PluginSystem.hpp>

#include <cstring>
#include <link.h>
#include <string_view>

namespace gloview::noshare_cover {

inline constexpr const char* kPluginName = "noshare-cover";
inline constexpr const char* kSoNeedle   = "noshare-cover";

enum class Path {
    A_DualView, // own ScreenshareFrame::renderMonitor; export-only tile blackout
    B_Coexist,  // leave renderMonitor to noshare-cover (or other owner); Option B while SS
};

inline bool nameLooksLikeCover(std::string_view s) {
    if (s.empty())
        return false;
    if (s == kPluginName)
        return true;
    return s.find(kSoNeedle) != std::string_view::npos;
}

inline bool mapsContainCoverSo() {
    struct Ctx {
        bool found = false;
    } ctx;
    dl_iterate_phdr(
        [](struct dl_phdr_info* info, size_t /*size*/, void* data) -> int {
            auto* c = static_cast<Ctx*>(data);
            if (!info || !info->dlpi_name || !info->dlpi_name[0])
                return 0;
            if (std::strstr(info->dlpi_name, kSoNeedle)) {
                c->found = true;
                return 1; // stop
            }
            return 0;
        },
        &ctx);
    return ctx.found;
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
    // Fallback: cover may be mid-load or listed under an unexpected name; maps still show .so.
    return mapsContainCoverSo();
}

// Decide export blackout path. Prefer Path B when cover is loaded and compat is on so we
// never fight for the exclusive renderMonitor trampoline.
// compatOn = plugin:gloview:noshare_cover_compat (default true).
inline Path choosePath(bool coverLoaded, bool compatOn) {
    if (compatOn && coverLoaded)
        return Path::B_Coexist;
    return Path::A_DualView; // attempt; caller falls back to B if hook fails
}

inline const char* pathLabel(Path p) {
    return p == Path::A_DualView ? "Path A (dual-view / own renderMonitor)" : "Path B (coexist / Option B while sharing)";
}

} // namespace gloview::noshare_cover
