{
  description = "coomer — screen zoom/magnifier for Wayland/X11";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system:
          let
            pkgs = nixpkgs.legacyPackages.${system};
            version =
              pkgs.lib.strings.removeSuffix "\n" (builtins.readFile ./VERSION);

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
          f {
            inherit
              system
              pkgs
              version
              systemLibs
              ;
          }
        );
    in
    {

      # ── Package (nix build) ───────────────────────────────────────────────
      packages = forAllSystems (
        {
          pkgs,
          version,
          systemLibs,
          ...
        }:
        {
          default = pkgs.stdenv.mkDerivation {
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
        }
      );

      # ── Dev shell (nix develop) ───────────────────────────────────────────
      devShells = forAllSystems (
        {
          pkgs,
          systemLibs,
          ...
        }:
        {
          default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            packages =
              with pkgs;
              [
                pkg-config
                wayland-scanner
                clang-tools
                gdb
              ]
              ++ systemLibs;
          };
        }
      );
    };
}
