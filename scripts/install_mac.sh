#!/bin/bash
echo "=== Collonka v1.0.0 Installer ==="
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# VST3 — ditto preserves bundle internals; cp -R strips Sealed Resources
# (and then launchd refuses to spawn the .app on Apple Silicon).
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$VST3_DIR"
if [ -d "$SCRIPT_DIR/VST3/Collonka.vst3" ]; then
  ditto "$SCRIPT_DIR/VST3/Collonka.vst3" "$VST3_DIR/Collonka.vst3"
  echo "VST3 installed to: $VST3_DIR"
fi

# AU — same bundle-preservation requirement.
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$AU_DIR"
if [ -d "$SCRIPT_DIR/AU/Collonka.component" ]; then
  ditto "$SCRIPT_DIR/AU/Collonka.component" "$AU_DIR/Collonka.component"
  echo "AU installed to: $AU_DIR"
fi

# Presets
PRESET_DIR="$HOME/Library/Application Support/Collonka/Presets/Factory"
mkdir -p "$PRESET_DIR"
cp "$SCRIPT_DIR/Presets/Factory/"*.xml "$PRESET_DIR/" 2>/dev/null
echo "Presets installed to: $PRESET_DIR"

echo ""
echo "Done! Restart your DAW and rescan plugins."
