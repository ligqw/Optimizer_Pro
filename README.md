# 🛡️ Universal Optimizer Pro

A comprehensive, safe, and lightweight system optimization tool for Windows. 

Originally a simple `.bat` script, the project has been completely rewritten in **Python** (with a legacy `.bat` version still maintained). Version 4.5.0 brings major UI fixes, real system optimizations, and advanced cleaning tools.

## 🚀 How to Download and Run

1. Go to the **[Releases](../../releases)** section.
2. Download the desired file from the latest release (see details below).
3. Run the file. **The program requires Administrator privileges** to modify system settings. 

### 📦 What to Download?

* **`Optimizer_Pro.exe`** *(Recommended)* — Ready-to-use application for modern systems (Windows 10/11). **You do NOT need to install Python.** Just download and run.
* **`Optimizer_Pro.bat`** — Classic batch script version. Perfect for older PCs (Windows 7/8) or as a lightweight fallback without any Python overhead.
* **`Optimizer_Pro.py`** — Python source code for developers (requires Python 3.x installed).

---

## ✨ Key Features (v4.5.0)

### ⚡ Real System Optimization
* **Disable Hibernation:** Frees up 4–16 GB of disk space on your C: drive by removing `hiberfil.sys`.
* **Ultimate Performance:** Unlocks and activates the hidden Windows "Ultimate Performance" power plan (removes CPU power limits).
* **Visual Effects Off:** Disables Windows animations and transparency for a snappier UI and higher FPS in games.

### 🧹 Deep Cleaning & Debloat
* **Safe Temp Cleaner:** Cleans temporary files without force-closing critical processes.
* **Game Cache Cleaner:** Flushes web caches for Steam, Epic Games, and Riot Client.
* **Empty Recycle Bin:** Automatically clears the Recycle Bin.
* **Windows Debloat:** Safely removes built-in bloatware (Bing Weather, Skype, Xbox apps, etc.).

### 🛠️ Advanced System Tools
* **Smart Startup Manager:** View programs that launch with Windows without opening Task Manager.
* **Deep Service Disabling:** Option to disable heavy background services (SysMain, WSearch, Xbox services).
* **Safe RAM Cleaner:** Clears the working sets of active applications while strictly ignoring critical Windows processes (`explorer`, `svchost`, `csrss`, etc.).
* **System Integrity Check:** Built-in execution of `SFC /scannow` and `DISM /RestoreHealth`.

### 💻 Command-Line Interface (CLI) for `.exe`
You can automate optimization using command-line flags:
* `Optimizer_Pro.exe --silent` — Runs a full safe optimization silently (great for Task Scheduler).
* `Optimizer_Pro.exe --clean --ram` — Runs specific tasks (e.g., just Temp cleaning and RAM).
* `Optimizer_Pro.exe --debloat` — Removes built-in apps.
* `Optimizer_Pro.exe --lang en` — Forces English UI.

---

## ⚠️ Important Information

> **Antivirus & Browser Warnings:** 
> Because the program is compiled with PyInstaller and requests Admin rights to modify the registry, **Windows Defender or your browser might flag the `.exe` as a false positive**. This is common for system utilities. If prompted, click "More info" -> "Run anyway" (or "Keep anyway").

> **System Restore Point:** 
> It is highly recommended to select **"1. Full Optimization"** in the main menu. The first step of the program automatically creates a Windows System Restore Point just in case.

> **Reboot:** 
> After optimization is complete, a PC reboot is highly recommended so all registry and service changes can fully apply.

---

## 🛠️ For Developers

If you want to build the `.exe` yourself from the source code:
```bash
pip install pyinstaller
pyinstaller --onefile --uac-admin --name Optimizer_Pro Optimizer_Pro.py
