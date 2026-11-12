<div align="center">

# ⚡ ZeroBloat

**Ultra-lightweight, native C++20 system optimization, debloating, and hardware monitoring utility for Windows 10 & 11.**

[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?logo=windows)](https://github.com/ligqw/ZeroBloat)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://github.com/ligqw/ZeroBloat)
[![Latest Release](https://img.shields.io/github/v/release/ligqw/ZeroBloat)](https://github.com/ligqw/ZeroBloat/releases)
[![Downloads](https://img.shields.io/github/downloads/ligqw/ZeroBloat/total)](https://github.com/ligqw/ZeroBloat/releases)
[![Stars](https://img.shields.io/github/stars/ligqw/ZeroBloat)](https://github.com/ligqw/ZeroBloat/stargazers)

**English** • [Русский](README.ru.md)

[Features](#key-features) • [Safety & Hardening](#safety-and-hardening) • [Installation](#installation-and-usage) • [Hotkeys & CLI](#hotkeys-and-cli) • [Build from Source](#building-from-source)

</div>

---

**ZeroBloat** is a high-performance toolkit built from the ground up in **Native C++ (Win32 API + Edge WebView2)**. Unlike bloatware optimizers built on Electron or .NET, ZeroBloat requires no heavy runtimes, consumes only a few megabytes of RAM, launches instantly, and talks directly to Windows APIs for maximum efficiency.

> [!TIP]
> **New in [v13.0.0](https://github.com/ligqw/ZeroBloat/releases/tag/v13.0.0):** Hardened WebView2 sandbox with strict CSP, automatic `audit.log` tracking, minidump crash reporting, background ICMP ping worker, and S.M.A.R.T. disk health/temperature monitoring.

---

## Key Features

### 🧠 Smart Optimization & Diagnostics
* **Smart PC Analyzer:** Scans your system and generates a curated list of safe, reversible optimizations with estimated disk space savings and risk ratings.
* **Fast MFT Drive Scanner:** Lightning-fast disk analyzer featuring an interactive visual **Treemap** to pinpoint space-hogging files and folders.
* **Security Dashboard:** Instant status check for Windows Defender, Firewall, UAC, and Windows Update to detect vulnerabilities at a glance.
* **Hardware & Disk Health:** Real-time telemetry for CPU, GPU (NVIDIA/AMD/Intel), VRAM, network speeds, background ICMP ping, and S.M.A.R.T. drive wear/temperature.

### 🚀 Gaming & Performance Boost
* **Auto Game Detection & Booster:** Automatically detects fullscreen games, elevates game priority, and suspends background telemetry/indexing services (`SysMain`, `WSearch`, `DiagTrack`).
* **One-Click Game Presets:** Instantly switch between **Competitive FPS**, **Balanced**, and **Quality** power and registry profiles.
* **Low-Latency Registry Tweaks:** Optimizes `Win32PrioritySeparation`, disables Network Throttling, disables the Nagle algorithm to reduce ping, and disables Fullscreen Optimizations (FSO).
* **Hidden GPU Overclocking:** Silently launches MSI Afterburner in the background to apply your saved profile without cluttering your desktop.
* **LZX Game Compression:** Transparently compresses game directories via the native Windows `compact.exe` to save gigabytes of SSD space without hurting load times.

### 🧹 Deep Cleaning & Memory Management
* **ISLC-Class Cache Cleaner:** Purges Temp files, Prefetch, browser/launcher caches, Windows Update & Delivery Optimization leftovers, DirectX/NVIDIA shader caches, and thumbnails.
* **Mem Reduct-Class RAM Purger:** Flushes the Standby list, clears the modified page list, and trims process Working Sets with adjustable aggressiveness (Levels 1–5) and automatic idle/threshold triggers.

### 🛠️ Advanced System & App Toolkit
* **Bulk App Uninstaller & Winget Manager:** Batch-uninstall multiple programs silently (MSI, Inno Setup, NSIS) or update all installed software in one click via the official Microsoft Winget repositories.
* **Startup & Service Managers:** Full control over registry autostart entries (`HKCU` / `HKLM`) and Windows services (`Auto`, `Manual`, `Disabled`).
* **Windows 11 Context Menu & Shell Extensions:** Toggle between the classic Win10 and modern Win11 context menus, and disable sluggish third-party Shell Extensions.
* **Safe Hosts Editor:** Built-in editor with automatic `.bak` backups and post-write integrity verification.
* **File Utilities:** Fast duplicate finder (by size and header hash), large file scanner, and a secure **3-pass File Shredder**.

### 🎨 Modern UI & In-Game Overlay
* **13 Dynamic & Static Themes:** Includes the signature **Liquid Glass** frosted acrylic UI, minimalist presets (*Black, White, Midnight*), and smooth animated themes (*Void, Ocean, Neon, Forest, Crimson*, etc.).
* **Customizable In-Game Overlay:** Displays FPS-critical metrics (CPU, GPU, VRAM, RAM, Temps, Ping) over fullscreen games with custom X/Y positioning, transparency, and Dark/Light modes.
* **Bilingual Interface:** Seamless switching between **English** and **Russian**.

---

## Safety and Hardening

ZeroBloat is engineered with strict safeguards to protect system integrity:
* **BSOD Pagefile Guard:** Checks physical RAM before changing paging settings (`DisablePagingExecutive`). The operation is blocked if your system has **less than 32 GB of RAM**.
* **Non-System Drive Protection:** Automatically hides critical OS tweaks (Lite Debloat, Services, Registry) when a secondary drive is selected in settings.
* **Locked-Down WebView2 Sandbox:** The UI runs from a local virtual host with a strict Content-Security-Policy (CSP), blocking external navigation and enforcing strict IPC message limits.
* **Automatic Backups & Audit Trail:** Creates timestamped `.reg` and `.bak` restore points before modifying the registry or `hosts` file, and logs all critical actions to `audit.log`.
* **Non-Blocking Async Architecture:** All heavy I/O, WMI, and network tasks run in isolated background threads with timeout protection, so the UI never freezes.

---

## Installation and Usage

1. Download the latest `ZeroBloat.exe` from the [**Releases**](https://github.com/ligqw/ZeroBloat/releases) page.
2. Right-click and select **Run as administrator** *(required for service, registry, and WMI access)*.
3. *(Optional)* If the **Microsoft Edge WebView2 Runtime** is missing on your PC, ZeroBloat will verify Microsoft's Authenticode signature and offer a one-click silent installation.

> [!NOTE]
> Because the executable is not signed with a paid EV certificate, Windows SmartScreen may show a warning on first launch. Click **More info** → **Run anyway**.

---

## Hotkeys and CLI

### Global Hotkeys

| Shortcut | Action |
| :--- | :--- |
| `Ctrl` + `Alt` + `O` | Toggle In-Game Diagnostic Overlay |
| `Ctrl` + `Alt` + `M` | Instant RAM Purge (Standby & Working Sets) |
| `Ctrl` + `Alt` + `C` | Quick Temp Cleanup & DNS Flush |

### Command Line Interface (CLI)
```bat
ZeroBloat.exe [options]
```

| Argument | Description |
| :--- | :--- |
| `--portable` | Store settings, backups, logs, and WebView2 cache next to `ZeroBloat.exe` (ideal for USB drives). |
| `--silent` | Start minimized directly to the system tray. |
| `--no-admin` | Launch without administrator privileges (some features will be unavailable). |
| `--help` | Display available command-line flags. |

---

## Building from Source

### Prerequisites
* **Visual Studio 2022** (with the *Desktop development with C++* workload) or the MSVC Build Tools
* **Windows SDK** (`10.0.22621.0` or newer)
* **WebView2 SDK** (`Microsoft.Web.WebView2` via NuGet)
* A compiler with **C++20** support (MSVC `cl.exe`)

### Compilation Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/ligqw/ZeroBloat.git
   cd ZeroBloat
   ```
2. Install the WebView2 SDK via NuGet (`Microsoft.Web.WebView2`).
3. Open the **x64 Native Tools Command Prompt for VS 2022** and compile `ZeroBloat.cpp` with MSVC. Use the same command and libraries as in [`.github/workflows/build.yml`](.github/workflows/build.yml), and make sure the `/std:c++20` and `/utf-8` flags are set (`/utf-8` is required so that Russian UI strings are not garbled).
4. Alternatively, fork the repository and run the **GitHub Actions** workflow: it builds `ZeroBloat.exe` on `windows-latest` and publishes it as a build artifact.

> [!NOTE]
> The required Windows libraries and `WebView2LoaderStatic.lib` are linked via `#pragma comment` in the source, so no extra manual linking is needed beyond the WebView2 SDK paths.

---

## Disclaimer

> [!WARNING]
> **Use at your own risk.** ZeroBloat modifies the Windows Registry, system services, and OS files. While actions include safety checks, backups, and warnings, the author assumes no responsibility for system instability, data loss, or unintended side effects. **Always create a Windows System Restore Point** before applying aggressive tweaks such as *Lite Windows Debloat* or disabling core services.

---

<div align="center">
  Made with ❤️ for the gaming and PC enthusiast community.
</div>
