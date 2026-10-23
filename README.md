# Optimizer Pro 🚀 — v9.0.0 (Ultimate Edition)

Optimizer Pro is an ultra-high-performance system tuning, privacy hardening, and hardware monitoring utility for Windows. Re-engineered from scratch in Native C++ (Win32 API & Microsoft Edge WebView2), this version completely eliminates heavy managed runtimes (like .NET or Electron) to deliver a near-zero memory footprint, instant execution speeds, and bulletproof system-level stability.

---

## ⚡ Core Architectural & Feature Upgrades

### 🛠 1. Global Refactoring & Modern C++ Architecture
* **Modern Toolset:** Rewritten completely adhering to strict C++17/C++20 engineering practices.
* **Leak-Proof Runtime:** Built with strict RAII primitives for OS descriptors (featuring explicit `UniqueHandle`, `UniqueRegKey`, `ScHandle`, and `WinHttpHandle` guard classes) ensuring a 100% leak-proof execution under heavy stress loads.
* **Crash Isolation:** Intercepts unhandled critical thread panics via low-level Structured Exception Handling hooks (`CrashHandler`), capturing instruction fault addresses and piping them directly into `optimization_log.txt`.

### 📊 2. Thread-Safe Telemetry & Self-Resource Tracking
* **Asynchronous Telemetry:** Spawns a dedicated background thread (`MonitorThread`) to pull live system diagnostics without blocking user interaction or causing interface micro-stutters.
* **Sub-Second Accuracy:** Tracks total hardware utilization (CPU and GPU) with sub-second accuracy using native Windows PDH Performance Counters.
* **Self-Usage Metrics (New in v9.0.0):** Features transparent self-resource tracking. The dashboard now natively tracks and displays the app's own lightweight consumption metrics (`App CPU %`, `App RAM MB`, `App GPU %`).
* **Thermal Diagnostics:** Monitors structural thermals via granular WMI ACPI queries (`MSAcpi_ThermalZoneTemperature`).

### 📦 3. Lite Windows Debloating & Winget Integration (New in v9.0.0)
* **Lite Windows Debloat:** Includes a powerful, aggressive debloating engine designed to strip down standard Windows into an ultra-lightweight workspace (similar to Tiny11 or 7tims OS). It permanently removes pre-installed UWP bloatware (Bing Weather, Xbox, Zune, People, Skype, etc.), kills tracking pipelines, and disables heavy background indexing routines (`WSearch`).
* **Winget App Manager:** Implements a single-click package installation suite. Users can easily deploy popular tools, web browsers, messaging clients, and gaming hubs (Chrome, Discord, 7-Zip, Steam, Notepad++, etc.) directly from official Microsoft Winget repositories.
* **Driver Updates Suite:** Features a dedicated control module providing direct navigation to official driver downloads (NVIDIA, AMD, Intel) alongside automated background updating mechanisms.

### 🧠 4. Native Hardware Feature Detection (New in v9.0.0)
* **Register-Level Inspection:** Natively queries low-level processor instruction extensions via hardware registers (`CPUID` and `XGETBV`). 
* **Capability Mapping:** Visually charts hardware compatibility for advanced computing instructions, confirming support for `SSE4.1`, `SSE4.2`, `POPCNT`, `AVX`, `FMA3`, `AVX2`, `BMI1`, `BMI2`, and `AVX-512`.

### 🧹 5. Advanced Maintenance & File Shredding
* **Deep Cache Purge:** Dynamically wipes hidden cache structures, including DirectX Shader databases (`D3DSCache`), NVIDIA pipelines (`DXCache`), Windows Prefetch, and system thumbnail caches.
* **Secure Shredding:** Implements a high-security `ShredFile` module that overwrites destination sectors 3 times with randomized cryptographic data streams before dropping the file descriptor, permanently preventing data recovery.
* **Benchmarking Suite:** Features integrated drive write/read benchmarking (`TestDiskSpeed`), automated network download bandwidth calculation via Cloudflare's infrastructure (`MeasureDownloadSpeed`), file-hash duplicate identifiers (`HashFile`), and broken uninstall registry scanners.

### 🎮 6. Native GDI Diagnostic Game Overlay
* **Zero Performance Drop:** Includes an ultra-lightweight, hardware-transparent companion window (`GameOverlay`) that automatically renders vital health metrics over full-screen 3D games.
* **Low Overhead:** Powered by pure Win32 GDI drawing logic with configurable layouts, ensuring absolute zero frame-rate degradation during intense gaming sessions.

### 🎨 7. Next-Gen Front-End & Fluid UI Core
* **WebView2 Container:** Replaced old layouts with a web-standard reactive interface inside an isolated Microsoft Edge WebView2 core.
* **UI Responsiveness Fix (New in v9.0.0):** Refactored communication bridges using robust JavaScript `.finally()` promise chains. The UI is guaranteed to clear its `busy-state` and return to "Ready" even if background routines encounter unexpected exceptions or timeouts.
* **Internationalization:** Built-in multilingual support with seamless runtime language switching (English/Russian).
* **Flexible Styling:** Ships with flexible themes (Dark, Light, AMOLED) combined with a workspace-saving Compact Mode (Mini-UI) toggle.
* **Policy Serialization:** Embeds custom JSON serialization protocols allowing users to easily export or import optimization policies across multiple machines.

---

## 🛠 Compilation & Build Requirements

To compile the native binary from source, ensure your development workspace meets the following specifications:

* **Operating System:** Windows 10 / 11 (X64 architecture required).
* **Compiler / Toolset:** MSVC v143 or higher (Visual Studio 2022 or newer) supporting C++17/C++20 standards.
* **Windows SDK:** Windows SDK `10.0.22621.0` or newer.
* **Dependencies:** Microsoft.Web.WebView2 SDK package.
* **Privileges:** **Administrator privileges are strictly required** to run or debug the application since it interacts with system services (`sc config`), task schedulers (`schtasks`), registers, system power schemes (`powercfg`), and executes deep package management configurations.

### 🔒 Static Linking Note
The application links statically against essential Win32 subsystem libraries. Ensure your compiler linker settings include the following workspace directives:

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
