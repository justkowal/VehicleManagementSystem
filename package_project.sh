#!/usr/bin/env bash

# Package VehicleManagementSystem Source Code
# Generates a clean ZIP archive containing only tracked source code and build tools,
# excluding all Git metadata, GitHub workflows, ignore files, and packaging scripts.

set -e

# Define names
REPO_NAME="VehicleManagementSystem"
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
OUTPUT_ZIP="${REPO_NAME}-${TIMESTAMP}.zip"

echo "=== Packaging Project Source Code ==="

# Check if we are in a git repository
if ! git rev-parse --is-inside-work-tree &>/dev/null; then
    echo "[✗] Error: Not in a git repository."
    exit 1
fi

# Create a temporary directory for packaging
TEMP_DIR=$(mktemp -d -p .)
export TEMP_DIR

# 1. Package the main repository's staged working tree into a temporary tar
echo "    Staging current working tree..."
git add -A
git archive --format=tar -o "$TEMP_DIR/main.tar" $(git write-tree)
echo "    Restoring repository state..."
git reset --mixed HEAD &>/dev/null || git reset &>/dev/null || true

# Extract main tar to the temp directory
tar -xf "$TEMP_DIR/main.tar" -C "$TEMP_DIR"
rm "$TEMP_DIR/main.tar"

# 2. Package all submodules recursively
git submodule foreach --recursive '
    echo "    Packaging submodule: $displaypath"
    # Stage any local changes in the submodule
    git add -A
    git archive --format=tar -o "$toplevel/$TEMP_DIR/submodule.tar" $(git write-tree)
    git reset --mixed HEAD &>/dev/null || git reset &>/dev/null || true
    # Extract the submodule files into the correct place in the temp directory
    mkdir -p "$toplevel/$TEMP_DIR/$displaypath"
    tar -xf "$toplevel/$TEMP_DIR/submodule.tar" -C "$toplevel/$TEMP_DIR/$displaypath"
    rm "$toplevel/$TEMP_DIR/submodule.tar"
'

# 3. Strip unwanted files from the temporary directory
echo "    Stripping unnecessary configurations..."
rm -rf "$TEMP_DIR/.github"
rm -f "$TEMP_DIR/.gitignore"
rm -f "$TEMP_DIR/.gitmodules"
rm -f "$TEMP_DIR/.gitattributes"
rm -f "$TEMP_DIR/package_project.sh"
rm -f "$TEMP_DIR/setup_docker.sh"

# Strip them recursively in submodules too
find "$TEMP_DIR" -name ".git" -o -name ".github" -o -name ".gitignore" -o -name ".gitattributes" -o -name ".gitmodules" | xargs rm -rf 2>/dev/null || true

# 4. Zip the temporary directory contents
echo "    Compressing into ZIP archive: $OUTPUT_ZIP ..."
(cd "$TEMP_DIR" && zip -r "../$OUTPUT_ZIP" . >/dev/null)

# 5. Clean up temp directory
rm -rf "$TEMP_DIR"

# Print results
if [ -f "$OUTPUT_ZIP" ]; then
    SIZE_KB=$(du -k "$OUTPUT_ZIP" | cut -f1)
    echo "[✓] Project packaged successfully!"
    echo "    Archive file: $OUTPUT_ZIP"
    echo "    Archive size: ${SIZE_KB} KB"
    echo ""
    echo "=== Archive Contents (Top Level) ==="
    unzip -l "$OUTPUT_ZIP" | head -n 25
else
    echo "[✗] Error: Packaging failed."
    exit 1
fi
