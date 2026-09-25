# TODO

## Dual-view screen-share blackout (per-tile noscreenshare)

`plugin:gloview:no_screen_share` (default **1**) honors Hyprland `noscreenshare` /
`no_screen_share` on overview **preview tiles** in the screencopy export:

- Hook `Screenshare::CScreenshareFrame::renderMonitor` (fallback: `::render()`)
- After the original blit + Hyprland window/layer blackouts, for each overview
  preview tile whose window has `w->m_ruleApplicator->noScreenShare().valueOrDefault()`,
  draw a `CRectPassElement` with `Colors::BLACK` on **that tile’s box only**
  (rounding matches the preview)
- Other tiles and overview chrome stay visible in the share
- Local interactive overview keeps **live** previews (export path only)
- Runs only inside `beginRender(RENDER_MODE_TO_BUFFER / FULL_FAKE)` for share clients

**Do not** reintroduce full-monitor/full-buffer black, `saveBufferForMirror` +
`bindTempFB`/`glClear`, or RENDER_POST EGL remake — those are wrong or ABRT’d on
NVIDIA / 83cf6a6.

Resolution: exact Itanium mangling via `dlsym` first
(`_ZN11Screenshare17CScreenshareFrame13renderMonitorEv`), then
`findFunctionsByName`. If `renderMonitor` is missing or already hooked (e.g.
noshare-cover), fall back to `CScreenshareFrame::render()`. Soft-disable is silent
(debug log only when `plugin:gloview:debug_logs=1`); overview still loads.
Verified exported on Hyprland v0.56.2 and 83cf6a6.
