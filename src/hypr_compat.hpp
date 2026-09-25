#pragma once

// Dual-ABI helpers for Hyprland:
//   Legacy — v0.56.2 (desktop/view/Window.hpp, SHyprCtlCommand, State::create/query.id,
//             m_isMapped / m_title / m_floatingOffset, unscoped HL_MODIFIER_*)
//   New    — 83cf6a6+ (desktop/view/window/Window.hpp, ipc/s1/S1.hpp, createNumbered /
//             query.numbered, mapped()/presentation()/metadata()/backend(), Input:: mods)
//
// Detection is compile-time via __has_include on headers that only exist on one side.
// Prefer these wrappers over raw Hyprland symbols so one tree builds against either pin.

#if defined(__has_include)
#    if __has_include(<hyprland/src/desktop/view/window/Window.hpp>)
#        define GLOVIEW_HYPR_ABI_NEW 1
#    endif
#    if __has_include(<hyprland/src/ipc/s1/S1.hpp>)
#        define GLOVIEW_HYPR_IPC_S1 1
#    endif
#endif

#ifndef GLOVIEW_HYPR_ABI_NEW
#    define GLOVIEW_HYPR_ABI_NEW 0
#endif
#ifndef GLOVIEW_HYPR_IPC_S1
#    define GLOVIEW_HYPR_IPC_S1 0
#endif

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>

#if GLOVIEW_HYPR_ABI_NEW
#    include <hyprland/src/desktop/view/LayerSurface.hpp>
#    include <hyprland/src/desktop/view/window/Window.hpp>
#    include <hyprland/src/desktop/view/window/WindowBackend.hpp>
#    include <hyprland/src/desktop/view/window/WindowMetadata.hpp>
#    include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#    include <hyprland/src/input/Keys.hpp>
#    include <hyprland/src/output/Monitor.hpp>
#    include <hyprland/src/workspace/AbstractWorkspace.hpp>
#    include <hyprland/src/workspace/HLWorkspace.hpp>
#    include <hyprland/src/workspace/RegularWorkspace.hpp>
#else
#    include <hyprland/src/desktop/Workspace.hpp>
#    include <hyprland/src/desktop/view/LayerSurface.hpp>
#    include <hyprland/src/desktop/view/Window.hpp>
#    include <hyprland/src/devices/IKeyboard.hpp>
#    include <hyprland/src/output/Monitor.hpp> // complete Monitor::CMonitor for m_id in wsCreateNumbered
#endif

#if GLOVIEW_HYPR_IPC_S1
#    include <hyprland/src/ipc/s1/S1.hpp>
#endif

#include <cstdint>
#include <string>
#include <utility>

