# TODO

## Dual-view screen-share blackout (per-tile noscreenshare)

`plugin:gloview:no_screen_share` (default **1**) honors Hyprland `noscreenshare` /
`no_screen_share` on overview **preview tiles** in the screencopy export.

### Path A — dual-view (we own `renderMonitor`)

- Hook `Screenshare::CScreenshareFrame::renderMonitor`
- After the original blit + Hyprland window/layer blackouts, for each overview
  preview tile whose window has `noScreenShare()`, draw a `CRectPassElement`
  with `Colors::BLACK` on **that tile’s box only**
- Other tiles and overview chrome stay visible in the share
- Local interactive overview keeps **live** previews (export path only)

### Path B — coexist with noshare-cover (Arch / v0.56.2 typical)

noshare-cover owns the single `renderMonitor` trampoline. Falling back to
`CScreenshareFrame::render()` alone is unreliable on some 0.56.2 builds (tile
blackout after `render()` returns may not stick). Instead:

- Do **not** fight for `renderMonitor` (leave noshare-cover’s covers working)
- Optionally best-effort hook `::render()` (never enables dual-view)
- While `Screenshare::mgr()->isOutputBeingSSd(monitor)`, overview’s
  `renderWindowLive` draws solid black for ruled tiles into the normal pass
  (**Option B**). The mirror then carries black to demka/Discord
- **Local tradeoff:** ruled tiles are also black on the interactive overview
  for as long as that output is being shared. When not sharing, tiles are live

**Do not** reintroduce full-monitor/full-buffer black, `saveBufferForMirror` +
`bindTempFB`/`glClear`, or RENDER_POST EGL remake — those are wrong or ABRT’d on
NVIDIA / 83cf6a6.

Resolution: exact Itanium mangling via `dlsym` first, then
`findFunctionsByName`. Soft-disable of export hooks is silent (debug log only
when `plugin:gloview:debug_logs=1`); overview still loads. Verified on
Hyprland v0.56.2 and 83cf6a6.
