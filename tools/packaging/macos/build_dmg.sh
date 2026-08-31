#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
VERSION="${1:-0.1.0}"
OUTPUT_DIR="${2:-${SOURCE_DIR}/dist}"
BUILD_DIR="${3:-${SOURCE_DIR}/build/macos-release}"

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

APP_DIR="$(mktemp -d /tmp/ArcadeBlocksII.app.XXXXXX)/ArcadeBlocksII.app"
mkdir -p "${APP_DIR}/Contents/MacOS"
mkdir -p "${APP_DIR}/Contents/Resources"

echo "==> Building macOS Application Bundle..."
# Find executable
if [ -f "${BUILD_DIR}/ArcadeBlocksII" ]; then
    cp "${BUILD_DIR}/ArcadeBlocksII" "${APP_DIR}/Contents/MacOS/ArcadeBlocksII"
elif [ -f "${BUILD_DIR}/bin/ArcadeBlocksII" ]; then
    cp "${BUILD_DIR}/bin/ArcadeBlocksII" "${APP_DIR}/Contents/MacOS/ArcadeBlocksII"
else
    echo "Error: ArcadeBlocksII binary not found in ${BUILD_DIR}" >&2
    exit 1
fi
chmod +x "${APP_DIR}/Contents/MacOS/ArcadeBlocksII"

# Copy assets
cp -R "${SOURCE_DIR}/assets" "${APP_DIR}/Contents/Resources/assets"
cp "${SOURCE_DIR}/assets/sprites/favicon-192.png" "${APP_DIR}/Contents/Resources/arcadeblocks2.png"

# Generate .icns if iconutil is available
if command -v iconutil >/dev/null 2>&1 && command -v sips >/dev/null 2>&1; then
    ICONSET_DIR="$(mktemp -d /tmp/arcadeblocks2.iconset.XXXXXX)"
    sips -z 16 16     "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_16x16.png" >/dev/null 2>&1 || true
    sips -z 32 32     "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_16x16@2x.png" >/dev/null 2>&1 || true
    sips -z 32 32     "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_32x32.png" >/dev/null 2>&1 || true
    sips -z 64 64     "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_32x32@2x.png" >/dev/null 2>&1 || true
    sips -z 128 128   "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_128x128.png" >/dev/null 2>&1 || true
    sips -z 192 192   "${SOURCE_DIR}/assets/sprites/favicon-192.png" --out "${ICONSET_DIR}/icon_128x128@2x.png" >/dev/null 2>&1 || true
    iconutil -c icns "${ICONSET_DIR}" -o "${APP_DIR}/Contents/Resources/arcadeblocks2.icns" >/dev/null 2>&1 || true
    rm -rf "${ICONSET_DIR}"
fi

# Create Info.plist
cat << PLIST > "${APP_DIR}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>ArcadeBlocksII</string>
    <key>CFBundleIconFile</key>
    <string>arcadeblocks2</string>
    <key>CFBundleIdentifier</key>
    <string>com.leocallidus.arcadeblocks2</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Arcade Blocks II</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
PLIST

echo "APPL????" > "${APP_DIR}/Contents/PkgInfo"

echo "==> Creating macOS DMG..."
DMG_STAGING="$(mktemp -d /tmp/arcadeblocks2-dmg.XXXXXX)"
cp -R "${APP_DIR}" "${DMG_STAGING}/ArcadeBlocksII.app"
ln -s /Applications "${DMG_STAGING}/Applications"

DMG_FILE="${OUTPUT_DIR}/ArcadeBlocksII-${VERSION}-macOS.dmg"
hdiutil create -volname "Arcade Blocks II" -srcfolder "${DMG_STAGING}" -ov -format UDZO "${DMG_FILE}"

rm -rf "${DMG_STAGING}" "$(dirname "${APP_DIR}")"
echo "==> Successfully created ${DMG_FILE}"
