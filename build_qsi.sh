#!/bin/bash
set -e

# ==============================================================================
# Script Universel de Génération d'Installeurs Q4OS (.qsi) - PuTTY-TDE
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QSI_DIR="$SCRIPT_DIR/qsi_setup"
DEB_DIR="$QSI_DIR/deb_packages"
OUT_DIR="$QSI_DIR/output"
TEMPLATES_DIR="$QSI_DIR/setup_templates"

# 1. Paramètres de version
APP_VERSION="${1:-0.74-1}"

echo "=================================================="
echo " Q4OS .qsi Installer Universal Builder: PuTTY-TDE"
echo " Target Version: $APP_VERSION"
echo "=================================================="

# 2. Vérification des outils nécessaires
if ! command -v build-qinstaller >/dev/null 2>&1; then
    echo "[Error] 'build-qinstaller' non trouvé. Installez : sudo apt install q4os-devpack-base"
    exit 1
fi

# 3. Préparation des répertoires de staging
mkdir -p "$DEB_DIR" "$OUT_DIR" "$TEMPLATES_DIR"
rm -f "$DEB_DIR"/*.deb "$OUT_DIR"/*.qsi

# 4. Compilation du paquet Debian via build_deb.sh
if [ -x "$SCRIPT_DIR/build_deb.sh" ]; then
    echo "[Info] Compilation du paquet Debian..."
    "$SCRIPT_DIR/build_deb.sh" "$APP_VERSION"
fi

LATEST_DEB=$(ls -t "$SCRIPT_DIR"/*.deb "$SCRIPT_DIR"/build/*.deb 2>/dev/null | head -n 1)
if [ -z "$LATEST_DEB" ] || [ ! -f "$LATEST_DEB" ]; then
    echo "[Error] Aucun fichier .deb trouvé dans le répertoire du projet !"
    exit 1
fi

cp -a "$LATEST_DEB" "$DEB_DIR/"
DEB_FILENAME=$(basename "$LATEST_DEB")
PACKAGE_NAME=$(dpkg-deb -f "$LATEST_DEB" Package 2>/dev/null || echo "putty-tde")
echo "Paquet Debian détecté : $DEB_FILENAME (Nom: $PACKAGE_NAME)"

# 5. Configuration de l'installeur Q4OS
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

# 6. Exécution du générateur Q4OS
cd "$QSI_DIR"
build-qinstaller qinstaller

# 7. Finalisation et copie à la racine du projet
cd "$SCRIPT_DIR"
LATEST_QSI=$(ls -t "$OUT_DIR"/*.qsi 2>/dev/null | head -n 1)
if [ -n "$LATEST_QSI" ] && [ -f "$LATEST_QSI" ]; then
    FINAL_QSI_NAME=$(basename "$LATEST_QSI")
    cp -a "$LATEST_QSI" "$SCRIPT_DIR/$FINAL_QSI_NAME"
    chmod +x "$SCRIPT_DIR/$FINAL_QSI_NAME"
    echo ""
    echo "=================================================="
    echo " SUCCÈS : Installeur Q4OS généré avec succès !"
    echo " Fichier : $SCRIPT_DIR/$FINAL_QSI_NAME"
    echo " Taille  : $(ls -lh "$SCRIPT_DIR/$FINAL_QSI_NAME" | awk '{print $5}')"
    echo "=================================================="
else
    echo "[Error] Échec lors de la génération du fichier .qsi."
    exit 1
fi
