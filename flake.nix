{
  description = "Vehicle Management TUI Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixgl.url = "github:nix-community/nixGL";
    escpresso = {
      url = "github:justkowal/escpresso";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      nixgl,
      escpresso,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        escpresso-pkg = pkgs.rustPlatform.buildRustPackage {
          pname = "escpresso";
          version = "0.1.2";
          src = escpresso;

          cargoLock = {
            lockFile = "${escpresso}/Cargo.lock";
          };

          nativeBuildInputs = with pkgs; [
            pkg-config
            makeWrapper
          ];

          buildInputs = with pkgs; [
            libx11
            libxcursor
            libxrandr
            libxi
            libxkbcommon
            wayland
          ];

          postInstall = ''
            wrapProgram $out/bin/escpresso \
              --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath (with pkgs; [
                libx11
                libxcursor
                libxrandr
                libxi
                libxkbcommon
                vulkan-loader
                wayland
                libGL
              ])}"
          '';
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            notcurses
            ncurses
            pkg-config
            cmake

            llvmPackages_18.clang-tools
            nil

            kitty
            mesa-demos
            escpresso-pkg
            xterm

            nixgl.packages.${system}.nixGLDefault
          ];

          shellHook = ''
            export IN_NIX_SHELL="1"
            echo "Vehicle Management TUI Engine Dev Shell Active"
          '';
        };
      }
    );
}
