{
  description = "coomer — screen zoom/magnifier for Wayland/X11";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs   = nixpkgs.legacyPackages.${system};

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

      # \u2500\u2500 Pre-generate glad v1 headers (GL 3.3 core) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
      # python3Packages.glad is the original glad v1 generator.
      # Output layout matching what coomer includes (<glad/glad.h>):
      #   $out/include/glad/glad.h
      #   $out/include/KHR/khrplatform.h
      #   $out/src/glad.c
      gladGL33 = pkgs.stdenv.mkDerivation {
        name = "glad-gl33-core";
        dontUnpack = true;
        nativeBuildInputs = [ pkgs.python3Packages.glad ];
        buildPhase = ''
          python3 -m glad \
            --profile core \
            --api gl=3.3 \
            --generator c \
            --spec gl \
            --out-path $out
        '';
        installPhase = "true";
      };

    in {

      # \u2500\u2500 Package \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname   = "coomer";
        version = "0.1.0";
        src     = ./.;

        nativeBuildInputs = with pkgs; [
          xmake
          pkg-config
          wayland-scanner
          clang
        ];

        buildInputs = systemLibs;

        buildPhase = ''
          export HOME=$TMPDIR
          export XMAKE_GLOBALDIR=$TMPDIR/.xmake

          # stb is header-only (no .pc file); expose via CPATH.
          export CPATH="${pkgs.stb}/include''${CPATH:+:$CPATH}"

          # glad_includedir points to pre-generated glad v1 headers;
          # xmake will compile $glad_includedir/../src/glad.c directly.
          xmake f --yes -p linux -a x86_64 \
            --mode=release \
            --wayland=y --x11=y --portal=y \
            --glad_includedir=${gladGL33}/include

          xmake build --yes
        '';

        installPhase = ''
          runHook preInstall
          install -Dm755 build/linux/x86_64/release/coomer $out/bin/coomer
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

      # \u2500\u2500 Dev shell \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
      # Usage:
      #   nix develop
      #   xmake f --nix=y   # stb from nix::, glad downloaded normally
      #   xmake
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          xmake
          pkg-config
          wayland-scanner
          clang
          clang-tools
          gdb
        ] ++ systemLibs;

        shellHook = ''
          echo "coomer dev shell"
          echo "  xmake f --nix=y   \u2014 configure (stb from nix::, glad downloaded)"
          echo "  xmake             \u2014 build"
          echo "  xmake run         \u2014 build & run"
        '';
      };
    };
}