namespace gloview {

// --- modifiers (legacy: unscoped HL_MODIFIER_*; new: Input::eKeyboardModifiers) ---------

#if GLOVIEW_HYPR_ABI_NEW
inline constexpr uint32_t MOD_NONE  = static_cast<uint32_t>(Input::HL_MODIFIER_NONE);
inline constexpr uint32_t MOD_SHIFT = static_cast<uint32_t>(Input::HL_MODIFIER_SHIFT);
inline constexpr uint32_t MOD_CTRL  = static_cast<uint32_t>(Input::HL_MODIFIER_CTRL);
inline constexpr uint32_t MOD_ALT   = static_cast<uint32_t>(Input::HL_MODIFIER_ALT);
inline constexpr uint32_t MOD_META  = static_cast<uint32_t>(Input::HL_MODIFIER_META);
#else
inline constexpr uint32_t MOD_NONE  = 0;
inline constexpr uint32_t MOD_SHIFT = HL_MODIFIER_SHIFT;
inline constexpr uint32_t MOD_CTRL  = HL_MODIFIER_CTRL;
inline constexpr uint32_t MOD_ALT   = HL_MODIFIER_ALT;
inline constexpr uint32_t MOD_META  = HL_MODIFIER_META;
#endif

inline constexpr uint32_t MOD_STRICT = MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_META;

// --- window accessors ------------------------------------------------------------------

inline bool winMapped(const PHLWINDOW& w) {
#if GLOVIEW_HYPR_ABI_NEW
    return w && w->mapped();
#else
    return w && w->m_isMapped;
#endif
}

inline const std::string& winTitle(const PHLWINDOW& w) {
    static const std::string empty;
    if (!w)
        return empty;
#if GLOVIEW_HYPR_ABI_NEW
    return w->metadata().title();
#else
    return w->m_title;
#endif
}

inline const std::string& winAppId(const PHLWINDOW& w) {
    static const std::string empty;
    if (!w)
        return empty;
#if GLOVIEW_HYPR_ABI_NEW
    return w->metadata().appID();
#else
    return w->m_class;
#endif
}

inline Vector2D winFloatingOffset(const PHLWINDOW& w) {
    if (!w)
        return {};
#if GLOVIEW_HYPR_ABI_NEW
    return w->presentation().floatingOffset();
#else
    return w->m_floatingOffset;
#endif
}

inline float winRoundingPower(const PHLWINDOW& w) {
    if (!w)
        return 2.F;
#if GLOVIEW_HYPR_ABI_NEW
    return w->presentation().roundingPower();
#else
    return w->roundingPower();
#endif
}

inline bool winIsX11(const PHLWINDOW& w) {
    if (!w)
        return false;
#if GLOVIEW_HYPR_ABI_NEW
    return w->backend().isX11();
#else
    return w->m_isX11;
#endif
}

inline Vector2D winReportedSize(const PHLWINDOW& w) {
    if (!w)
        return {};
#if GLOVIEW_HYPR_ABI_NEW
    return w->backend().reportedSize();
#else
    return w->getReportedSize();
#endif
}

inline bool winNoScreenShare(const PHLWINDOW& w) {
    return w && w->m_ruleApplicator && w->m_ruleApplicator->noScreenShare().valueOrDefault();
}

// --- layer surfaces --------------------------------------------------------------------

inline bool layerMapped(const PHLLS& ls) {
#if GLOVIEW_HYPR_ABI_NEW
    return ls && ls->mapped();
#else
    return ls && ls->m_mapped;
#endif
}

// --- workspace accessors ---------------------------------------------------------------

inline bool wsIsSpecial(const PHLWORKSPACE& ws) {
    if (!ws)
        return false;
#if GLOVIEW_HYPR_ABI_NEW
    return ws->type() == Workspace::eWorkspaceType::SPECIAL;
#else
    return ws->m_isSpecialWorkspace;
#endif
}

// Numbered workspace id as int, or 0 when the workspace is named/special/unnumbered.
inline int wsNumericId(const PHLWORKSPACE& ws) {
    if (!ws)
        return 0;
#if GLOVIEW_HYPR_ABI_NEW
    if (const auto n = ws->numberedID())
        return static_cast<int>(*n);
    return 0;
#else
    return static_cast<int>(ws->m_id);
#endif
}

inline bool wsIsNumberedPositive(const PHLWORKSPACE& ws) {
#if GLOVIEW_HYPR_ABI_NEW
    return wsNumericId(ws) > 0 && !wsIsSpecial(ws);
#else
    return ws && !ws->m_isSpecialWorkspace && ws->m_id > 0;
#endif
}

inline const std::string& wsDisplayName(const PHLWORKSPACE& ws) {
    static const std::string empty;
    if (!ws)
        return empty;
#if GLOVIEW_HYPR_ABI_NEW
    return ws->displayName();
#else
    return ws->m_name;
#endif
}

inline bool wsVisible(const PHLWORKSPACE& ws) {
    if (!ws)
        return false;
#if GLOVIEW_HYPR_ABI_NEW
    return ws->visible();
#else
    return ws->isVisible(); // == m_visible on 0.56.2
#endif
}

inline void wsSetVisible(const PHLWORKSPACE& ws, bool visible) {
    if (!ws)
        return;
#if GLOVIEW_HYPR_ABI_NEW
    ws->setVisible(visible);
#else
    ws->m_visible = visible;
#endif
}

inline void wsSetPersistent(const PHLWORKSPACE& ws, bool persistent) {
    if (!ws)
        return;
#if GLOVIEW_HYPR_ABI_NEW
    if (const auto regular = dynamicPointerCast<Workspace::CRegularWorkspace>(ws))
        regular->setPersistent(persistent);
#else
    ws->setPersistent(persistent);
#endif
}

inline bool wsIsPersistent(const PHLWORKSPACE& ws) {
    if (!ws)
        return false;
#if GLOVIEW_HYPR_ABI_NEW
    if (const auto regular = dynamicPointerCast<Workspace::CRegularWorkspace>(ws))
        return regular->isPersistent();
    return false;
#else
    return ws->isPersistent();
#endif
}

inline PHLWORKSPACE wsQueryById(int id) {
#if GLOVIEW_HYPR_ABI_NEW
    return State::workspaceState()->query().numbered(::Workspace::SWorkspaceNumberedID{static_cast<::Workspace::WorkspaceIDContainer>(id)}).run();
#else
    return State::workspaceState()->query().id(static_cast<WORKSPACEID>(id)).run();
#endif
}

inline bool wsIdExists(int id) {
    return !!wsQueryById(id);
}

inline PHLWORKSPACE wsCreateNumbered(int id, const PHLMONITOR& monitor, std::string displayName = {}, bool isEmpty = true) {
    if (!monitor)
        return nullptr;
#if GLOVIEW_HYPR_ABI_NEW
    return State::workspaceState()->createNumbered(::Workspace::SWorkspaceNumberedID{static_cast<::Workspace::WorkspaceIDContainer>(id)}, monitor, std::move(displayName),
                                                   isEmpty);
#else
    return State::workspaceState()->create(static_cast<WORKSPACEID>(id), monitor->m_id, std::move(displayName), isEmpty);
#endif
}

// --- hyprctl / Socket1 registration ----------------------------------------------------

// Register an exact-match hyprctl command that runs `action` and replies "ok\n".
// New ABI: IPC::Socket1::SCommand. Legacy: SHyprCtlCommand.
template <typename F>
inline void registerExactHyprCtl(HANDLE handle, const char* name, F action) {
#if GLOVIEW_HYPR_IPC_S1
    HyprlandAPI::registerHyprCtlCommand(handle, IPC::Socket1::SCommand{
                                                    .name    = name,
                                                    .match   = IPC::Socket1::COMMAND_MATCH_EXACT,
                                                    .handler = [action](const IPC::Socket1::SRequest&) -> IPC::Socket1::SResponse {
                                                        action();
                                                        return "ok\n";
                                                    },
                                                });
#else
    HyprlandAPI::registerHyprCtlCommand(handle, SHyprCtlCommand{
                                                    .name  = name,
                                                    .exact = true,
                                                    .fn    = [action](eHyprCtlOutputFormat, std::string) -> std::string {
                                                        action();
                                                        return "ok\n";
                                                    },
                                                });
#endif
}

} // namespace gloview
