FROM mcr.microsoft.com/devcontainers/base:debian-12

ENV DEBIAN_FRONTEND=noninteractive

# Install GUI, GPU, and terminal bridging dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    dbus-x11 \
    fontconfig \
    fonts-dejavu-core \
    libgl1-mesa-dri \
    mesa-utils \
    direnv \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Hook direnv seamlessly into the shell profiles
RUN echo 'eval "$(direnv hook bash)"' >> /home/vscode/.bashrc && \
    echo 'eval "$(direnv hook zsh)"' >> /home/vscode/.zshrc

# Grant write access to /usr/local for local installations without sudo
RUN chown -R vscode:vscode /usr/local