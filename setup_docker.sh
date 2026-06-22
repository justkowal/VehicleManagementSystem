#!/usr/bin/env bash

set -e

prompt_yn() {
    local message="$1"
    while true; do
        read -p "$message [y/n]: " yn
        case $yn in
            [Yy]* ) return 0;;
            [Nn]* ) return 1;;
            * ) echo "Please answer y or n.";;
        esac
    done
}

echo "This script requires administrative privileges for corrective actions."
sudo -v

echo "=== Docker Environment Setup Checklist ==="

if ! command -v docker &> /dev/null; then
    echo "[✗] Docker is not installed."
    if prompt_yn "    Action: Install Docker using the official script?"; then
        curl -fsSL https://get.docker.com | sh
        if ! command -v docker &> /dev/null; then
            echo "[✗] Installation failed."
            exit 1
        fi
        echo "[✓] Docker installed successfully."
    else
        echo "[✗] Execution halted: Docker is required."
        exit 1
    fi
else
    echo "[✓] Docker installation verified."
fi

if command -v systemctl &> /dev/null; then
    if ! systemctl is-active --quiet docker; then
        echo "[✗] Docker service is inactive."
        if prompt_yn "    Action: Start the Docker service via systemctl?"; then
            sudo systemctl start docker
            if ! systemctl is-active --quiet docker; then
                echo "[✗] Failed to start Docker service."
                exit 1
            fi
            echo "[✓] Docker service started."
        else
            echo "[✗] Execution halted: Active service required."
            exit 1
        fi
    else
        echo "[✓] Docker service is active."
    fi
else
    echo "[ ] Docker service status: Unknown (systemctl missing)"
fi

if ! getent group docker | grep -q "\b$USER\b"; then
    echo "[✗] User '$USER' is missing from the docker group."
    if prompt_yn "    Action: Add '$USER' to the docker group?"; then
        if ! getent group docker > /dev/null; then
            sudo groupadd docker
        fi
        sudo usermod -aG docker "$USER"
        echo "[✓] User added to group."
    else
        echo "[ ] Skipping group modification."
    fi
else
    echo "[✓] User group membership verified."
fi

if [ ! -S /var/run/docker.sock ]; then
    echo "[✗] Docker socket file is missing or inaccessible."
    exit 1
fi

if [ ! -r /var/run/docker.sock ] || [ ! -w /var/run/docker.sock ]; then
    echo "[✗] Read/write access denied for /var/run/docker.sock."
    if prompt_yn "    Action: Apply temporary read/write permissions (chmod 666) to the socket?"; then
        sudo chmod 666 /var/run/docker.sock
        echo "[✓] Socket permissions updated."
    else
        echo "[✗] Access denied to socket loop."
    fi
else
    echo "[✓] Docker socket permissions verified."
fi

HAS_NVIDIA=false
if command -v nvidia-smi &> /dev/null || (command -v lspci &> /dev/null && lspci | grep -iq nvidia); then
    HAS_NVIDIA=true
fi

mkdir -p .devcontainer/desktop

# Grant X11 access to local Docker containers (required for MIT-MAGIC-COOKIE on RPi OS)
if command -v xhost &> /dev/null && [ -n "$DISPLAY" ]; then
    xhost +local:docker > /dev/null 2>&1
    echo "[✓] X11 access granted to local Docker containers."
else
    echo "[ ] X11 access: skipped (no DISPLAY or xhost unavailable)."
fi

