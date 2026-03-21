{
  description = "coomer — screen zoom/magnifier for Wayland/X11";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      version = pkgs.lib.strings.removeSuffix "\n" (builtins.readFile ./VERSION);

      systemLibs = with pkgs; [
        wayland
        wayland-protocols
        libxkbcommon
        libGL
        libglvnd
        egl-wayland
        libX11
        libXrandr
        dbus
      ];
    in
    {

      # ── Package (nix build) ───────────────────────────────────────────────
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "coomer";
        inherit version;
        src = ./.;

        nativeBuildInputs = with pkgs; [
          clang
          pkg-config
          wayland-scanner
        ];
        buildInputs = systemLibs;

        buildPhase = ''
          make -j$NIX_BUILD_CORES
        '';

        installPhase = ''
          runHook preInstall
          make install PREFIX=$out
          runHook postInstall
        '';

        meta = {
          description = "Screen zoom/magnifier for Wayland and X11";
          homepage = "https://github.com/yuzujr/coomer";
          license = pkgs.lib.licenses.mit;
          platforms = pkgs.lib.platforms.linux;
          mainProgram = "coomer";
        };
      };

      # ── Dev shell (nix develop) ───────────────────────────────────────────
      devShells.${system}.default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
        packages =
          with pkgs;
          [
            xmake
            pkg-config
            wayland-scanner
            clang-tools
            gdb
          ]
          ++ systemLibs;

        shellHook = ''
          echo "coomer dev shell"
          echo "  xmake             — build"
          echo "  make              — build with plain make"
        '';
      };
    };
}
