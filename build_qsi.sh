#!/bin/bash
set -e

# ==============================================================================
# Universal Q4OS Installer Generator Script (.qsi) - PuTTY-TDE
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QSI_DIR="$SCRIPT_DIR/qsi_setup"
DEB_DIR="$QSI_DIR/deb_packages"
OUT_DIR="$QSI_DIR/output"
TEMPLATES_DIR="$QSI_DIR/setup_templates"

# 1. Version configuration
APP_VERSION="${1:-0.74-1}"

echo "=================================================="
echo " Q4OS .qsi Installer Universal Builder: PuTTY-TDE"
echo " Target Version: $APP_VERSION"
echo "=================================================="

# 2. Check required dependencies
if ! command -v build-qinstaller >/dev/null 2>&1; then
    echo "[Error] 'build-qinstaller' not found. Please install: sudo apt install q4os-devpack-base"
    exit 1
fi

# 3. Prepare staging directories
mkdir -p "$DEB_DIR" "$OUT_DIR" "$TEMPLATES_DIR"
rm -f "$DEB_DIR"/*.deb "$OUT_DIR"/*.qsi

# 4. Build Debian package via build_deb.sh
if [ -x "$SCRIPT_DIR/build_deb.sh" ]; then
    echo "[Info] Building Debian package..."
    "$SCRIPT_DIR/build_deb.sh" "$APP_VERSION"
fi

LATEST_DEB=$(ls -t "$SCRIPT_DIR"/*.deb "$SCRIPT_DIR"/build/*.deb 2>/dev/null | head -n 1)
if [ -z "$LATEST_DEB" ] || [ ! -f "$LATEST_DEB" ]; then
    echo "[Error] No .deb package found in project directory!"
    exit 1
fi

cp -a "$LATEST_DEB" "$DEB_DIR/"
DEB_FILENAME=$(basename "$LATEST_DEB")
PACKAGE_NAME=$(dpkg-deb -f "$LATEST_DEB" Package 2>/dev/null || echo "putty-tde")
echo "Detected Debian package: $DEB_FILENAME (Package name: $PACKAGE_NAME)"

# 5. Configure Q4OS installer
cat <<EOF > "$QSI_DIR/qinstaller"
#***q4os*setup*config*header*do*not*delete*it***#
PK_NAME="$PACKAGE_NAME"
APPNAME_DESC="PuTTY-TDE SSH and Terminal Client"
APP_ICON="$PACKAGE_NAME"
PK_VERS="$APP_VERSION"
SETUP_TYPE="2"
INST_DEBS="$PACKAGE_NAME"
DEBPCKS_DIR="$DEB_DIR"
TEMPLATES_DIR="$TEMPLATES_DIR"
OUT_DIR="$OUT_DIR"
APPLNK_ENTRY="1"
DESKTOP_ENTRY="0"
MENU_ENTRY="1"
DSTR_BASE="debian;ubuntu"
DSTR_EDTN="bullseye;bookworm;trixie;jammy;noble"
Q4VER_MIN="4.0"
CHK_INET="0"
EOF

# 6. Run Q4OS installer generator
cd "$QSI_DIR"
build-qinstaller qinstaller

# 7. Finalize and copy to project root
cd "$SCRIPT_DIR"
LATEST_QSI=$(ls -t "$OUT_DIR"/*.qsi 2>/dev/null | head -n 1)
if [ -n "$LATEST_QSI" ] && [ -f "$LATEST_QSI" ]; then
    FINAL_QSI_NAME=$(basename "$LATEST_QSI")
    cp -a "$LATEST_QSI" "$SCRIPT_DIR/$FINAL_QSI_NAME"
    chmod +x "$SCRIPT_DIR/$FINAL_QSI_NAME"
    echo ""
    echo "=================================================="
    echo " SUCCESS: Q4OS installer successfully generated!"
    echo " File: $SCRIPT_DIR/$FINAL_QSI_NAME"
    echo " Size: $(ls -lh "$SCRIPT_DIR/$FINAL_QSI_NAME" | awk '{print $5}')"
    echo "=================================================="
else
    echo "[Error] Failed to generate .qsi file."
    exit 1
fi