if [ "$HAS_NVIDIA" = true ]; then
    if ! command -v nvidia-container-toolkit &> /dev/null; then
        echo "[✗] NVIDIA hardware detected but toolkit is missing."
        if prompt_yn "    Action: Install NVIDIA Container Toolkit?"; then
            curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
            curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
                sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
                sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
            sudo apt-get update
            sudo apt-get install -y nvidia-container-toolkit
            sudo nvidia-ctk runtime configure --runtime=docker
            sudo systemctl restart docker
            echo "[✓] NVIDIA Container Toolkit configured."
        else
            echo "[ ] Skipping toolkit implementation."
        fi
    else
        echo "[✓] NVIDIA Container Toolkit verified."
    fi

    # Add user to render group for GPU access
    if ! getent group render | grep -q "\b$USER\b" 2>/dev/null; then
        echo "[ ] User '$USER' not in 'render' group (optional, for /dev/dri/renderD128 access)."
    fi

    echo -n "[ ] Writing NVIDIA-optimized devcontainer.json... "
    cat << 'EOF' > .devcontainer/desktop/devcontainer.json
{
    "name": "Vehicle Management TUI Engine (Linux Desktop)",
    "build": {
        "dockerfile": "../Dockerfile"
    },
    "features": {
        "ghcr.io/devcontainers/features/nix:1": {
            "extraNixConfig": "experimental-features = nix-command flakes"
        }
    },
    "runArgs": [
        "--net=host",
        "--gpus=all",
        "--group-add", "render",
        "--group-add", "video",
        "-e", "DISPLAY=${localEnv:DISPLAY}",
        "-e", "XAUTHORITY=/tmp/.docker.xauth"
    ],
    "mounts": [
        "source=/tmp/.X11-unix,target=/tmp/.X11-unix,type=bind",
        "source=${localEnv:XAUTHORITY},target=/tmp/.docker.xauth,type=bind,readonly"
    ],
    "ipc": "host",
    "postCreateCommand": "sudo mkdir -p /etc/nix && echo 'experimental-features = nix-command flakes' | sudo tee -a /etc/nix/nix.conf > /dev/null && sudo dbus-uuidgen --ensure=/etc/machine-id && direnv allow",
    "userEnvProbe": "loginShell",
    "customizations": {
        "vscode": {
            "settings": {
                "editor.formatOnSave": true,
                "C_Cpp.clang_format_style": "file",
                "C_Cpp.clang_format_fallbackStyle": "Google",
                "workbench.colorTheme": "Catppuccin Mocha",
                "C_Cpp.intelliSenseEngine": "disabled",
                "nix.enableLanguageServer": true,
                "nix.serverPath": "nil"
            },
            "extensions": [
                "Catppuccin.catppuccin-vsc",
                "xaver.clang-format",
                "llvm-vs-code-extensions.vscode-clangd",
                "ms-vscode.cpptools-extension-pack",
                "eamodio.gitlens",
                "zaaack.markdown-editor",
                "jnoortheen.nix-ide",
                "mkhl.direnv"
            ]
        }
    }
}
EOF
    echo "DONE"

