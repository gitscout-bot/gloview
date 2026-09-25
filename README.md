# GloView
[![license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/fedsfarm/gloview/blob/main/LICENSE) [![Matrix](https://img.shields.io/badge/Matrix-Join%20chat-green?logo=matrix&logoColor=white)](https://escape.feds.farm/#main:feds.farm) [![Donate](https://img.shields.io/badge/Donate-XMR%20%C2%B7%20BTC%20%C2%B7%20ETH-orange?logo=monero&logoColor=white&labelColor=555)](#donate)

https://github.com/user-attachments/assets/0a3d812a-eae0-4ca5-8698-7a006e540857

A better macOS Mission Control-style overview plugin for Hyprland

## Install

### Dual Hyprland ABI

One codebase builds against both:

| Target | Hyprland | Typical use |
|---|---|---|
| **Legacy** | **v0.56.2** | Arch `hyprpm`, upstream-style pins |
| **New** | **83cf6a6…** (`83cf6a6ed540dc37808434259c6a3ba663de9616`) | nixoser / post–workspace-window-IPC refactor |

Compile-time detection lives in `src/hypr_compat.hpp` (`__has_include` on
`desktop/view/window/Window.hpp` and `ipc/s1/S1.hpp`). Wrappers cover workspace
create/query, window mapped/title/appid/presentation, modifiers, hyprctl IPC, and
noscreenshare accessors. The flake exposes a single `gloview` package; the
`hyprland` input decides which ABI you link against.

### Via hyprpm (Arch / system Hyprland)

Rebuild against **your installed Hyprland headers** so the `.so` matches the
running compositor (0.56.2 on current Arch packages):

```sh
# this fork (dual ABI) — or upstream fedsfarm/gloview on stock 0.56.2 only
hyprpm add https://github.com/gitscout-bot/gloview
hyprpm update
hyprpm enable gloview
```

If enable fails with missing headers such as `ipc/s1/S1.hpp` or
`desktop/view/window/Window.hpp`, you were on a new-only tree against old
headers — pull latest `main` (this dual-ABI tree) and `hyprpm update` again.
`hyprpm` compiles with the system Hyprland pkg-config; no flake override needed.

### Arch (AUR)

```sh
yay -S gloview
```

### NixOS / Home Manager

**Always** make the plugin follow your compositor's Hyprland input so the ABI
matches (`follows` is the switch between 0.56.2 and 83cf6a6):

```nix
inputs.gloview.url = "github:gitscout-bot/gloview";
# Required: same Hyprland derivation as the running compositor
inputs.gloview.inputs.hyprland.follows = "hyprland";
```

With `follows`, the flake passes that Hyprland derivation through unchanged. Updating
`gloview` and switching Home Manager therefore builds only the plugin `.so`; it does
not create or rebuild a second Hyprland derivation.

```nix
wayland.windowManager.hyprland = {
  enable = true;
  plugins = [ inputs.gloview.packages.${pkgs.system}.gloview ];
  settings.bind = [ "SUPER, TAB, gloview:toggle" ];
};
```

Flake default pin is `83cf6a6…`. To build explicitly against either side:

```sh
# new (default lock / nixoser)
nix build .#gloview -L

# legacy v0.56.2 (Arch-typical)
nix build .#gloview --override-input hyprland github:hyprwm/Hyprland/v0.56.2 -L
```

Or set `inputs.hyprland.url` in a consumer flake to
`github:hyprwm/Hyprland?ref=v0.56.2` or
`github:hyprwm/Hyprland?rev=83cf6a6ed540dc37808434259c6a3ba663de9616`.

## Manual build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Produces `build/gloview.so`. The ABI must match the running Hyprland exactly —
build against the same headers, or a version skew gives a `.so` that crashes on
load. While iterating, `cmake --build build --target reload` rebuilds and
hot-reloads into the running Hyprland.

## Usage

Dispatchers: `gloview:toggle`, `gloview:open`, `gloview:close`, `gloview:desktop`,
`gloview:allworkspaces` (the all-workspaces "expo" view — opens into it if closed)
Or `hyprctl gloview` / `gloviewclose` / `gloviewdesktop` / `gloviewall`

Workspace navigation: `gloview:next`, `gloview:prev`, `gloview:setworkspace <id>`
(or `hyprctl gloviewnext` / `gloviewprev`). **Inside** the overview they behave like
`tab` and the scroll wheel: they move the *displayed* workspace and commit it to the
live desktop on close. **Outside** it they switch the live desktop right away, walking
the same ordered list the strip shows — so one pair of binds drives workspaces
everywhere. With `dynamic_workspaces` on, stepping past the last populated workspace
lands you on a fresh empty one and the workspace you left behind is reaped if you
emptied it (`gloview:setworkspace` never creates; it only switches to a workspace that
already exists on the monitor).

Lua:

```lua
hl.bind("SUPER + TAB", hl.plugin.gloview.toggle)
hl.bind("SUPER + SHIFT + TAB", hl.plugin.gloview.desktop)
hl.bind("SUPER + CTRL + TAB", hl.plugin.gloview.allworkspaces)

hl.bind("SUPER + bracketright", hl.plugin.gloview.next)
hl.bind("SUPER + bracketleft", hl.plugin.gloview.prev)
hl.bind("SUPER + 2", function() hl.plugin.gloview.setworkspace(2) end)
```

```ini
bind = SUPER, TAB, gloview:toggle
bind = SUPER SHIFT, TAB, gloview:desktop
bind = SUPER CTRL, TAB, gloview:allworkspaces

bind = SUPER, bracketright, gloview:next
bind = SUPER, bracketleft, gloview:prev
bind = SUPER, 2, gloview:setworkspace, 2
```

## Config

All keys live under `plugin:gloview:*`. Colors are `0xAARRGGBB` integers.

- **`rows`** (default) — macOS-like: previews keep their aspect ratio and are packed into balanced rows, with the row count chosen to make the previews as large as possible. Reads spatially like the real desktop.
- **`grid`** — uniform cells, one preview centered in each.
- **`natural`** — keeps each window's real on-screen position, uniformly scaling the whole arrangement to fit.

| Option | Type | Default | Description |
|---|---|---|---|
| `layout` | `rows` \| `grid` \| `natural` | `rows` | Main-area preview layout engine |
| `gap` | int (px) | `34` | Min spacing between window previews |
| `padding` | int (px) | `80` | Left/right outer margin of the preview area |
| `padding_top` | int (px) | `40` | Extra gap between the strip and the previews |
| `padding_bottom` | int (px) | `70` | Bottom outer margin |
| `max_scale` | float | `1.0` | Never enlarge a preview past real size × this |
| `preview_filter` | `box4` \| `box16` \| `linear` | `box4` | Scale-aware GPU downsampling for reduced previews: `box4` uses a 2×2 kernel, `box16` uses a smoother but costlier 4×4 kernel, and `linear` restores Hyprland's normal single-sample path |
| `duration` | int (ms) | `360` | Open/close animation length |
| `preview_round` | int (px) | `12` | Window preview corner radius |
| `blur` | float `0`..`1` | `1.0` | Backdrop + strip blur strength (`0` = off; fractions allowed) |
| `switch_animation` | bool (0/1) | `1` | Slide the previews sideways when the displayed workspace changes (the outgoing set leaves as the incoming one arrives) |
| `switch_duration` | int (ms) | `260` | Length of that slide |
| `move_animation` | bool (0/1) | `1` | A window dropped on a workspace card keeps flying into it, shrinking into its slot; the card holds off drawing it until it lands |
| `move_duration` | int (ms) | `240` | Length of that flight |
| `anchor` | `top` \| `bottom` \| `left` \| `right` | `top` | Edge the workspace strip attaches to |
| `strip_offset` | int (px) | `0` | Inset from the anchored edge (0 = flush, no gap) |
| `strip_height` | int (px) | `150` | Strip band thickness, label included |
| `strip_margin` | int (px) | `22` | Padding around the strip |
| `strip_gap` | int (px) | `18` | Spacing between workspace cards |
| `strip_card_round` | int (px) | `10` | Workspace card corner radius |
| `backdrop_color` | color | `0x73070a10` | Dim + blur fill over the desktop |
| `strip_band_color` | color | `0x24ffffff` | Band behind the cards |
| `strip_card_color` | color | `0x3a0e131c` | Inactive workspace card fill |
| `strip_active_color` | color | `0x4d1c2c44` | Active workspace card fill |
| `strip_active_border` | color | `0xf0ffffff` | Active card border |
| `strip_active_border_size` | int (px) | `2` | Active card border thickness |
| `strip_hover_border` | color | `0x80ffffff` | Hovered card border |
| `strip_hover_border_size` | int (px) | `2` | Hovered card border thickness |
| `strip_plus_color` | color | `0xd0eef4ff` | The "+" glyph |
| `preview_bg` | color | `0xff14181f` | Opaque backing drawn under a preview's live surface |
| `shadow_color` | color | `0x70000000` | Window preview drop shadow |
| `hover_border` | color | `0xf0ffffff` | Hovered window preview border |
| `hover_border_size` | int (px) | `3` | Hovered preview border thickness |
| `select_border` | color | `0xf066ccff` | Keyboard-selected preview border |
| `select_border_size` | int (px) | `3` | Keyboard-selected preview border thickness |
| `focus_follows_mouse` | bool (0/1) | `1` | Keyboard selection tracks the hovered preview |
| `scroll_switches_workspace` | bool (0/1) | `1` | Wheel over the main area steps prev/next workspace |
| `passthrough_keys` | bool (0/1) | `1` | Let keys the overview doesn't use reach Hyprland (keybinds keep working) |
| `key_close` | key names | `escape` | Keys that dismiss |
| `key_next_workspace` | key names | `tab` | Cycle the displayed workspace forward (wraps); `""` to disable. Held modifiers match exactly, so e.g. a `SUPER+Tab` toggle bind still passes through and closes |
| `key_prev_workspace` | key names | `shift+tab` | Cycle the displayed workspace backward (`mod+key` combos supported: `shift`/`ctrl`/`alt`/`super`) |
| `key_activate` | key names | `enter` | Keys that focus the selected preview |
| `key_close_window` | key names | `d` | Keys that close the selected preview's window (overview stays open); `""` to disable |
| `key_left` / `key_right` / `key_up` / `key_down` | key names | `left` / `right` / `up` / `down` | Move the keyboard selection (e.g. set `h`/`l`/`k`/`j` for vim nav) |
| `key_desktop` | key names | `shift` | Flip canvas↔grid |
| `key_all_workspaces` | key names | `a` | Toggle the all-workspaces (expo) view; `""` to disable |
| `key_workspace` | key names | `1,2,3,4,5,6,7,8,9,0` | Each key switches to the Nth strip card's workspace, for real (slot position = card index) |
| `exit_on_click` | bool (0/1) | `1` | Click on empty space dismisses the overview |
| `exit_on_switch` | bool (0/1) | `0` | Dismiss when the live workspace changes underneath (e.g. a keybind) |
| `show_all_workspaces` | bool (0/1) | `0` | Main area shows every window on the monitor (expo), not just the displayed workspace. Toggle live with `gloview:allworkspaces`, the `key_all_workspaces` key, or the strip's "All" card. Picking a preview here follows it: the overview closes onto that window's workspace |
| `show_empty` | bool (0/1) | `1` | Keep empty workspaces as strip cards. Has no effect while `dynamic_workspaces` is on (which it is by default) — set that to `0` to get the old always-list-everything strip back |
| `dynamic_workspaces` | bool (0/1) | `1` | GNOME/hyprnome-style workspaces: only populated workspaces are listed and the strip ends in one empty card (drawn as a workspace, not a `+`). Stepping onto it or dropping a window into it creates it; the *next* empty card only appears once a window actually lands there, so you never see two blank desktops in a row, and emptying it again takes the extra card back away. Workspaces you empty drop off the strip. Forces `show_empty` off. Pair with `autodelete_empty` if you also want workspaces your config pins to be destroyed, not just hidden |
| `autodelete_empty` | bool (0/1) | `1` | Let Hyprland reap empty workspaces this monitor still pins. Hyprland already destroys unpinned empties on its own, so this only affects ones held by a `persistent:true` rule (and gloview's own abandoned trailing workspace) — the "empty ones get deleted automatically" half of GNOME-style workspaces. **Releasing a persistent workspace lasts until your next config reload — set this to `0` if you keep `persistent:true` workspaces you want left alone.** Skips the displayed workspace, anything visible on any monitor, workspaces holding any window (mapped or not), scratchpads, named workspaces, and other monitors' workspaces |
| `show_workspace_labels` | bool (0/1) | `1` | Workspace names above the strip cards. Off frees the label band, so the cards grow into it |
| `show_window_labels` | bool (0/1) | `1` | Window title pill under a hovered/selected preview |
| `show_special` | bool (0/1) | `0` | Include the special (scratchpad) workspace as a strip card |
| `strip_all_card` | bool (0/1) | `0` | Show a leading "All workspaces" card on the strip that toggles the expo view |
| `drag_to_swap` | bool (0/1) | `1` | Grid mode: dropping a preview onto another swaps the two windows' places |
| `switch_on_drop` | bool (0/1) | `0` | Dropping a window on a card also follows it to that workspace |
| `switch_on_new_workspace` | bool (0/1) | `1` | Clicking `+` follows the display to the new workspace |
| `close_button_color` | color | `0xe6e23b3b` | Desktop-mode `✕` close-button fill |
| `no_screen_share` | bool (0/1) | `1` | Honor Hyprland `noscreenshare` / `no_screen_share` on overview **preview tiles** in share: black those boxes only; other tiles + chrome stay visible. Dual-view (local live) via Path A (`ScreenshareFrame::renderMonitor`) or Path C (noshare-cover extra-rect API); old cover without API → Option B (local black while shared). Not mirror-FB clears |
| `noshare_cover_compat` | bool (0/1) | `1` | Detect [noshare-cover](https://github.com/gitscout-bot/noshare-cover) and prefer Path C / Path B over fighting for `ScreenshareFrame::renderMonitor`. Set `0` to always attempt dual-view Path A even if cover is loaded. See *Screen share + noshare-cover* below |
| `hide_top_layers` | bool (0/1) | `0` | Fade out Top layer surfaces (bars, e.g. Waybar) while open |
| `hide_overlay_layers` | bool (0/1) | `0` | Fade out Overlay layer surfaces (popups/notifications) while open |
| `above_namespaces` | string | `""` | Comma/space list of layer namespaces to draw *above* the overview (trailing `*` glob; a namespace containing `aboveoverview` always qualifies) |
| `debug_logs` | bool (0/1) | `0` | Verbose `[gloview]` logging |

`top`/`bottom` give a horizontal strip, `left`/`right` a vertical one. `anchor`
supersedes the older `bar_position` (top/bottom only); set `anchor` and it wins.

### Lua

```lua
    hl.config({
        plugin = {
            gloview = {
                layout         = "rows",
                gap            = 34,
                padding        = 80,
                padding_top    = 40,
                padding_bottom = 70,
                max_scale      = 1.0,
                preview_filter = "box4",
                duration       = 200,
                preview_round  = 12,
                blur           = 1,

                switch_animation = 1,
                switch_duration  = 260,
                move_animation   = 1,
                move_duration    = 240,

                anchor           = "top",
                strip_offset     = 0,
                strip_height     = 150,
                strip_margin     = 22,
                strip_gap        = 18,
                strip_card_round = 10,

                focus_follows_mouse       = 1,
                scroll_switches_workspace = 1,
                passthrough_keys          = 1,
                exit_on_click             = 1,
                exit_on_switch            = 0,

                key_close     = "escape",
                key_next_workspace = "tab",
                key_prev_workspace = "shift+tab",
                key_activate  = "enter",
                key_close_window = "d",
                key_left      = "left",
                key_right     = "right",
                key_up        = "up",
                key_down      = "down",
                key_desktop   = "shift",
                key_all_workspaces = "a",
                key_workspace = "1,2,3,4,5,6,7,8,9,0",

                show_all_workspaces     = 0,
                show_empty              = 1,
                dynamic_workspaces      = 1,
                autodelete_empty        = 1,
                show_workspace_labels   = 1,
                show_window_labels      = 1,
                show_special            = 0,
                strip_all_card          = 1,
                drag_to_swap            = 1,
                switch_on_drop          = 0,
                switch_on_new_workspace = 1,

                no_screen_share       = 1,  -- black noscreenshare preview tiles in share (Path A/C dual-view)
                noshare_cover_compat  = 1,  -- Path C (extra-rect API) or B when noshare-cover is loaded
                hide_top_layers       = 0,
                hide_overlay_layers = 0,
                above_namespaces    = "",
                debug_logs = 0,

                select_border_size  = 3,
                select_border       = 0xf066ccff,
                close_button_color  = 0xe6e23b3b,
                backdrop_color      = 0x73070a10,
                strip_band_color    = 0x24ffffff,
                strip_card_color    = 0x3a0e131c,
                strip_active_color  = 0x4d1c2c44,
                strip_active_border = 0xf0ffffff,
                strip_hover_border  = 0x80ffffff,
                strip_active_border_size = 2,
                strip_hover_border_size  = 2,
                strip_plus_color    = 0xd0eef4ff,
                preview_bg          = 0xff14181f,
                shadow_color        = 0x70000000,
                hover_border        = 0xf0ffffff,
                hover_border_size   = 3,
            },
        },
    })
```

### hyprland.conf

```ini
plugin {
    gloview {
        layout = rows
        gap = 34
        padding = 80
        padding_top = 40
        padding_bottom = 70
        max_scale = 1.0
        preview_filter = box4
        duration = 200
        preview_round = 12
        blur = 1

        switch_animation = 1
        switch_duration  = 260
        move_animation   = 1
        move_duration    = 240

        anchor = top
        strip_offset = 0
        strip_height = 150
        strip_margin = 22
        strip_gap = 18
        strip_card_round = 10

        focus_follows_mouse       = 1
        scroll_switches_workspace = 1
        passthrough_keys          = 1
        exit_on_click             = 1
        exit_on_switch            = 0

        key_close     = escape
        key_next_workspace = tab
        key_prev_workspace = shift+tab
        key_activate  = enter
        key_close_window = d
        key_left      = left
        key_right     = right
        key_up        = up
        key_down      = down
        key_desktop   = shift
        key_all_workspaces = a
        key_workspace = 1,2,3,4,5,6,7,8,9,0

        show_all_workspaces     = 0
        show_empty              = 1
        dynamic_workspaces      = 1
        autodelete_empty        = 1
        show_workspace_labels   = 1
        show_window_labels      = 1
        show_special            = 0
        strip_all_card          = 0
        drag_to_swap            = 1
        switch_on_drop          = 0
        switch_on_new_workspace = 1

        hide_top_layers     = 0
        hide_overlay_layers = 0
        above_namespaces    =
        debug_logs = 0

        select_border_size  = 3
        select_border       = 0xf066ccff
        close_button_color  = 0xe6e23b3b
        backdrop_color      = 0x73070a10
        strip_band_color    = 0x24ffffff
        strip_card_color    = 0x3a0e131c
        strip_active_color  = 0x4d1c2c44
        strip_active_border = 0xf0ffffff
        strip_hover_border  = 0x80ffffff
        strip_active_border_size = 2
        strip_hover_border_size  = 2
        strip_plus_color    = 0xd0eef4ff
        preview_bg          = 0xff14181f
        shadow_color        = 0x70000000
        hover_border        = 0xf0ffffff
        hover_border_size   = 3
    }
}
```

## Contribute

If you want to contribute, you can check out the feature requests by users in the Discussions tab, or implement your own feature

It will be merged if it builds and fits. If you aren't sure, you can contact me (see below)

AI code is allowed if it's submitted and tested by a human

## Donate

#### XMR:
`42uxSBp4aMyTAsPCMGEwHvJyGpemr1c7kdjtFsD5tnEsU7XsnYMjseyXBzLWHkruSWFGbQWagsh31bBRdU7vDNUBAzm1Mo4`  

#### BTC:
`bc1p2xkwf9elq8wgajtq2cc6zthuh4k998tgnk6365cnjqgal7mpd09q4jtfq8`

#### ETH (ERC-20):
`0xBD636eBD3a6b9F046930101657459E90DA370e81`  

---

Email [root@feds.farm](mailto:root@feds.farm) or DM [@root:feds.farm](https://escape.feds.farm/#@root:feds.farm) on Matrix if you want your donation to be visible

## Screen share + noshare-cover

[noshare-cover](https://github.com/gitscout-bot/noshare-cover) replaces Hyprland’s
`no_screen_share` black boxes with an image/video on the **share** path. It hooks
`ScreenshareFrame::renderMonitor` (Hyprland’s function trampoline is exclusive — two
plugins cannot both own that hook) and paints covers on real window boxes. It also
exports a small C API so other plugins can register **extra** cover rectangles
(overview tile boxes):

```c
void noshare_cover_clear_extra_rects(void);
void noshare_cover_add_extra_rect(int monitor_id, double x, double y, double w, double h, double rounding);
```

`monitor_id` is Hyprland’s monitor id (`hyprctl monitors`). `x,y,w,h,rounding` are
**global layout pixels** (same space as window position/size). Rects persist until
`clear`; they are drawn opaque black after window covers. `rounding=0` is sharp.

**RTLD_LOCAL caveat:** Hyprland loads plugins with `RTLD_LOCAL`, so
`dlsym(RTLD_DEFAULT, …)` will **not** see those symbols. gloview resolves the mapped
`.so` via `dl_iterate_phdr` (needle `noshare-cover` / `libnoshare-cover.so`), then
`dlopen(path, RTLD_LAZY | RTLD_NOLOAD)` + `dlsym`. See `src/noshare_cover_compat.hpp`.

gloview (`plugin:gloview:noshare_cover_compat`, default **1**) therefore:

1. **Detects** noshare-cover via Hyprland’s plugin list or a mapped `.so`, and **binds**
   the extra-rect API when present (null-checked; never links noshare-cover at build time).
2. **Path C** (cover loaded + API bound): never fights for `renderMonitor`. Each overview
   frame `clear()`s then `add_extra_rect` for every noscreenshare preview tile (main grid,
   strip cards, drag/fly/slide). **Dual-view:** local overview keeps live tiles; share
   sees black tile boxes via noshare-cover. Clears on overview close / plugin unload.
3. **Path B** (cover loaded, API missing — older cover): leave `renderMonitor` alone;
   while `isOutputBeingSSd`, overview draws solid black for ruled tiles in the normal
   pass so the mirror carries black. **Local tradeoff:** those tiles are also black on
   the interactive overview while that output is shared.
4. **Path A** (cover not loaded, or compat off): gloview hooks `renderMonitor` for
   dual-view when the trampoline is free.

**Load order:** load/enable noshare-cover **before** gloview so detection/bind succeed at
init. If gloview took Path A first, unload/reload with cover first, or keep compat on
and reload gloview after cover.

Set `noshare_cover_compat = 0` only if you intentionally want gloview to fight for
Path A (hook may still fail silently → Path B). Soft failures never ABRT or orange-notify.

## TODO / known limitations

- **Screen-share tile blackout** (`plugin:gloview:no_screen_share`, default 1): Path A/C
  dual-view or Path B fallback — see *Screen share + noshare-cover* above. Not full-buffer
  clear / mirror-FB / RENDER_POST EGL.

