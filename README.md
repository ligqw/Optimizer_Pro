# 🛡️ Universal Optimizer Pro

A comprehensive, safe, and lightweight system optimization tool for Windows.

Originally a simple `.bat` script, the project has been completely rewritten in **Python**. It now features a better user interface, error handling, and a safe rollback system, while still maintaining support for legacy systems.

## 🚀 How to Download and Run

1. Go to the **[Releases](../../releases)** section.
2. Download the desired file from the latest release (see details below).
3. Run the file. **The program will automatically request Administrator privileges** (a standard UAC prompt will appear — click "Yes").

### 📦 What to Download?

* **`Optimizer_Pro.exe`** *(Recommended)* — Ready-to-use application for modern systems (Windows 10/11). **You do NOT need to install Python.** Just download and run.
* **`Optimizer_Pro.bat`** — Classic script version. Perfect for older PCs (Windows 7/8) or if the `.exe` doesn't launch for some reason.
* **`Optimizer_Pro.py`** — Python source code for developers (requires Python 3.x installed).

---

## ✨ Key Features

* **Full or Selective Optimization**: You choose which steps to execute (Y/N prompts).
* **Safe Rollback**: If something goes wrong or you don't like the changes, select "Restore Defaults" in the menu. The program will revert services, power plans, and GameDVR settings to their original states.
* **RAM Benchmark**: The program measures free RAM *before* and *after* optimization, showing you exactly how many megabytes were freed.
* **Smart Cleaning**: Safely removes temporary files and Windows Update caches without force-closing critical background processes.
* **Integrity Check**: Built-in execution of `SFC /scannow` and `DISM /RestoreHealth`.
* **Privacy & Telemetry**: Disables tracking services for Windows, NVIDIA, and AMD, as well as background GameDVR recording.

---

## ⚠️ Important Information

> **Antivirus & Browser Warnings:** 
> Because the program is compiled with PyInstaller and requests Admin rights to modify the registry, **Windows Defender or your browser might flag the `.exe` as a false positive**. This is common for system utilities. If prompted, click "More info" -> "Run anyway" (or "Keep anyway").

> **System Restore Point:** 
> It is highly recommended to select **"1. Full Optimization"** in the main menu, as the first step of the program automatically creates a Windows System Restore Point just in case.

> **Reboot:** 
> After the optimization is complete, a PC reboot is highly recommended so all registry and service changes can fully apply.

---

## 🛠️ For Developers

If you want to build the `.exe` yourself from the source code:
```bash
pip install pyinstaller
pyinstaller --onefile --uac-admin --name Optimizer_Pro Optimizer_Pro.py
