{
  description = "coomer — screen zoom/magnifier for Wayland/X11";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs   = nixpkgs.legacyPackages.${system};

      # ── System libraries consumed via pkgconfig ──────────────────
      systemLibs = with pkgs; [
        wayland
        wayland-protocols
        libxkbcommon
        libGL
        libglvnd       # EGL
        egl-wayland
        xorg.libX11
        xorg.libXrandr
        dbus
      ];

      # ── Fixed-output derivation: pre-fetch xmake packages ────────
      # (glad + stb are managed by xmake's own package manager.)
      # Run `nix build .#xmakeDeps` once — it fails and prints the
      # correct sha256; paste it into outputHash below.
      xmakeDeps = pkgs.stdenv.mkDerivation {
        name = "coomer-xmake-deps";
        src  = ./.;

        nativeBuildInputs = with pkgs; [
          xmake curl git cacert pkg-config
          wayland-scanner
        ] ++ systemLibs;

        # Network IS allowed in fixed-output derivations — that's the
        # whole point.
        outputHashAlgo = "sha256";
        outputHashMode = "recursive";
        # Fill in the real hash after the first `nix build` attempt
        # (the error will print: "got: sha256-XXXX").
        outputHash     = "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

        buildPhase = ''
          export HOME=$TMPDIR
          export XMAKE_GLOBALDIR=$TMPDIR/.xmake
          xmake f --yes -p linux -a x86_64 \
            --wayland=y --x11=y --portal=y 2>&1
        '';

        installPhase = ''
          cp -r $TMPDIR/.xmake $out
        '';
      };

    in {

      # ── Package ──────────────────────────────────────────────────
      packages.${system} = {
        # Exposes the cached deps so you can update the hash:
        #   nix build .#xmakeDeps 2>&1 | grep "got:"
        xmakeDeps = xmakeDeps;

        default = pkgs.stdenv.mkDerivation {
          pname   = "coomer";
          version = "0.1.0";
          src     = ./.;

          nativeBuildInputs = with pkgs; [
            xmake pkg-config wayland-scanner
          ];
          buildInputs = systemLibs;

          buildPhase = ''
            export HOME=$TMPDIR
            export XMAKE_GLOBALDIR=$TMPDIR/.xmake
            # Inject pre-fetched package cache (read-write copy)
            cp -rT ${xmakeDeps} $TMPDIR/.xmake
            chmod -R +w $TMPDIR/.xmake

            xmake f --yes -p linux -a x86_64 \
              --wayland=y --x11=y --portal=y --network=none
            xmake build --yes
          '';

          installPhase = ''
            runHook preInstall
            install -Dm755 build/linux/x86_64/release/coomer \
              $out/bin/coomer
            runHook postInstall
          '';

          meta = {
            description = "Screen zoom/magnifier for Wayland and X11";
            homepage    = "https://github.com/yuzujr/coomer";
            license     = pkgs.lib.licenses.mit;
            platforms   = pkgs.lib.platforms.linux;
          };
        };
      };

      # ── Dev shell ────────────────────────────────────────────────
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          xmake
          pkg-config
          wayland-scanner
          clang
          clang-tools    # clangd, clang-format
          gdb
        ] ++ systemLibs;

        shellHook = ''
          echo "coomer dev shell"
          echo "  xmake          — build"
          echo "  xmake run      — build & run"
          echo "  xmake f -c     — clean config cache"
        '';
      };
    };
}
