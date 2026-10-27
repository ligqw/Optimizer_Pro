# ⚡ Optimizer Pro

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-0078D7?style=flat-square&logo=windows)](https://github.com/ligqw/Optimizer_Pro)
[![C++](https://img.shields.io/badge/Language-C%2B%2B20-00599C?style=flat-square&logo=c%2B%2B)](https://github.com/ligqw/Optimizer_Pro)
[![Version](https://img.shields.io/badge/Version-v11.0.0-brightgreen?style=flat-square)](https://github.com/ligqw/Optimizer_Pro/releases)

**Optimizer Pro** is a high-performance utility for fine-tuning, cleaning, and monitoring the Windows system.

Written in native **C++ (Win32 API + WebView2)**, the program does not require heavy runtime environments (such as .NET or Electron). This ensures minimal RAM consumption, instant UI response, and deep access to system functions.

---

## ✨ Key Features (v11.0.0)

### 🧠 Smart Optimization
* **Automatic PC Analysis**: Scans the system and generates a list of safe (or reversible) improvements, indicating the category, potential space savings, and risk level.
* **Flexible Application**: You choose which items to apply (cache cleaning, memory optimization, enabling Game Mode, etc.).

### 🚀 Performance & Gaming
* **Game Booster**: Instantly stops background services (SysMain, WSearch, DiagTrack) to boost FPS.
* **Game Mode & FSO**: Enables Windows Game Mode and disables Fullscreen Optimizations.
* **Hidden Overclock**: Launches MSI Afterburner in the background, applying your saved profile (without opening windows).
* **Game Compression (LZX)**: Transparent compression of game folders via `compact.exe` to save SSD space.
* **Registry Tweaks**: Configures `Win32PrioritySeparation`, disables Network Throttling, and the Nagle algorithm to reduce ping.

### 🧹 Deep Cleaning & Memory
* **Enhanced Cache (ISLC-class)**: Cleans Temp, Prefetch, browser caches, launchers, Delivery Optimization, Windows Update cache, shader cache, and thumbnails.
* **Enhanced RAM (Mem Reduct-class)**: Clears the Standby list, flushes the modified page list, and trims process Working Sets. Adjustable aggressiveness.

### 🛠️ Advanced System Tools
* **Startup Manager**: Manage entries from the registry (HKCU and HKLM).
* **Service Manager**: Start, stop, and change the startup type (Auto, Manual, Disabled) for any service.
* **Context Menu Manager**: Enable the classic Windows 11 menu or the modern one, and disable third-party Shell Extensions.
* **Hosts Editor**: Built-in editor with automatic backup creation and integrity verification.
* **File Tools**: Large file finder, duplicate finder (by hash and size), and a secure 3-pass file shredder.

### 🎨 UI & Customization
* **12 Dynamic Themes**: From classic Black/White/Midnight to animated themes (Void, Ocean, Neon, Forest, Crimson, etc.).
* **In-Game Overlay**: Displays CPU, GPU, RAM, and temperature on top of games. Custom positioning (X/Y) and transparency settings.
* **Multi-language**: Full support for Russian and English.

---

## 🛡️ Safety & Protection
* **BSOD Protection**: When attempting to disable the pagefile (Disable Paging), the program checks your RAM size. If you have less than 32 GB, the operation is blocked.
* **Drive Protection**: When a non-system drive is selected (in settings), dangerous features (Lite Windows, Services, Registry tweaks) are automatically hidden.
* **Asynchronous Operations**: All heavy operations run in background threads, preventing the UI from freezing.

---

## 🚀 Installation & Running

1. Go to the [Releases](https://github.com/ligqw/Optimizer_Pro/releases) section.
2. Download the latest `Optimizer_pro.exe` file.
3. Run as **Administrator** (required for access to services, registry, and WMI).

*(If Edge WebView2 Runtime is not installed on your system, the program will automatically offer to download and install it on the first launch).*

### Command Line Arguments (CLI)
* `--no-admin` : Run without administrator privileges (limited functionality).
* `--silent` : Start minimized to the system tray.
* `--help` : Show help for arguments.

---

## 💻 Building from Source

To build the project, you will need:
* **Visual Studio 2022** (with Desktop development with C++).
* **Windows SDK** (10.0.22621.0 or newer).
* **WebView2 SDK** (Microsoft.Web.WebView2).
* **C++20** (or C++17).

1. Clone the repository.
2. Open the `.sln` solution file in Visual Studio.
3. Ensure that the linker settings include `WebView2LoaderStatic.lib` and other libraries listed in the `#pragma comment`.
4. Build the project in `Release` / `x64` configuration.

---

## ⚠️ Disclaimer
The program makes changes to the registry, system services, and files. Although all actions are accompanied by warnings and are (mostly) reversible, the author is not responsible for any system instability, data loss, or other consequences. **Use at your own risk.** Before applying aggressive tweaks (Lite Windows, disabling services), it is highly recommended to create a system restore point.

---
*Made with ❤️ for the gaming and enthusiast community.*
