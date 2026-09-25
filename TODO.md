# TODO

## Dual-view screen-share blackout (per-tile noscreenshare)

`plugin:gloview:no_screen_share` (default **1**) honors Hyprland `noscreenshare` /
`no_screen_share` on overview **preview tiles** in the screencopy export.

`plugin:gloview:noshare_cover_compat` (default **1**) detects
[noshare-cover](https://github.com/gitscout-bot/noshare-cover) and picks Path A/B
(see `src/noshare_cover_compat.hpp`).

### noshare-cover research (upstream main)

- Hooks `Screenshare::CScreenshareFrame::renderMonitor`; calls previous trampoline
  (`g_hook->m_original`) then paints covers.
- Hyprland prologue hooks are still exclusive — a second `createFunctionHook` on the
  same address typically fails.
- No plugin API / IPC / config to register extra cover rects (overview tile boxes).
- Only covers **real window boxes** for `no_screen_share` windows (image/video media).
- Detect via `g_pPluginSystem` name/path `"noshare-cover"` or mapped `.so` needle.

### Path A — dual-view (we own `renderMonitor`)

- Used when cover is **not** loaded (or `noshare_cover_compat=0`) and the hook succeeds
- Hook `Screenshare::CScreenshareFrame::renderMonitor`
- After the original blit + Hyprland window/layer blackouts, for each overview
  preview tile whose window has `noScreenShare()`, draw a `CRectPassElement`
  with `Colors::BLACK` on **that tile’s box only**
- Other tiles and overview chrome stay visible in the share
- Local interactive overview keeps **live** previews (export path only)

### Path B — coexist with noshare-cover (Arch / v0.56.2 typical)

When cover is detected with compat on, **never** attempt `renderMonitor`:

- Leave noshare-cover’s covers working on real window boxes
- Optionally best-effort hook `::render()` (never enables dual-view)
- While `Screenshare::mgr()->isOutputBeingSSd(monitor)`, overview’s
  `renderWindowLive` draws solid black for ruled tiles into the normal pass
  (**Option B**). The mirror then carries black to demka/Discord
- **Local tradeoff:** ruled tiles are also black on the interactive overview
  for as long as that output is being shared. When not sharing, tiles are live

**Load order:** enable/load noshare-cover **before** gloview so detection sees it at
init. If gloview loads first and takes Path A, cover’s later hook may fail; set
`noshare_cover_compat=1` and reload gloview after cover, or unload/reload both with
cover first.

**Do not** reintroduce full-monitor/full-buffer black, `saveBufferForMirror` +
`bindTempFB`/`glClear`, or RENDER_POST EGL remake — those are wrong or ABRT’d on
NVIDIA / 83cf6a6.

Resolution: exact Itanium mangling via `dlsym` first, then
`findFunctionsByName`. Soft-disable of export hooks is silent (debug log only
when `plugin:gloview:debug_logs=1`); overview still loads. Verified on
Hyprland v0.56.2 and 83cf6a6.
