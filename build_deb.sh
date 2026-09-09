#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="putty-tde"
PKG_VERSION="${1:-0.74-1}"
PKG_MAINTAINER="seb3773"
PKG_SECTION="net"
PKG_PRIORITY="optional"

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
ARCH="$(dpkg --print-architecture)"
BUILD_DIR="$SRC_ROOT/build"
PKGROOT="$BUILD_DIR/pkgroot"
PKGTMP="$BUILD_DIR/pkgtmp"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd dpkg-deb
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd du
need_cmd ln
need_cmd install

mkdir -p -- "$BUILD_DIR"
rm -rf -- "$PKGTMP"
mkdir -p -- "$PKGTMP"

# Compile binary using build.sh
"$SRC_ROOT/build.sh"

BIN_PATH="$BUILD_DIR/putty-tde"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Stage filesystem layout
rm -rf -- "$PKGROOT"
mkdir -p -- \
	"$PKGROOT/DEBIAN" \
	"$PKGROOT/usr/bin" \
	"$PKGROOT/usr/share/applications" \
	"$PKGROOT/usr/share/icons/hicolor" \
	"$PKGROOT/usr/share/pixmaps"

# Install binary and putty convenience symlink
install -m 0755 "$BIN_PATH" "$PKGROOT/usr/bin/putty-tde"
ln -sf putty-tde "$PKGROOT/usr/bin/putty"

# Install desktop entry
cat > "$PKGROOT/usr/share/applications/putty-tde.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=PuTTY-TDE
GenericName=SSH and Telnet Client
Comment=Connect to remote systems via SSH, Telnet, Serial or Rlogin
Exec=putty-tde %u
Icon=putty-tde
Terminal=false
Type=Application
Categories=Network;RemoteAccess;TerminalEmulator;
EOF
chmod 0644 "$PKGROOT/usr/share/applications/putty-tde.desktop"

# Install application icon tree (using icons/putty.png)
ICON_SRC="$SRC_ROOT/icons/putty.png"
if test -f "$ICON_SRC"; then
	real_sz="48x48"
	real_dir="$PKGROOT/usr/share/icons/hicolor/$real_sz/apps"
	mkdir -p -- "$real_dir"
	install -m 0644 "$ICON_SRC" "$real_dir/putty-tde.png"
	install -m 0644 "$ICON_SRC" "$PKGROOT/usr/share/pixmaps/putty-tde.png"
	ln -sf putty-tde.png "$PKGROOT/usr/share/pixmaps/putty.png"
	for sz in 16x16 22x22 24x24 32x32 64x64; do
		dstdir="$PKGROOT/usr/share/icons/hicolor/$sz/apps"
		mkdir -p -- "$dstdir"
		ln -sf "../../$real_sz/apps/putty-tde.png" "$dstdir/putty-tde.png"
		ln -sf "putty-tde.png" "$dstdir/putty.png"
	done
else
	echo "warning: missing $ICON_SRC (application icon will not be installed)" >&2
fi

# Specify explicit optimal runtime dependencies
DEPENDS="libtqt3-mt-trinity (>= 4:14.0.0) | libtqt3-mt, libx11-6, libc6 (>= 2.15)"

# Strip / sstrip the staged binary again
STAGED_BIN="$PKGROOT/usr/bin/putty-tde"
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$STAGED_BIN" >/dev/null 2>&1 || true
else
	echo "info: sstrip not found, using strip --strip-all"
	strip --strip-all "$STAGED_BIN" >/dev/null 2>&1 || true
fi

# Debian control file
INSTALLED_SIZE_KB="$(du -sk "$PKGROOT/usr" | awk '{print $1}')"
cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: $PKG_SECTION
Priority: $PKG_PRIORITY
Architecture: $ARCH
Maintainer: $PKG_MAINTAINER
Installed-Size: $INSTALLED_SIZE_KB
Depends: $DEPENDS
Provides: putty
Description: Lightweight SSH and Telnet client for Trinity Desktop Environment (TDE)
 PuTTY-TDE is a native TQt3/TDE port of the popular PuTTY terminal emulator
 and SSH client. It provides a lightweight, highly responsive, and robust
 interface for SSH, Telnet, Serial, and Rlogin connections without any
 GNOME or GTK dependencies.
EOF

# postinst: refresh caches & tdebuildsycoca
cat > "$PKGROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -x /opt/trinity/bin/tdebuildsycoca ]; then
    /opt/trinity/bin/tdebuildsycoca >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postinst"

# postrm: refresh caches
cat > "$PKGROOT/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ -x /opt/trinity/bin/tdebuildsycoca ]; then
    /opt/trinity/bin/tdebuildsycoca >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postrm"

# prerm: best effort cache refresh
cat > "$PKGROOT/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/prerm"

OUT_DEB="$SRC_ROOT/${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
rm -f -- "$OUT_DEB"

dpkg-deb --build "$PKGROOT" "$OUT_DEB" >/dev/null

echo "=================================================="
echo " Debian package successfully built:"
echo " $OUT_DEB"
echo " Size: $(ls -lh "$OUT_DEB" | awk '{print $5}')"
echo "=================================================="
exit 0
