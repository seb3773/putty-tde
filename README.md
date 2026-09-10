# PuTTY-TDE

<p align="center">
  <img src="konqiputty.png" alt="PuTTY-TDE">
</p>

> **A native, lightweight, and modern TQt3 port of PuTTY for the Trinity Desktop Environment (TDE).**

PuTTY-TDE is a complete and faithful port of the renowned **PuTTY (v0.74)** graphical client to **pure TQt3** (`libtqt-mt`). It provides a native, highly responsive, and feature-complete SSH, Telnet, Serial, and terminal emulator specifically crafted for **Trinity Desktop Environment (TDE)** and lightweight X11 environments, completely free of any GTK, GNOME, or KParts dependencies.

---

## 🌟 Key Highlights

* **100% Pure TQt3**: Directly uses `libtqt-mt` and `libX11`. Zero dependencies on GTK (`libgtk`, `libgdk`, `libglib`), GNOME libraries, or heavy desktop frameworks.
* **Unmodified Upstream Core**: Retains the complete, rock-solid PuTTY C99 engine (114 upstream source files compiled directly) for cryptography, protocol handling, and VT100 terminal emulation.
* **Featherweight Footprint**: Self-contained executable of only **~910 KB** through Link-Time Optimization (LTO), dead-code elimination, and aggressive ELF header stripping (`sstrip`).
* **Self-Contained Resources**: All application icons and assets are compiled directly into the executable binary as C byte arrays. No external PNG files or runtime theme packages required.
* **UX Enhancements over Legacy GTK PuTTY**:
  * **Persistent Configuration Window**: Automatically unhides and restores when your terminal session ends, with input focus positioned on the host field for immediate reconnection.
  * **Position & Geometry Memory**: Automatically remembers and restores window size and screen coordinates across sessions via `TQSettings`.
  * **Unified SSH Login Dialog**: Enter username and password simultaneously in a single, well-proportioned (300px) dialog.
  * **Smart Status Bar**: Clean connection lifecycle display (`Connecting...` ➔ `user@host[:port]` ➔ `[Closed]`) with options shortcut hint.
  * **Full Context Menu**: Classic Unix `Ctrl + Right Click` menu with session switching, dynamic saved sessions, remote special commands, and on-the-fly reconfiguration.

---

## 🚀 Features & Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      PuTTY-TDE Frontend                     │
│  ┌───────────────────┐ ┌──────────────────┐ ┌─────────────┐ │
│  │ PuTTYConfigDialog │ │PuTTYSessionWindow│ │  tqtask.cpp │ │
│  │   (tqtdlg.cpp)    │ │   (tqtwin.cpp)   │ │(Auth Dialog)│ │
│  └─────────┬─────────┘ └────────┬─────────┘ └──────┬──────┘ │
│            │   ┌────────────────┴──────────────┐   │        │
│            └───┤   PuTTYTermWidget (VT Canvas) ├───┘        │
│                └────────────────┬──────────────┘            │
│  ┌──────────────────────────────┴────────────────────────┐  │
│  │      TQtEventBridge (tqtcomm.cpp) & Glue (tqtglue.c)  │  │
│  └──────────────────────────────┬────────────────────────┘  │
└─────────────────────────────────┼───────────────────────────┘
                                  ▼
