{
  description = "GloView — a macOS Mission Control-style overview plugin for Hyprland";

  inputs = {
    # Default pin: Hyprland 83cf6a6 (workspace/window/IPC refactor). The plugin also
    # compiles against Hyprland v0.56.2 via compile-time __has_include detection in
    # src/hypr_compat.hpp — override the input (or `follows` your compositor) to pick ABI:
    #
    #   # newer / nixoser-style:
    #   inputs.hyprland.url = "github:hyprwm/Hyprland?rev=83cf6a6ed540dc37808434259c6a3ba663de9616";
    #   # Arch / upstream-typical:
    #   inputs.hyprland.url = "github:hyprwm/Hyprland?ref=v0.56.2";
    #   # OR always match your system Hyprland:
    #   inputs.gloview.inputs.hyprland.follows = "hyprland";
    #
    # Single package `gloview` — the flake's hyprland input decides the ABI. No separate
    # gloview-legacy output; override-input / follows is the switch.
    hyprland.url = "github:hyprwm/Hyprland?rev=83cf6a6ed540dc37808434259c6a3ba663de9616";
    nixpkgs.follows = "hyprland/nixpkgs";
    systems.follows = "hyprland/systems";
  };

  outputs = {
    self,
    hyprland,
    nixpkgs,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system: import nixpkgs {inherit system;});
  in {
    packages = eachSystem (system: let
      pkgs = pkgsFor.${system};
      # Build against the exact Hyprland package from the flake input so the plugin ABI
      # matches that pin. On v0.56.2, Hyprland's CMake still asks for `glaze 7...<8` while
      # its nixpkgs ships glaze 8 — relax the constraint only when the pattern is present
      # (83cf6a6 already dropped it; --replace-fail would fail there).
      hyprlandPkg = hyprland.packages.${system}.hyprland.overrideAttrs (old: {
        postPatch =
          (old.postPatch or "")
          + ''
            if grep -q 'glaze 7\.\.\.<8' CMakeLists.txt 2>/dev/null; then
              substituteInPlace CMakeLists.txt --replace-fail "glaze 7...<8" "glaze"
            fi
          '';
      });
    in {
      # mkHyprlandPlugin now lives in nixpkgs (pkgs.hyprlandPlugins.mkHyprlandPlugin), not in
      # the Hyprland flake's `lib`. It is built on hyprland.stdenv.mkDerivation and auto-adds
      # pkg-config + hyprland + hyprland.buildInputs. We `.override` the whole scope so BOTH
      # the build stdenv and the hyprland buildInput are the EXACT pinned Hyprland (ABI must
      # match the running compositor), not nixpkgs' possibly-skewed hyprland.
      gloview = (pkgs.hyprlandPlugins.override {hyprland = hyprlandPkg;}).mkHyprlandPlugin {
        pluginName = "gloview";
        version = "0.3.0";
        src = ./.;

        nativeBuildInputs = [pkgs.cmake pkgs.pkg-config];
        # Hyprland's own build inputs are propagated by mkHyprlandPlugin; we only add Lua
        # (for the gloview.* Lua config functions). luajit is what Hyprland links.
        buildInputs = [pkgs.luajit];

        # The build emits `gloview.so` (CMake PREFIX ""), but Home Manager's
        # `wayland.windowManager.hyprland.plugins = [ pkg ]` looks for `lib<pname>.so`.
        # Symlink it so the idiomatic one-liner install works with no extra config.
        postInstall = ''
          ln -sf gloview.so "$out/lib/libgloview.so"
        '';

        meta = {
          description = "macOS Mission Control-style overview for Hyprland";
          homepage = "https://github.com/gitscout-bot/gloview";
          license = lib.licenses.gpl3Plus;
          platforms = lib.platforms.linux;
        };
      };

      default = self.packages.${system}.gloview;
    });

    devShells = eachSystem (system: let
      pkgs = pkgsFor.${system};
    in {
      default = pkgs.mkShell {
        # `nix develop` gives a shell that can configure+build the plugin against the
        # pinned Hyprland (cmake -S . -B build && cmake --build build).
        inputsFrom = [self.packages.${system}.gloview];
        packages = [pkgs.clang-tools];
      };
    });

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
