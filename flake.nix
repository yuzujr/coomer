{
  description = "coomer — screen zoom/magnifier for Wayland/X11";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs   = nixpkgs.legacyPackages.${system};

      systemLibs = with pkgs; [
        wayland wayland-protocols libxkbcommon
        libGL libglvnd egl-wayland
        libX11 libXrandr dbus
      ];
    in {

      # ── Package (nix build) ───────────────────────────────────────────────
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname   = "coomer";
        version = "0.1.0";
        src     = ./.;

        nativeBuildInputs = with pkgs; [
          clang
          pkg-config
          wayland-scanner
        ];
        buildInputs = systemLibs;

        buildPhase = ''
          # stb is header-only with no .pc file — expose via CPATH.
          export CPATH="${pkgs.stb}/include/stb''${CPATH:+:$CPATH}"
          make -j$NIX_BUILD_CORES
        '';

        installPhase = ''
          runHook preInstall
          make install PREFIX=$out
          runHook postInstall
        '';

        meta = {
          description = "Screen zoom/magnifier for Wayland and X11";
          homepage    = "https://github.com/yuzujr/coomer";
          license     = pkgs.lib.licenses.mit;
          platforms   = pkgs.lib.platforms.linux;
          mainProgram = "coomer";
        };
      };

      # ── Dev shell (nix develop) ───────────────────────────────────────────
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          xmake pkg-config wayland-scanner
          clang clang-tools gdb
        ] ++ systemLibs;
        shellHook = ''
          echo "coomer dev shell"
          echo "  xmake             — build (downloads stb)"
          echo "  xmake f --nix=y   — build using nix:: stb"
          echo "  make              — build with plain make"
        '';
      };
    };
}