┌─────────────────────────────────────────────────────────────┐
│                     PuTTY Core Engine                       │
│  ┌───────────────┐  ┌───────────────┐  ┌──────────────────┐ │
│  │  terminal.c   │  │    uxsel.c    │  │  SSH / Protocols │ │
│  │ (VT Emulation)│  │ (Async Events)│  │ (Crypto / Network│ │
│  └───────────────┘  └───────────────┘  └──────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 1. Networking & Protocols
* **Supported Protocols**: SSH-1, SSH-2, Telnet, Rlogin, Raw, Serial (RS-232 / USB-to-UART adapters), and SUPDUP.
* **State-of-the-Art Cryptography**: Full suite of ciphers and key exchange algorithms: RSA, DSA, ECDSA, Ed25519, ChaCha20-Poly1305, AES-GCM, Diffie-Hellman group exchange.
* **Advanced Features**: SSH Agent forwarding, X11 forwarding, SOCKS4/5 / HTTP proxies, and local/remote port tunneling.

### 2. Native TQt3 Event-Driven Backend
* **Asynchronous Socket Multiplexing**: PuTTY's `uxsel` subsystem is bridged directly to `TQSocketNotifier` (Read, Write, Exception).
* **Timers & Callbacks**: PuTTY's internal timer queue (`timing.c`) is mapped to `TQTimer`, and asynchronous top-level coroutine callbacks are posted directly into the TQt3 event loop.

### 3. Terminal Emulator & Canvas
* **Double-Buffered Rendering**: Flicker-free canvas powered by `TQPainter` drawing to an off-screen `TQPixmap` backing store.
* **Color Schemes**: Standard 16 ANSI colors, extended 256-color palette, 24-bit TrueColor support, and reverse video.
* **Configurable Cursor**: Full block, underline, or vertical bar with blinking support.
* **Visual Bell**: Native screen flash (`ATTR_REVERSE`) support when audio bells are disabled.
* **Window Manager Geometry Snapping**: Compliant with TDE Twin / KWin size hints (`PResizeInc`, `setBaseSize`). Window resize dynamically snaps to font character cell bounds (`80x24`, `120x35`, etc.) with visual geometry indicators.
* **Full Screen Mode**: Toggle full-screen with `Alt + Enter`, automatically adapting scrollbars and the status bar.

### 4. X11 Mouse & Keyboard Ergonomics
* **Full Mouse Selection**:
  * Left-click drag for continuous character-level selection.
  * Double-click to select word (`MA_2CLK`).
  * Triple-click to select whole line (`MA_3CLK`).
  * Middle-click to paste immediately into the console (`MBT_PASTE`).
  * Right-click to extend current selection (`MBT_EXTEND`).
* **Clipboard Integration**: Synchronous support for both X11 `PRIMARY` selection and `CLIPBOARD`.
* **Standard Keybindings**:
  * `Shift + Insert` / `Ctrl + Shift + Insert`: Paste from PRIMARY or CLIPBOARD.
  * `Shift + PageUp` / `Shift + PageDown`: Half-page scrollback.
  * `Ctrl + Shift + PageUp` / `Ctrl + Shift + PageDown`: Top / bottom of scrollback.
  * `Ctrl + PageUp` / `Ctrl + PageDown`: Line-by-line scrollback.
  * `Ctrl + >` (`Ctrl + +`) / `Ctrl + <` (`Ctrl + -`): Dynamic on-the-fly terminal font scaling with automatic column/row re-computation.
* **Desktop Drag-and-Drop**: Drag files from Konqueror, Krusader, or the desktop directly into the terminal to automatically paste their sanitized absolute filepaths.
* **Pointer Auto-Hide**: Hides the mouse pointer during typing, reappearing on mouse movement.

### 5. Persistent Configuration System
* **Complete Upstream Tree**: All categories present (Session, Logging, Terminal, Keyboard, Bell, Features, Window, Appearance, Behaviour, Translation, Selection, Colours, Connection, Data, Proxy, Telnet, Rlogin, SSH, Kex, Host Keys, Cipher, Auth, X11, Tunnels, Serial).
* **Smart Lifecycle**:
  * The configuration dialog does not destroy itself when clicking "Open"; it hides and safely preserves settings.
  * When a session ends or disconnects, the configuration dialog automatically reappears with focus restored on the host input for 1-key reconnection.
  * Geometry, coordinates, and custom dimensions are saved across runs using `TQSettings` (`/putty/config_window/`).
* **Mid-Session Reconfiguration**: Open "Change Settings..." while inside an active terminal to update fonts, colors, and behavior on the fly (`term_reconfig`).

### 6. Authentication & Security
* **Unified Login Prompt**: Combines username and password into a clean 300px dialog. Pressing `Enter` in the username field shifts focus to the password field; pressing `Enter` again submits credentials.
* **SSH Host Key Verification**: 3-button modal prompt (&Accept, Connect &Once, &Cancel) executed synchronously to avoid coroutine re-entrancy conflicts during SSH transport handshake.
* **Exit Confirmation**: Safeguards active connections with an exit warning prompt (`CONF_warn_on_close`) when pressing Alt+F4 or closing the window.

### 7. Informational Status Bar
* **Clean & Non-Intrusive**: Purely informational standard `TQStatusBar` without hijacking mouse clicks.
* **Connection Lifecycle**:
  * **Connecting / Authenticating**: Displays `Connecting to <host>...` (local workstation username is never prematurely guessed or displayed).
  * **Connected**: Displays `user@host[:port]` with the authenticated remote username.
  * **Serial**: Displays device path and baud rate (e.g., `/dev/ttyUSB0, 9600 baud`).
  * **Closed**: Suffixes `[Closed]` upon remote exit or disconnection.
* **Options Shortcut Hint**: Permanent label on the right (`Ctrl+Right Click for options  `).
* **Configurable**: Toggleable from the context menu (`Show Status Bar`) and hidden automatically in full-screen mode.

### 8. Full Context Menu (`Ctrl + Right Click`)
Access the complete upstream menu tree at any time:
1. `New Session...`
2. `Restart Session` (active when disconnected)
3. `Duplicate Session`
4. `Saved Sessions ▶` (dynamically populated from storage)
5. `Change Settings...`
6. `Event Log` (with clipboard export)
7. `Special Commands ▶` (SSH Break, EOF, signals)
8. `Clear Scrollback`
9. `Reset Terminal`
10. `Copy to CLIPBOARD` / `Paste from CLIPBOARD`
11. `Show Status Bar` (toggleable checkmark)
12. `About PuTTY-TDE`

---

## 📦 Building from Source

### Prerequisites

You will need a Linux environment with TDE development libraries and standard build tools installed:

* **Debian / Ubuntu (with Trinity Desktop Environment repository)**:
  ```bash
  sudo apt-get install cmake gcc g++ pkg-config libtqt3-mt-dev libx11-dev sstrip
  ```

### Automated Build Scripts

To simplify building and packaging, automated scripts are provided:

* **Standard / Stripped Binary Build**:
  ```bash
  ./build.sh
  ```
  Produces `./build/putty-tde` with LTO and `sstrip` optimizations.

* **Debian Package (`.deb`)**:
  ```bash
  ./build_deb.sh [version]
  # Example: ./build_deb.sh 0.74-1
  ```
  Generates `putty-tde_0.74-1_amd64.deb` with proper system integration (desktop file, hicolor icons, `/usr/bin/putty` symlink, and cache triggers).

* **Q4OS Universal Installer (`.qsi`)**:
  ```bash
  ./build_qsi.sh [version]
  # Example: ./build_qsi.sh 0.74-1
  ```
  Builds `setup_putty-tde_0.74-1.qsi` with GUI wizard (welcome screen, license, installation progress, and desktop/menu registration). Requires `q4os-devpack-base`.

### Manual Compilation

1. Clone or extract the repository:
   ```bash
   git clone https://github.com/your-repo/putty-tde.git
   cd putty-tde
   ```

2. Configure and build:
   ```bash
   cmake -B build -DPUTTY_AGGRESSIVE_FLAGS=ON
   cmake --build build -j$(nproc)
   ```

3. The stripped, optimized binary is generated at:
   ```bash
   ./build/putty-tde
   ```

### Command-Line Usage

`putty-tde` supports all standard PuTTY command-line flags:

```bash
# Connect to an SSH host
./build/putty-tde user@hostname.example.com

# Connect with custom port
./build/putty-tde -P 2222 user@hostname.example.com

# Connect using a private key
./build/putty-tde -i ~/.ssh/id_rsa user@hostname.example.com

# Load a saved session directly
./build/putty-tde -load "MyServer"

# Connect to a serial port
./build/putty-tde -serial /dev/ttyUSB0 -P 115200

# Other supported protocols:
# -ssh, -telnet, -rlogin, -raw, -serial
```

---

## 📊 Binary Size Comparison

| Variant | Binary Size | Notes |
| :--- | :---: | :--- |
| **Standard Upstream PuTTY (GTK3 build)** | ~1,650 KB | Requires GTK3, GDK, GLib, Pango, Cairo |
| **Standard PuTTY (GTK2 build)** | ~1,400 KB | Requires GTK2, GDK, GLib |
| **PuTTY-TDE (Release + LTO + sstrip)** | **910 KB** | **100% Pure TQt3 + X11 only (-45% size reduction)** |

---

## 📂 Source Tree Overview

```
putty-tde/
├── CMakeLists.txt         # Root build configuration with LTO, sstrip, and TQt MOC rules
├── README.md              # Project documentation
├── build.sh               # Fast release build & strip script
├── build_deb.sh           # Native Debian (.deb) package generator
├── build_qsi.sh           # Q4OS Universal Installer (.qsi) generator
├── convert_images.py      # Asset compiler converting PNG icons to C byte arrays
├── icons/                 # Raw application icon assets (putty.png, puttycfg.png, about_puttytde.png)
├── konqiputty.png         # Project banner and mascot artwork
├── qsi_setup/             # Q4OS installer templates, resources, and hooks
├── tqt/                   # TQt3 Native Frontend implementation
│   ├── putty_headers.h    # C++ namespace/macro isolation header for PuTTY C headers
│   ├── putty_icons.h      # Embedded binary icon byte arrays
│   ├── tqtcomm.h/.cpp     # Asynchronous event bridge (sockets, timers, callbacks)
│   ├── tqtdlg.h/.cpp      # Configuration dialog, dynamic controls, session persistence
│   ├── tqtask.h/.cpp      # Authentication, SSH host key verification, security alerts
│   ├── tqtwin.h/.cpp      # Terminal window, VT rendering, mouse/keys, status bar
│   └── tqtglue.c          # Platform hooks and glue logic
└── _LEGACY/               # Upstream PuTTY v0.74 core engine (crypto, protocols, VT100)
```

---

## 📄 License

PuTTY-TDE is distributed under the same permissive **MIT License** as upstream PuTTY:

```
PuTTY is copyright 1997-2020 Simon Tatham.
Portions copyright Robert de Bath, Joris van Rantwijk, Delian Delchev,
Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry, Justin Bradford,
Ben Harris, Malcolm Smith, Ahmad Khalifa, Markus Kuhn, Colin Watson,
Christopher Staite, and Jacob Nevins.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
```
