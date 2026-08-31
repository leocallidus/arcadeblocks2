#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
VERSION="${1:-0.1.0}"
OUTPUT_DIR="${2:-${SOURCE_DIR}/dist}"
BUILD_DIR="${3:-${SOURCE_DIR}/build/linux-release}"
APPDIR="$(mktemp -d /tmp/arcadeblocks2-appdir.XXXXXX)"

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

echo "==> Staging AppDir at ${APPDIR}..."
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}" --prefix /usr

# Also provide assets at AppDir root for maximum compatibility
mkdir -p "${APPDIR}/assets"
cp -R "${SOURCE_DIR}/assets/"* "${APPDIR}/assets/"

# AppImage requires desktop file and icon at root of AppDir
cp "${APPDIR}/usr/share/applications/arcadeblocks2.desktop" "${APPDIR}/"
cp "${APPDIR}/usr/share/icons/hicolor/192x192/apps/arcadeblocks2.png" "${APPDIR}/"
cp "${APPDIR}/usr/share/icons/hicolor/192x192/apps/arcadeblocks2.png" "${APPDIR}/.DirIcon"

# AppRun launcher
cat << 'APPRUN' > "${APPDIR}/AppRun"
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "${HERE}/usr/bin/ArcadeBlocksII" "$@"
APPRUN
chmod +x "${APPDIR}/AppRun"

# Download appimagetool if not found
APPIMAGETOOL="${APPIMAGETOOL:-}"
if [ -z "${APPIMAGETOOL}" ] || [ ! -f "${APPIMAGETOOL}" ]; then
    TOOL_DIR="$(mktemp -d /tmp/appimagetool.XXXXXX)"
    echo "==> Downloading appimagetool..."
    curl -sLfo "${TOOL_DIR}/appimagetool" https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "${TOOL_DIR}/appimagetool"
    (
        cd "${TOOL_DIR}"
        ./appimagetool --appimage-extract >/dev/null 2>&1 || true
    )
    if [ -f "${TOOL_DIR}/squashfs-root/AppRun" ]; then
        APPIMAGETOOL="${TOOL_DIR}/squashfs-root/AppRun"
    else
        APPIMAGETOOL="${TOOL_DIR}/appimagetool"
    fi
fi

APPIMAGE_OUTPUT="${OUTPUT_DIR}/ArcadeBlocksII-${VERSION}-x86_64.AppImage"
echo "==> Generating AppImage: ${APPIMAGE_OUTPUT}..."
ARCH=x86_64 "${APPIMAGETOOL}" "${APPDIR}" "${APPIMAGE_OUTPUT}"

rm -rf "${APPDIR}"
echo "==> Successfully created ${APPIMAGE_OUTPUT}"
