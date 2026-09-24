#pragma once

// Thin helpers for Hyprland @ 83cf6a6 (workspace state refactor).
// workspaceState() is an inline wrapper around Workspace::state() in headers;
// plugins must call through it (or Workspace::state()) so they do not take a
// dependency on the removed non-inline State::workspaceState() export.

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/workspace/AbstractWorkspace.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>
#include <hyprland/src/workspace/RegularWorkspace.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace gloview {

inline bool wsIsSpecial(const PHLWORKSPACE& ws) {
    return ws && ws->type() == Workspace::eWorkspaceType::SPECIAL;
}

// Numbered workspace id as int, or 0 when the workspace is named/special/unnumbered.
inline int wsNumericId(const PHLWORKSPACE& ws) {
    if (!ws)
        return 0;
    if (const auto n = ws->numberedID())
        return static_cast<int>(*n);
    return 0;
}

inline bool wsIsNumberedPositive(const PHLWORKSPACE& ws) {
    return wsNumericId(ws) > 0 && !wsIsSpecial(ws);
}

inline const std::string& wsDisplayName(const PHLWORKSPACE& ws) {
    static const std::string empty;
    return ws ? ws->displayName() : empty;
}

inline void wsSetPersistent(const PHLWORKSPACE& ws, bool persistent) {
    if (const auto regular = dynamicPointerCast<Workspace::CRegularWorkspace>(ws))
        regular->setPersistent(persistent);
}

inline ::Workspace::SWorkspaceNumberedID wsNumbered(int id) {
    return ::Workspace::SWorkspaceNumberedID{static_cast<::Workspace::WorkspaceIDContainer>(id)};
}

inline PHLWORKSPACE wsQueryById(int id) {
    return State::workspaceState()->query().numbered(wsNumbered(id)).run();
}

inline bool wsIdExists(int id) {
    return !!wsQueryById(id);
}

inline PHLWORKSPACE wsCreateNumbered(int id, const PHLMONITOR& monitor, std::string displayName = {}, bool isEmpty = true) {
    return State::workspaceState()->createNumbered(wsNumbered(id), monitor, std::move(displayName), isEmpty);
}


inline bool wsIsPersistent(const PHLWORKSPACE& ws) {
    if (const auto regular = dynamicPointerCast<Workspace::CRegularWorkspace>(ws))
        return regular->isPersistent();
    return false;
}

} // namespace gloview
