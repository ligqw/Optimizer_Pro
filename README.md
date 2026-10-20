# 🛡️ Universal Optimizer Pro

<p align="center">
  <strong>A powerful, safe, and lightweight Windows optimization & deep cleaning utility.</strong><br>
  <em>Eliminate stutters, boost FPS, lower input lag, and reclaim gigabytes of disk space.</em>
</p>

---

## ⚡ Why Optimizer Pro?
Most optimization scripts only delete `Temp` files and call it a day. **Optimizer Pro** goes deeper. It implements safe, low-level registry tweaks, flushes stubborn graphics caches, and shuts down heavy background processes to make your system feel snappier—whether you are on a 10-year-old low-end laptop or a modern gaming rig.

---

## ✨ Core Features & Real Optimization

### 🧹 Advanced Deep Cleaning & Cache Flush
* **Graphics Shader Clean:** Completely wipes hidden DirectX and NVIDIA shader caches (`DXCache` / `GLCache`) to fix sudden frame drops and micro-stutters in competitive games.
* **Browser & Client Purge:** Flushes bloated web caches from Google Chrome, Microsoft Edge, Yandex Browser, Firefox, Steam, Epic Games, and Riot Client.
* **System Log & Dump Wipe:** Wipes heavy system logs, error dumps, and Windows Update cache files that gather dust over months.

### 🚀 Low-Level Performance Tuning
* **Disable Paging Executive:** Forces Windows kernel and drivers to stay in your fast physical RAM instead of paging them to a slow HDD or SSD.
* **Ultimate Performance Plan:** Automatically unlocks and forces the hidden Windows "Ultimate Performance" power scheme, removing CPU power management limits.
* **SysMain & Indexing Stop:** Disables background tracking and indexers (`SysMain`, `WSearch`) that trigger 100% disk usage and spike your CPU.
* **Visual Optimization:** Switches the Windows GUI to maximum performance mode, removing laggy animations.

### 🌐 Network & RAM Optimization
* **Ping Stabilizer:** Fully resets the Winsock catalog and flushes DNS cache to clear congested network routes.
* **Process Working Set Flush:** Forces active processes to release unused memory back to the system safely without breaking core Windows infrastructure.

### 🛠️ Built-in System Tools
* **Windows Debloat:** One-click uninstallation of background-heavy pre-installed UWP apps (Cortana, Xbox apps, Bing Weather, Skype, etc.).
* **Smart Startup Manager:** Lists and lets you instantly delete malicious or heavy apps running in your user startup registry.
* **Integrity Scanner:** Embedded automated repair executing `SFC /scannow` and `DISM /RestoreHealth`.

---

## 🚀 How to Download & Run

1. Navigate to the [Releases](https://github.com) page.
2. Download the version that fits your needs:
   * 📦 **`Optimizer_Pro.exe` (Recommended)** – Ready-to-use app for Windows 10/11. **No Python installation required.**
   * 📄 **`Optimizer_Pro.bat`** – Native classic batch script. Ultra-lightweight fallback with zero overhead.
   * 🐍 **`Optimizer_Pro.py`** – Python source code for developers.
3. Right-click and **Run as Administrator** (The tool requires admin rights to change power plans and registry values).

---

## 🛡️ Safety & Fail-Safes
* **Automated Backups:** Before changing registry or service states, the tool backs up your original configuration. 
* **One-Click Rollback:** You can completely revert your system back to its original state by choosing **Option 7 (Oткатить изменения)** in the main menu.
* **System Restore:** If system protection is enabled on your C: drive, the tool will automatically attempt to trigger a native Windows System Restore Point.

---

## ⚠️ Antivirus Notice & False Positives
Because this program is compiled into an `.exe` file using PyInstaller and modifies system keys (Registry, Services), **Windows Defender or your web browser might flag it as a false positive**. This is an incredibly common issue with open-source system tools. 
* *The source code is 100% open, transparent, and available right here for you to audit.*

---

## 🛠️ For Developers (Manual Build)
If you want to manually build the standalone executable from the Python source code:
```bash
pip install pyinstaller
pyinstaller --onefile --uac-admin --name Optimizer_Pro Optimizer_Pro.py
```

---
<p align="center">
  Если вам помогла эта программа, пожалуйста, поставьте 🌟 <strong>Star</strong> этому репозиторию! Это очень мотивирует улучшать проект.<br>
  <em>If this project helped you, please leave a 🌟 <strong>Star</strong> to support development!</em>
</p>
