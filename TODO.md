# TODO

## Dual-view screen-share blackout

`plugin:gloview:no_screen_share` (default **1**) blacks the screencopy export while
overview is open, mirroring Hyprland’s own noscreenshare path:

- Hook `Screenshare::CScreenshareFrame::renderMonitor`
- After the original blit + window/layer blackouts, draw a full-buffer
  `CRectPassElement` with `Colors::BLACK` via `g_pHyprRenderer->draw(...)`
- Runs only inside `beginRender(RENDER_MODE_TO_BUFFER / FULL_FAKE)` for share clients
- Local swapchain / mainFB untouched → dual-view

**Do not** reintroduce `saveBufferForMirror` + `bindTempFB`/`glClear` or RENDER_POST EGL
remake — those ABRT’d on NVIDIA / 83cf6a6.

Residual risk: if `findFunctionsByName("renderMonitor")` fails to match the Screenshare
symbol (ABI rename), blackout is soft-disabled with a notification; overview still loads.
