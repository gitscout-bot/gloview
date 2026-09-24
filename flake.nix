{
  description = "GloView — a macOS Mission Control-style overview plugin for Hyprland";

  inputs = {
    # Pin Hyprland to the compositor ABI this fork targets. Commit 83cf6a6 (reported as
    # v0.56.0-style / nix build ABI 83cf6a6…_aq_0.15_…) inlined State::workspaceState() and
    # refactored workspace create/query onto State::Workspace::CState. Downstream MUST set
    # `inputs.gloview.inputs.hyprland.follows = "hyprland"` so the plugin links against the
    # EXACT same Hyprland as the running compositor.
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
      # Upstream's tagged flake package does not build in a nix sandbox as-is: Hyprland
      # 0.56.2's CMake asks for `find_package(glaze 7...<8)` while its own nixpkgs pin ships
      # glaze 8, so the check fails and the FetchContent fallback tries to `git clone` glaze
      # (no git, no network in the sandbox) -> "could not find git for clone of glaze".
      # nixpkgs' own hyprland 0.56.2 relaxes that exact constraint and builds fine against
      # glaze 8, so mirror its patch here instead of pinning an older Hyprland.
      hyprlandPkg = hyprland.packages.${system}.hyprland.overrideAttrs (old: {
        postPatch =
          (old.postPatch or "")
          + ''
            substituteInPlace CMakeLists.txt --replace-fail "glaze 7...<8" "glaze"
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
          homepage = "https://github.com/fedsfarm/gloview";
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
