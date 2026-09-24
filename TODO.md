# TODO

## Dual-view screen-share blackout (reverted)

`plugin:gloview:no_screen_share` is intentionally a **no-op** (default `0`).

Earlier commits hooked Hyprland `saveBufferForMirror` and cleared the share mirror FB
(`bindTempFB` + `glClear`) so screencopy clients saw black while the local monitor kept
live overview tiles. That ABRT'd the compositor on NVIDIA / Hyprland **83cf6a6**
(`libgloview.so` → `CHyprOpenGLImpl::end()`). try/catch around the clear did not help
(may be noexcept or an assert).

**Do not** reintroduce GL screenshare-privacy calls in this plugin until a proven-safe
path exists. Stability > feature.
