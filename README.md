# Optimizer Pro 🚀 — v10.0.0 (Ultimate Edition)

Optimizer Pro is an ultra-high-performance system tuning, privacy hardening, and hardware monitoring utility for Windows. Re-engineered from scratch in Native C++ (Win32 API & Microsoft Edge WebView2), this version completely eliminates heavy managed runtimes (like .NET or Electron) to deliver a near-zero memory footprint, instant execution speeds, and bulletproof system-level stability.

---

## ⚡ Core Architectural & Feature Upgrades

### 🎮 1. Gaming UI Overhaul & Overclocking Suite (New in v10.0.0)
* **Cyberpunk/Gaming Aesthetic:** Complete visual redesign featuring a dark neon theme, glowing accents (`Orbitron` & `Rajdhani` fonts), custom clip-path UI elements, and animated load charts. 
* **Hidden MSI Afterburner Launch:** A dedicated Overclocking tab allows you to apply GPU overclocks completely hidden. The backend silently launches MSI Afterburner with `/Profile1 /StartMinimized`, applying your saved profile without popping up any windows.
* **CPU & RAM Boost:** Includes native toggles for `Win32PrioritySeparation` (maximizing CPU priority for games) and Windows Memory Compression management.
* **Pagefile BSOD Protection:** A smart safety mechanism for the "Disable Paging" feature. If the system detects less than 32 GB of RAM, the operation is blocked with a warning to prevent system crashes (BSOD).
* **Fully Closable Donation Modal:** The YooMoney donation widget now features a fixed close button, a bottom "Close" button, and supports `Esc` or background-click dismissal, preventing UI traps.

### 🛡️ 2. Bulletproof Stability & Thread Safety (New in v10.0.0)
* **WMI Memory Leak Fix:** Added explicit `obj.Reset()` calls before `en->Next()` across all WMI queries (`GetDiskHealthJson`, thermal zones). Long monitoring sessions will no longer accumulate memory.
* **Fixed UI Freeze on Exit:** The main monitor thread (handling WMI and hardware polling) is now safely joined and terminated, ensuring the application closes instantly without hanging in the background.
* **`DisableNagle` Infinite Loop Fixed:** The registry enumeration loop now correctly exits on `ERROR_NO_MORE_ITEMS`, preventing the app from freezing when applying low-latency network tweaks.
* **Safe File Traversal:** Wrapped `fs::recursive_directory_iterator` in `try-catch` blocks. Scanning large drives or restricted system folders will no longer result in application crashes.

### 📊 3. Thread-Safe Telemetry & Self-Resource Tracking
* **Asynchronous Telemetry:** Spawns a dedicated background thread (`MonitorThread`) to pull live system diagnostics without blocking user interaction or causing interface micro-stutters.
* **Sub-Second Accuracy:** Tracks total hardware utilization (CPU and GPU) with sub-second accuracy using native Windows PDH Performance Counters.
* **Self-Usage Metrics:** Features transparent self-resource tracking. The dashboard natively tracks and displays the app's own lightweight consumption metrics (`App CPU %`, `App RAM MB`, `App GPU %`).
* **Thermal Diagnostics:** Monitors structural thermals via granular WMI ACPI queries (`MSAcpi_ThermalZoneTemperature`).

### 📦 4. Lite Windows Debloating & Winget Integration (Restored)
* **Lite Windows Debloat:** Fully restored. Includes a powerful, aggressive debloating engine designed to strip down standard Windows into an ultra-lightweight workspace (similar to Tiny11). It permanently removes UWP bloatware (Bing Weather, Xbox, Zune, People, etc.), kills tracking pipelines, and disables background indexing (`WSearch`).
* **Winget App Manager:** Implements a single-click package installation suite. Deploy popular tools, web browsers, messaging clients, and gaming hubs (Chrome, Discord, 7-Zip, Steam, MSI Afterburner) directly from official Microsoft Winget repositories.
* **Runtime Libraries:** Quick-install buttons for essential Windows libraries including VC++ Redistributables, .NET Runtime, and Java.

### 🧹 5. Advanced Maintenance & File Shredding
* **Deep Cache Purge:** Dynamically wipes hidden cache structures, including DirectX Shader databases (`D3DSCache`), NVIDIA pipelines (`DXCache`), Windows Prefetch, Delivery Optimization cache, and system thumbnail caches.
* **Secure Shredding:** Implements a high-security `ShredFile` module that overwrites destination sectors with randomized data streams before dropping the file descriptor.
* **Benchmarking Suite:** Features integrated drive write/read benchmarking, automated network download bandwidth calculation via Cloudflare, file-hash duplicate identifiers, and broken uninstall registry scanners.

### 🎨 6. Next-Gen Front-End & Fluid UI Core
* **Full Localization:** Built-in multilingual support with seamless runtime language switching between **Russian** and **English** via the Settings tab.
* **UI Responsiveness:** Refactored communication bridges using robust JavaScript `.finally()` promise chains. The UI is guaranteed to clear its `busy-state` and return to "Ready" even if background routines encounter unexpected exceptions or timeouts.
* **Flexible Styling:** Ships with flexible themes (Dark, Light) combined with a workspace-saving Compact Mode toggle.
* **JSON Policy Serialization:** Embeds custom JSON serialization protocols allowing users to easily export or import optimization policies across multiple machines.

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
