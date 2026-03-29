#!/usr/bin/env bash
# Snare Rhythm Generator — macOS .pkg builder
# Run from repo root: bash installer/macos/build_pkg.sh
# Requires: Xcode CLT, cmake (brew install cmake)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
ARTEFACTS="$BUILD_DIR/SnareRhythmGenerator_artefacts/Release"

VST3_SRC="$ARTEFACTS/VST3/SnareRhythmGenerator.vst3"
AU_SRC="$ARTEFACTS/AU/SnareRhythmGenerator.component"

VERSION="1.1"
PKG_NAME="SnareRhythmGenerator-${VERSION}-macOS.pkg"
PKG_OUT="$SCRIPT_DIR/$PKG_NAME"

STAGE="$SCRIPT_DIR/stage"
PKG_VST3="$SCRIPT_DIR/pkg_vst3.pkg"
PKG_AU="$SCRIPT_DIR/pkg_au.pkg"
PKG_DIST="$SCRIPT_DIR/distribution.xml"

echo "==> Checking build artefacts..."
[[ -d "$VST3_SRC" ]] || { echo "ERROR: VST3 not found at $VST3_SRC — run cmake build first"; exit 1; }
[[ -d "$AU_SRC"   ]] || { echo "ERROR: AU not found at $AU_SRC — run cmake build first"; exit 1; }

echo "==> Staging files..."
rm -rf "$STAGE"
mkdir -p "$STAGE/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGE/au/Library/Audio/Plug-Ins/Components"

cp -r "$VST3_SRC" "$STAGE/vst3/Library/Audio/Plug-Ins/VST3/"
cp -r "$AU_SRC"   "$STAGE/au/Library/Audio/Plug-Ins/Components/"

echo "==> Building component packages..."
pkgbuild \
  --root "$STAGE/vst3" \
  --identifier "com.snaregen.vst3" \
  --version "$VERSION" \
  --install-location "/" \
  "$PKG_VST3"

pkgbuild \
  --root "$STAGE/au" \
  --identifier "com.snaregen.au" \
  --version "$VERSION" \
  --install-location "/" \
  "$PKG_AU"

echo "==> Writing distribution.xml..."
cat > "$PKG_DIST" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Snare Rhythm Generator ${VERSION}</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt" mime-type="text/plain"/>
    <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>

    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>

    <choice id="vst3" title="VST3 Plugin"
            description="Installs SnareRhythmGenerator.vst3 to ~/Library/Audio/Plug-Ins/VST3/"
            start_selected="true">
        <pkg-ref id="com.snaregen.vst3"/>
    </choice>

    <choice id="au" title="Audio Unit (AU)"
            description="Installs SnareRhythmGenerator.component to ~/Library/Audio/Plug-Ins/Components/"
            start_selected="true">
        <pkg-ref id="com.snaregen.au"/>
    </choice>

    <pkg-ref id="com.snaregen.vst3" version="${VERSION}">pkg_vst3.pkg</pkg-ref>
    <pkg-ref id="com.snaregen.au"   version="${VERSION}">pkg_au.pkg</pkg-ref>
</installer-gui-script>
XML

echo "==> Writing welcome and license stubs..."
cat > "$SCRIPT_DIR/welcome.html" <<HTML
<html><body>
<h2>Snare Rhythm Generator ${VERSION}</h2>
<p>Genre-aware MIDI snare pattern generator for VST3 and AU hosts.</p>
<p>Supports: Logic Pro, Ableton Live, Bitwig, GarageBand, Reaper, and any VST3/AU DAW.</p>
<p>After installation, rescan plugins in your DAW.</p>
</body></html>
HTML

cat > "$SCRIPT_DIR/license.txt" <<TXT
Snare Rhythm Generator ${VERSION}
Copyright (c) 2026 SnareGen / Gleinkaa

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software to use, copy, modify, and distribute it without restriction.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
TXT

echo "==> Building final .pkg..."
productbuild \
  --distribution "$PKG_DIST" \
  --package-path "$SCRIPT_DIR" \
  --resources "$SCRIPT_DIR" \
  "$PKG_OUT"

echo "==> Cleaning up intermediate files..."
rm -rf "$STAGE" "$PKG_VST3" "$PKG_AU" "$PKG_DIST" \
       "$SCRIPT_DIR/welcome.html" "$SCRIPT_DIR/license.txt"

echo ""
echo "✓ Installer: $PKG_OUT"
echo ""
echo "To validate AU after install:"
echo "  auval -v aumu SRhG Snrg"
