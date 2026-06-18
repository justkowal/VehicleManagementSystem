{
  description = "Vehicle Management TUI Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixgl.url = "github:nix-community/nixGL";
  };

  outputs = { self, nixpkgs, flake-utils, nixgl }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;


        nvidiaVersionEnv = builtins.getEnv "NVIDIA_VERSION";
        nvidiaVersion = if nvidiaVersionEnv != "" then nvidiaVersionEnv else null;

        nixglPkgs = nixgl.inputs.nixpkgs.legacyPackages.${system};

        nixGLNvidiaPkg = if nvidiaVersion != null then
          (import "${nixgl}/default.nix" {
            pkgs = nixglPkgs;
            enable32bits = nixglPkgs.stdenv.hostPlatform.isx86;
            enableIntelX86Extensions = nixglPkgs.stdenv.hostPlatform.isx86;
            inherit nvidiaVersion;
          }).nixGLNvidia
        else
          null;

        nixGLNvidiaLink = if nixGLNvidiaPkg != null then
          pkgs.runCommand "nixGLNvidia-link" {} ''
            mkdir -p $out/bin
            ln -s ${nixGLNvidiaPkg}/bin/nixGLNvidia-${nvidiaVersion} $out/bin/nixGLNvidia
            ln -s ${nixGLNvidiaPkg}/bin/nixGLNvidia-${nvidiaVersion} $out/bin/nixGL
          ''
        else
          null;


        wrapGPU = name: exe: pkgs.writeShellScriptBin name ''
          if [ -e /proc/driver/nvidia ] || command -v nvidia-smi >/dev/null 2>&1; then
            if command -v nixGLNvidia >/dev/null 2>&1; then
              exec nixGLNvidia ${exe} "$@"
            elif command -v nixGL >/dev/null 2>&1; then
              exec nixGL ${exe} "$@"
            fi
          else
            if command -v nixGLIntel >/dev/null 2>&1; then
              exec nixGLIntel ${exe} "$@"
            elif command -v nixGL >/dev/null 2>&1; then
              exec nixGL ${exe} "$@"
            fi
          fi
          exec ${exe} "$@"
        '';

        wrappedKitty = wrapGPU "kitty" "${pkgs.kitty}/bin/kitty";
        wrappedGlxinfo = wrapGPU "glxinfo" "${pkgs.mesa-demos}/bin/glxinfo";
        wrappedGlxgears = wrapGPU "glxgears" "${pkgs.mesa-demos}/bin/glxgears";
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [

            notcurses
            ncurses
            pkg-config
            cmake
            clang-tools
            nil


            wrappedKitty
            wrappedGlxinfo
            wrappedGlxgears
            xterm


            nixgl.packages.${system}.nixGLIntel
            nixgl.packages.${system}.nixGLDefault
          ] ++ lib.optionals (nixGLNvidiaPkg != null) [
            nixGLNvidiaPkg
            nixGLNvidiaLink
          ];

          shellHook = ''
            export IN_NIX_SHELL="1"
            echo "🏎️ Vehicle Management TUI Engine Environment Active"


            if [ -e /proc/driver/nvidia ] || command -v nvidia-smi >/dev/null 2>&1; then
              if command -v nixGLNvidia >/dev/null 2>&1; then
                echo "GPU Acceleration Bridge: Configured for NVIDIA (using nixGLNvidia)"
              else
                echo "GPU Acceleration Bridge: Configured for NVIDIA (using nixGL)"
              fi
            else
              if command -v nixGLIntel >/dev/null 2>&1; then
                echo "GPU Acceleration Bridge: Configured for Intel/AMD/Mesa (using nixGLIntel)"
              else
                echo "GPU Acceleration Bridge: Configured for Intel/AMD/Mesa (using nixGL)"
              fi
            fi
          '';
        };
      }
    );
}
