# 🛡️ Universal Optimizer Pro

**A powerful, safe, and lightweight Windows optimization and deep cleaning utility.**  
*Eliminate stutters, boost FPS, reduce input lag, and reclaim gigabytes of disk space.*

---

### ⚠️ Important Notice (.bat Deprecated)
**We have completely abandoned the `.bat` script.** 
Legacy versions written in Batch are no longer supported and have been removed from this repository. They lacked a modern interface, could not create backups, and posed potential risks to the system. 
All new features (beautiful GUI, Smart Game Boost, safe rollback) are available **only in the Python version**.

---

### ⚡ Why Optimizer Pro?

Most optimization scripts just delete the `Temp` folder and call it a day. **Optimizer Pro** goes deeper and operates safely:
*   **🛡️ 100% Safe:** Before making any registry or service changes, a backup is automatically created. You can always click "Rollback Everything" to restore your system to its original state.
*   **🎮 Smart Game Boost:** Raises the game's process priority to `HIGH` and drops heavy background apps (browsers, Discord, Telegram) to `IDLE`. System processes are left untouched to prevent mouse and desktop lag.
*   **🧹 Deep Cleaning:** Clears junk not only from Windows but also from Chrome, Discord, and Telegram caches.
*   **🗑️ Selective Debloat:** Allows you to remove built-in Windows apps. Xbox-related apps are unchecked by default to prevent breaking Game Pass.
*   **🌐 Network Reset:** Flushes DNS cache and resets the TCP/IP stack to reduce in-game ping.
*   **🎨 Modern UI:** No black CMD windows. Just a beautiful and responsive GUI.

---

### 🚀 How to Run

#### For Regular Users (Ready-to-use `.exe`)
You don't need to install anything. 
1. Go to the **[Releases](../../releases)** section on the right.
2. Download the latest `OptimizerPro.exe` file.
3. Run it as Administrator. *(Windows SmartScreen might warn you since the file lacks an expensive digital signature — click "More info" -> "Run anyway").*

#### For Developers (Run from Source)
Make sure you have **Python 3.11+** installed.

1. Clone the repository:
   ```bash
   git clone https://github.com/ligqw/auto-killer.git
   cd auto-killer