else
    if [ -d /dev/dri ]; then
        if [ ! -r /dev/dri/renderD128 ] || [ ! -w /dev/dri/renderD128 ]; then
            echo "[✗] Direct Rendering Infrastructure access denied."
            if prompt_yn "    Action: Open access to /dev/dri devices (chmod 666)?"; then
                sudo chmod 666 /dev/dri/*
                echo "[✓] Render device permissions opened."
            else
                echo "[ ] Skipping hardware device adjustment."
            fi
        else
            echo "[✓] Direct Rendering Infrastructure verified."
        fi
    else
        echo "[✓] GPU Infrastructure: Core software rendering fallback active."
    fi

    echo -n "[ ] Writing Render-optimized devcontainer.json... "
    cat << 'EOF' > .devcontainer/desktop/devcontainer.json
{
    "name": "Vehicle Management TUI Engine (Linux Desktop)",
    "build": {
        "dockerfile": "../Dockerfile"
    },
    "features": {
        "ghcr.io/devcontainers/features/nix:1": {
            "extraNixConfig": "experimental-features = nix-command flakes"
        }
    },
    "runArgs": [
        "--net=host",
        "--device=/dev/dri",
        "--device=/dev/dri/renderD128",
        "--group-add", "render",
        "--group-add", "video",
        "-e", "DISPLAY=${localEnv:DISPLAY}",
        "-e", "XAUTHORITY=/tmp/.docker.xauth"
    ],
    "mounts": [
        "source=/tmp/.X11-unix,target=/tmp/.X11-unix,type=bind",
        "source=${localEnv:XAUTHORITY},target=/tmp/.docker.xauth,type=bind,readonly"
    ],
    "ipc": "host",
    "postCreateCommand": "sudo mkdir -p /etc/nix && echo 'experimental-features = nix-command flakes' | sudo tee -a /etc/nix/nix.conf > /dev/null && sudo dbus-uuidgen --ensure=/etc/machine-id && direnv allow",
    "userEnvProbe": "loginShell",
    "customizations": {
        "vscode": {
            "settings": {
                "editor.formatOnSave": true,
                "C_Cpp.clang_format_style": "file",
                "C_Cpp.clang_format_fallbackStyle": "Google",
                "workbench.colorTheme": "Catppuccin Mocha",
                "C_Cpp.intelliSenseEngine": "disabled",
                "nix.enableLanguageServer": true,
                "nix.serverPath": "nil"
            },
            "extensions": [
                "Catppuccin.catppuccin-vsc",
                "xaver.clang-format",
                "llvm-vs-code-extensions.vscode-clangd",
                "ms-vscode.cpptools-extension-pack",
                "eamodio.gitlens",
                "zaaack.markdown-editor",
                "jnoortheen.nix-ide",
                "mkhl.direnv"
            ]
        }
    }
}
EOF
    echo "DONE"
fi

if docker ps &> /dev/null; then
    echo "[✓] Container daemon communication verified."
else
    if sg docker -c "docker ps" &> /dev/null; then
        echo "[✓] Container daemon communication verified via group subshell fallback."
    else
        echo "[✗] Connection verification failed."
        exit 1
    fi
fi

echo "=== Graphical Sanity Check ==="
if prompt_yn "Would you like to run a universal glxgears container to verify hardware acceleration?"; then
    if [ -z "$DISPLAY" ]; then
        echo "[✗] Error: \$DISPLAY environment variable is missing."
    else
        echo "    Configuring X11 access control paths..."
        if command -v xhost &> /dev/null; then
            xhost +local:root > /dev/null 2>&1
        fi

        GPU_TEST_ARGS=""
        if [ "$HAS_NVIDIA" = true ]; then
            GPU_TEST_ARGS="--gpus all"
        elif [ -d /dev/dri ]; then
            GPU_TEST_ARGS="--device /dev/dri"
        fi

        set +e
        OUTPUT=$(docker run --rm \
            $GPU_TEST_ARGS \
            -e DISPLAY="$DISPLAY" \
            -e vblank_mode=0 \
            -e __GL_SYNC_TO_VBLANK=0 \
            -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
            --net=host \
            debian:12-slim sh -c "apt-get update >/dev/null && apt-get install -y --no-install-recommends mesa-utils >/dev/null && timeout 10 glxgears" 2>&1)
        STATUS=$?
        set -e

        if echo "$OUTPUT" | grep -qE "authentication required|unauthorized"; then
            echo "[✗] Authentication error: Stale/invalid login found on this guest machine."
            if prompt_yn "    Action: Run 'docker logout' to clear credentials and retry public pull?"; then
                docker logout
                echo "    Retrying test execution..."
                set +e
                OUTPUT=$(docker run --rm \
                    $GPU_TEST_ARGS \
                    -e DISPLAY="$DISPLAY" \
                    -e vblank_mode=0 \
                    -e __GL_SYNC_TO_VBLANK=0 \
                    -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
                    --net=host \
                    debian:12-slim sh -c "apt-get update >/dev/null && apt-get install -y --no-install-recommends mesa-utils >/dev/null && timeout 10 glxgears" 2>&1)
                set -e
            fi
        fi

        echo "$OUTPUT" | grep -E "frames|FPS" || true
        MAX_FPS=$(echo "$OUTPUT" | grep -oE '[0-9.]+\s+FPS' | awk '{print $1}' | sort -nr | head -n1 | cut -d. -f1)

        if [ -n "$MAX_FPS" ] && [ "$MAX_FPS" -gt 300 ]; then
            echo "[✓] Graphical verification complete: GPU acceleration active ($MAX_FPS FPS)."
        elif [ -n "$MAX_FPS" ]; then
            echo "[✗] Graphical warning: Framerate low ($MAX_FPS FPS). Running on software rendering or VSync is locked."
        else
            echo "[✗] Graphical verification failed: No benchmark data captured."
        fi
    fi
fi

echo "=== Configuration Complete ==="