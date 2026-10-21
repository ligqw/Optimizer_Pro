# Optimizer Pro 🚀

**Optimizer Pro** is a high-performance system tuning and hardware monitoring utility for Windows. Originally built on a legacy framework, this version represents a monumental evolution—**fully re-engineered from scratch in Native C++ (Win32 API & Microsoft Edge WebView2)**. 

By eliminating heavy runtimes and managed code dependencies, the project now delivers a near-zero memory footprint, instant execution speeds, and bulletproof runtime stability.

---

## ⚡ Core Architectural & Feature Upgrades

### 🛠 1. Global Refactoring & Modern C++ Architecture
* **Legacy Version:** Relied on traditional C++ implementations with raw pointer management, static memory buffers, and a restricted suite of optimization scripts.
* **Native C++ Edition (v8.0.0+):** Rewritten adhering to modern **C++17/C++20** engineering practices. Built with strict **RAII primitives** for OS descriptors (featuring explicit `UniqueHandle`, `UniqueRegKey`, `ScHandle`, and `WinHttpHandle` guard classes) ensuring a 100% leak-proof runtime environment under any stress load.

### 📊 2. Thread-Safe Real-Time Hardware Telemetry
* Spawns a dedicated asynchronous background thread (`MonitorThread`) to pull live system diagnostics without blocking user interactions.
* Tracks hardware utilization stats (**CPU** and **GPU**) with sub-second accuracy using native Windows **PDH Performance Counters**.
* Monitors structural thermals via granular **WMI ACPI queries** (`MSAcpi_ThermalZoneTemperature`).

### 🎮 3. Native GDI Diagnostic Game Overlay
* Includes an ultra-lightweight, hardware-transparent companion window (`GameOverlay`) that automatically renders vital health metrics over full-screen 3D games and media applications.
* Features low-overhead **Win32 GDI drawing logic** with high-visibility layout settings (custom typography configurations) optimized to cause zero frame-rate degradation during intense gaming sessions.

### 🧹 4. Advanced Disk, Registry, & Network Toolkits
* **Deep Maintenance:** Goes beyond deleting standard `%TEMP%` files to dynamically wipe complex cache structures, including DirectX Shader databases (`D3DSCache`), NVIDIA pipelines (`DXCache`), and system prefetch structures.
* **File Shredding:** Implements a highly secure `ShredFile` module that overwrites target sectors with randomized data streams multiple times before dropping the descriptor, preventing recovery.
* **Diagnosis Suites:** Features live drive write/read benchmarking (`TestDiskSpeed`), automated network download bandwidth calculations via Cloudflare infrastructure (`MeasureDownloadSpeed`), file-hash duplicate identifier engines (`HashFile`), and broken uninstall path registry scanners.

### 🎨 5. Next-Gen Front-End & Fluid UI Core
* Swapped out archaic layouts for a web-standard reactive interface inside a modern **Microsoft Edge WebView2** container.
* Designed with native internationalization supporting full multilingual runtime asset switching (English/Russian).
* Ships with flexible aesthetic styles (**Dark, Light, AMOLED**) combined with a portable workspace-saving **Compact Mode (Mini-UI)** toggle.
* Embeds custom JSON serialization protocols allowing users to easily export or import optimization policies across multiple machines.

---

## 🔒 Security & Crash Isolation
* Intercepts unhandled critical thread panics via a low-level Structured Exception Handling hook (`CrashHandler`), capturing instruction addresses and piping crash dump footprints directly to `optimization_log.txt`.
* Manages all file append vectors via thread-safe logging streams governed by multi-access `std::mutex` locks.
* Enforces circular log size boundaries (`LOG_MAX_SIZE`) to safeguard underlying storage resources.

---

## 🛠 Compilation Requirements

To compile the native binary from source, ensure your development workspace meets the following specifications:
* **Operating System:** Windows 10 / 11 (X64 architecture recommended)
* **Compiler / Toolset:** MSVC v143 or higher (**Visual Studio 2022** or newer) supporting C++17/C++20 standards.
* **Windows SDK:** Windows SDK 10.0.22621.0 or newer.
* **Dependencies:** Microsoft.Web.WebView2 SDK package.
* **Privileges:** Administrator privileges are required to run or debug the application since it interacts with system services (`sc config`), task schedulers (`schtasks`), and system power schemes (`powercfg`).

### Static Linking Note
The application links heavily against essential Win32 subsystem libraries. Ensure your compiler linker settings include the following workspace directives:
```cpp
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")
```
