{
  description = "Vehicle Management Engine Environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    nixgl.url = "github:nix-community/nixGL";
  };

  outputs = { self, nixpkgs, nixgl }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      nixgl-pkgs = nixgl.packages.${system};

      wrappedKitty = pkgs.writeShellScriptBin "kitty" ''
        exec ${nixgl-pkgs.default}/bin/nixGL ${pkgs.kitty}/bin/kitty \
          -o font_family="JetBrainsMono Nerd Font" \
          -o font_size=11.0 \
          ./build/bin/RentalSystem --massive-init "$@"
      '';

      wrappedXterm = pkgs.writeShellScriptBin "xterm" ''
        exec ${pkgs.xterm}/bin/xterm \
          -fa "JetBrainsMono Nerd Font" \
          -fs 11 \
          -bg black \
          -fg white \
          -title "VMS Presentation" \
          -e ./build/bin/RentalSystem --massive-init "$@"
      '';
    in {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          gcc13
          cmake
          pkg-config
          clang-tools
          tokei
          socat
          rpm
          gh
          fontconfig
          git
          zsh
          lsd
          fastfetch
          wrappedKitty
          wrappedXterm
        ];

        buildInputs = with pkgs; [
          notcurses
          nerd-fonts.jetbrains-mono
        ];

        shellHook = ''
          export XDG_DATA_DIRS="${pkgs.nerd-fonts.jetbrains-mono}/share:$XDG_DATA_DIRS"
          ${pkgs.fontconfig}/bin/fc-cache -f > /dev/null 2>&1

          if [ ! -d "$HOME/.oh-my-zsh" ]; then
            ${pkgs.git}/bin/git clone --depth=1 https://github.com/ohmyzsh/ohmyzsh.git "$HOME/.oh-my-zsh" > /dev/null 2>&1
          fi

          ZSH_CUSTOM="$HOME/.oh-my-zsh/custom"
          if [ ! -d "$ZSH_CUSTOM/plugins/zsh-autosuggestions" ]; then
            ${pkgs.git}/bin/git clone https://github.com/zsh-users/zsh-autosuggestions "$ZSH_CUSTOM/plugins/zsh-autosuggestions" > /dev/null 2>&1
          fi
          if [ ! -d "$ZSH_CUSTOM/plugins/zsh-syntax-highlighting" ]; then
            ${pkgs.git}/bin/git clone https://github.com/zsh-users/zsh-syntax-highlighting "$ZSH_CUSTOM/plugins/zsh-syntax-highlighting" > /dev/null 2>&1
          fi

          if [ ! -f "$HOME/.zshrc" ]; then
            cat << 'EOF' > "$HOME/.zshrc"
export ZSH="$HOME/.oh-my-zsh"
ZSH_THEME="agnosterzak"

plugins=(
    git
    zsh-autosuggestions
    zsh-syntax-highlighting
)

source $ZSH/oh-my-zsh.sh

fastfetch

alias ls='lsd'
alias l='ls -l'
alias la='ls -a'
alias lla='ls -la'
alias lt='ls --tree'

export PATH=$PATH
EOF
          fi

          if [ -z "$ZSH_VERSION" ]; then
            exec zsh
          fi
        '';
      };
    };
}