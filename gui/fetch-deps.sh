#!/usr/bin/env sh
#   - raylib (prebuilt Linux x86_64 static + shared libs)
#   - cJSON  (single-file JSON parser)
set -e
cd "$(dirname "$0")"
mkdir -p vendor
cd vendor

RAYLIB_VER=5.5
if [ ! -d raylib ]; then
    echo "Fetching raylib $RAYLIB_VER..."
    curl -L -o raylib.tar.gz \
        "https://github.com/raysan5/raylib/releases/download/$RAYLIB_VER/raylib-${RAYLIB_VER}_linux_amd64.tar.gz"
    tar xzf raylib.tar.gz
    rm raylib.tar.gz
    mv "raylib-${RAYLIB_VER}_linux_amd64" raylib
else
    echo "raylib already present, skipping."
fi

CJSON_VER=v1.7.18
if [ ! -f cjson/cJSON.c ]; then
    echo "Fetching cJSON $CJSON_VER..."
    mkdir -p cjson
    curl -L -o cjson/cJSON.c "https://raw.githubusercontent.com/DaveGamble/cJSON/$CJSON_VER/cJSON.c"
    curl -L -o cjson/cJSON.h "https://raw.githubusercontent.com/DaveGamble/cJSON/$CJSON_VER/cJSON.h"
else
    echo "cJSON already present, skipping."
fi

FONT_VER=v3.2.1
FONT_TTF=fonts/JetBrainsMonoNerdFontMono-Regular.ttf
if [ ! -f "$FONT_TTF" ]; then
    echo "Fetching JetBrainsMono Nerd Font $FONT_VER... (needs unzip)"
    mkdir -p fonts
    curl -L -o jbm.zip "https://github.com/ryanoasis/nerd-fonts/releases/download/$FONT_VER/JetBrainsMono.zip"
    # extract the four weights the Markdown renderer uses
    unzip -o -q jbm.zip \
        JetBrainsMonoNerdFontMono-Regular.ttf \
        JetBrainsMonoNerdFontMono-Bold.ttf \
        JetBrainsMonoNerdFontMono-Italic.ttf \
        JetBrainsMonoNerdFontMono-BoldItalic.ttf \
        -d fonts
    rm jbm.zip
else
    echo "JetBrainsMono Nerd Font already present, skipping."
fi

# Monochrome emoji fallback (raylib can't render colour emoji).
if [ ! -f fonts/NotoEmoji-Regular.ttf ]; then
    echo "Fetching Noto Emoji (monochrome)..."
    mkdir -p fonts
    curl -L -o fonts/NotoEmoji-Regular.ttf \
        "https://github.com/google/fonts/raw/main/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf"
else
    echo "Noto Emoji already present, skipping."
fi

echo "Done. Now run: make"
