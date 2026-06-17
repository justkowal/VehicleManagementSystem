#!/bin/bash
set -e

# Setup directories
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
MINGW_DIR="$SCRIPT_DIR/mingw64"
TMP_DIR="$SCRIPT_DIR/tmp_pkg"

echo "Initializing MinGW-w64 target dependency folder..."
mkdir -p "$MINGW_DIR"
mkdir -p "$TMP_DIR"
cd "$TMP_DIR"

# List of URLs to fetch
URLS=(
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-notcurses-3.0.17-3-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-ncurses-6.6-4-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libunistring-1.4.2-1-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libiconv-1.19-1-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libsystre-1.0.2-2-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libtre-0.9.0-2-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-gettext-runtime-1.0-1-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-pcre2-10.47-1-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-zlib-1.3.2-2-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-bzip2-1.0.8-3-any.pkg.tar.zst"
  "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libdeflate-1.25-1-any.pkg.tar.zst"
)

# Download and extract packages
for url in "${URLS[@]}"; do
  filename=$(basename "$url")
  if [ ! -f "$filename" ]; then
    echo "Downloading $filename..."
    curl -L -O "$url"
  else
    echo "$filename already downloaded."
  fi
  
  echo "Extracting $filename to $MINGW_DIR..."
  tar --use-compress-program=zstd -xf "$filename" -C "$MINGW_DIR" --strip-components=1
done

echo "Cleaning up temporary archives..."
rm -rf "$TMP_DIR"

echo "Forcing static linking by removing *.dll.a dynamic import libraries..."
find "$MINGW_DIR/lib" -name "*.dll.a" -delete

echo "Patching .pc files for pkg-config prefix adjustments..."
PC_DIR="$MINGW_DIR/lib/pkgconfig"
if [ -d "$PC_DIR" ]; then
  # Replace prefix=/mingw64 with prefix=<absolute path to libs/mingw64>
  # Need to escape slashes for sed
  ESCAPED_MINGW_DIR=$(echo "$MINGW_DIR" | sed 's/\//\\\//g')
  
  for pc in "$PC_DIR"/*.pc; do
    echo "Patching $(basename "$pc")..."
    sed -i 's/\r//g' "$pc"
    sed -i "s/^prefix=\/mingw64/prefix=$ESCAPED_MINGW_DIR/g" "$pc"
  done
  
  if [ -f "$PC_DIR/notcurses-core.pc" ]; then
    echo "Adding static dependencies to notcurses-core.pc..."
    sed -i '/^Libs:/ s/$/ -ldeflate -lpthread/' "$PC_DIR/notcurses-core.pc"
  fi
else
  echo "Warning: pkgconfig directory not found!"
fi

echo "Successfully completed MinGW-w64 dependency setup!"
