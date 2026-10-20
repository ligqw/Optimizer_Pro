#!/usr/bin/env python3
# Optimizer Pro - GUI Edition (v3.4 - Bugfix Release)
import sys, os

if sys.platform != "win32":
    print("Эта программа предназначена только для Windows.")
    sys.exit(1)

if sys.stdout is None:
    sys.stdout = open(os.devnull, 'w', encoding='utf-8')
if sys.stderr is None:
    sys.stderr = open(os.devnull, 'w', encoding='utf-8')

import subprocess, ctypes, time, shutil, threading, stat, base64, re, json, msvcrt, winreg
from ctypes import wintypes

if '--console' not in sys.argv:
    try:
        ctypes.windll.user32.ShowWindow(ctypes.windll.kernel32.GetConsoleWindow(), 0)
    except Exception:
        pass

try:
    import psutil
    PSUTIL_AVAILABLE = True
except Exception:
    PSUTIL_AVAILABLE = False

HAS_WEBVIEW = False
if '--console' not in sys.argv:
    try:
        import webview
        HAS_WEBVIEW = True
    except Exception:
        pass

UI_MODE = HAS_WEBVIEW

# ============================================================
# КОНСТАНТЫ И ПУТИ
# ============================================================
_current_proc = None
_proc_lock = threading.RLock()
_boost_lock = threading.RLock()

SYSTEM_ROOT = os.environ.get('SystemRoot', r'C:\Windows')
BACKUP_DIR = os.path.join(os.environ.get('APPDATA') or os.path.expanduser('~'), 'OptimizerPro')
BACKUP_FILE = os.path.join(BACKUP_DIR, "backup.json")
BOOST_STATE_FILE = os.path.join(BACKUP_DIR, "game_boost_state.json")
LOG_FILE = os.path.join(BACKUP_DIR, "optimization_log.txt")
SYSTEM_DRIVE = os.environ.get('SystemDrive', 'C:') + "\\"
CREATE_NO_WINDOW = 0x08000000
LOG_MAX_SIZE = 5 * 1024 * 1024  # 5 МБ

ULTIMATE_GUID = "e9a42b02-d5df-448d-aa00-03f14749eb61"
HIGH_PERF_GUID = "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"

IS_WOW64 = sys.maxsize <= 2**32 and bool(os.environ.get("PROCESSOR_ARCHITEW6432"))
NATIVE_SYS32 = os.path.join(SYSTEM_ROOT, "Sysnative" if IS_WOW64 else "System32")
PS_EXE = os.path.join(NATIVE_SYS32, 'WindowsPowerShell', 'v1.0', 'powershell.exe')
if not os.path.exists(PS_EXE):
    PS_EXE = shutil.which("powershell.exe") or PS_EXE

ORIG_USER_TEMP = None
if "--orig-temp" in sys.argv:
    idx = sys.argv.index("--orig-temp")
    if idx + 1 < len(sys.argv):
        ORIG_USER_TEMP = sys.argv[idx + 1]
        del sys.argv[idx:idx + 2]
    else:
        del sys.argv[idx]

try:
    cp = ctypes.windll.kernel32.GetOEMCP()
    OEM_ENCODING = f"cp{cp}" if cp else "cp866"
except Exception:
    OEM_ENCODING = "cp866"

try:
    os.makedirs(BACKUP_DIR, exist_ok=True)
except Exception as e:
    print(f"Не удалось создать папку бэкапа: {e}")

# ============================================================
# УТИЛИТЫ
# ============================================================
def write_log(text):
    try:
        try:
            if os.path.getsize(LOG_FILE) > LOG_MAX_SIZE:
                old = LOG_FILE + ".old"
                try: os.remove(old)
                except Exception: pass
                try: os.replace(LOG_FILE, old)
                except Exception: pass
        except FileNotFoundError:
            pass
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"{text}\n")
    except Exception:
        pass

def safe_input(prompt="", default=""):
    if UI_MODE: return default
    try: return input(prompt)
    except Exception: return default

def is_admin():
    try: return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception: return False

def run_as_admin():
    try:
        s = ctypes.windll.shell32
        s.ShellExecuteW.restype = wintypes.HINSTANCE
        s.ShellExecuteW.argtypes = [wintypes.HWND, wintypes.LPCWSTR, wintypes.LPCWSTR,
                                    wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_int]
        args = ([os.path.abspath(sys.argv[0])] if not getattr(sys, 'frozen', False) else []) + sys.argv[1:]
        if "--elevated" not in args:
            args.append("--elevated")
        if os.environ.get("TEMP"):
            args += ["--orig-temp", os.environ["TEMP"]]
        res = s.ShellExecuteW(None, "runas", sys.executable, subprocess.list2cmdline(args), None, 1)
        return (res or 0) > 32
    except Exception:
        return False

def _res_ok(res):
    if isinstance(res, dict): return bool(res.get("ok"))
    return bool(res)

def _res_err(res):
    if isinstance(res, dict) and res.get("error"): return str(res["error"])
    return "Операция завершилась с ошибкой"

# ============================================================
# ПРОЦЕССЫ И КОМАНДЫ
# ============================================================
def _set_proc(p):
    global _current_proc
    with _proc_lock:
        _current_proc = p

def _kill_proc(p=None):
    global _current_proc
    with _proc_lock:
        if p is None:
            p = _current_proc
        _current_proc = None
    if not p or p.poll() is not None:
        return
    try:
        subprocess.run(f"taskkill /f /t /pid {p.pid}", shell=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       creationflags=CREATE_NO_WINDOW, timeout=5)
    except Exception:
        pass

def _run_impl(cmd, timeout, enc, capture=True):
    if not cmd or not str(cmd).strip():
        return -1, "", "Empty command", False
    try:
        p = subprocess.Popen(cmd, shell=True,
                             stdout=subprocess.PIPE if capture else subprocess.DEVNULL,
                             stderr=subprocess.PIPE, text=True, encoding=enc, errors='replace',
                             creationflags=CREATE_NO_WINDOW, stdin=subprocess.DEVNULL)
    except Exception as e:
        return -1, "", str(e), False

    _set_proc(p)
    try:
        dl = time.time() + timeout
        while True:
            rem = dl - time.time()
            if rem <= 0:
                _kill_proc(p)
                try: out, err = p.communicate(timeout=5)
                except Exception: out, err = "", ""
                return (p.returncode if p.returncode is not None else -1), out or "", err or "", True
            try:
                out, err = p.communicate(timeout=min(0.5, rem))
                return p.returncode, out or "", err or "", False
            except subprocess.TimeoutExpired:
                continue
    finally:
        _set_proc(None)

def run_cmd(cmd, text=None, check=True, timeout=300, allowed=None):
    allowed = allowed or [0]
    if text:
        print(f"  [+] {text}...")
        write_log(f"  {text}")
    try:
        rc, out, err, tm = _run_impl(cmd, timeout, OEM_ENCODING)
        if tm:
            print("      +-- [ОШИБКА] Таймаут")
            write_log(f"      [ОШИБКА] Таймаут: {cmd}")
            return False
        if rc in allowed:
            if text: print("      +-- [OK]")
            return True
        detail = str((err or "").strip() or (out or "").strip() or f"Код {rc}")
        print(f"      +-- [{'ОШИБКА' if check else 'ПРЕДУПРЕЖДЕНИЕ'}] {detail[:80]}")
        write_log(f"      [ОШИБКА] {cmd}: {detail}")
        return False
    except Exception as e:
        print(f"      +-- [ОШИБКА] {e}")
        return False

def remove_readonly(func, path, exc_info):
    if not os.path.exists(path): return
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception: pass

# ============================================================
# ОЗУ
# ============================================================
class MEMORYSTATUSEX(ctypes.Structure):
    _fields_ = [
        ("dwLength", wintypes.DWORD), ("dwMemoryLoad", wintypes.DWORD),
        ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
        ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
        ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
        ("ullAvailExtendedVirtual", ctypes.c_ulonglong)
    ]

def get_free_ram():
    try:
        s = MEMORYSTATUSEX()
        s.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
        if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(s)):
            return 0
        return s.ullAvailPhys // (1024 * 1024)
    except Exception:
        return 0

# ============================================================
# БЭКАПЫ
# ============================================================
class BackupLock:
    """Кросс-процессная (msvcrt) и кросс-поточная (RLock) блокировка файла бэкапа.
    Поддерживает реентерабельность через thread-local глубину."""
    _lock = threading.RLock()
    _state = threading.local()
    _file = None

    def __init__(self, timeout=15):
        self.p = BACKUP_FILE + ".lock"
        self.t = timeout

    def __enter__(self):
        if not BackupLock._lock.acquire(timeout=self.t):
            raise TimeoutError("Не удалось заблокировать файл бэкапа")
        try:
            depth = getattr(BackupLock._state, 'depth', 0)
            if depth > 0:
                BackupLock._state.depth = depth + 1
                return self
            end = time.time() + self.t
            while time.time() < end:
                f = None
                try:
                    os.makedirs(BACKUP_DIR, exist_ok=True)
                    f = open(self.p, "a+b")
                    f.seek(0)
                    msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
                    BackupLock._file = f
                    BackupLock._state.depth = 1
                    return self
                except OSError:
                    if f:
                        try: f.close()
                        except Exception: pass
                    time.sleep(0.1)
            raise TimeoutError("Не удалось заблокировать файл бэкапа")
        except Exception:
            BackupLock._lock.release()
            raise

    def __exit__(self, *a):
        depth = getattr(BackupLock._state, 'depth', 1) - 1
        BackupLock._state.depth = depth
        if depth <= 0:
            f = BackupLock._file
            BackupLock._file = None
            if f:
                try:
                    f.seek(0)
                    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
                except Exception: pass
                try: f.close()
                except Exception: pass
        BackupLock._lock.release()
        return False

def _atomic_json(path, data):
    try:
        with open(path + ".tmp", "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.flush(); os.fsync(f.fileno())
        os.replace(path + ".tmp", path)
        return True
    except Exception as e:
        write_log(f"Ошибка записи JSON: {e}")
        return False

def _read_backup():
    try:
        with open(BACKUP_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}

def _is_valid_backup(b):
    if not isinstance(b, dict): return False
    if b.get("version") != 2: return False
    if not isinstance(b.get("services"), dict): return False
    if not isinstance(b.get("created_schemes", []), list): return False
    if not isinstance(b.get("removed_appx", []), list): return False
    if not isinstance(b.get("visual_fx_exists"), bool): return False
    if not isinstance(b.get("hibernate"), bool): return False
    return True

def _is_boost_valid():
    if not os.path.exists(BOOST_STATE_FILE): return False
    try:
        with open(BOOST_STATE_FILE, 'r', encoding='utf-8') as f:
            st = json.load(f)
        return isinstance(st, dict) and "game_name" in st and "priorities" in st
    except Exception:
        return False

def _get_scheme():
    try:
        out = subprocess.check_output("powercfg /getactivescheme", shell=True, text=True,
                                      encoding=OEM_ENCODING, creationflags=CREATE_NO_WINDOW, timeout=15)
        m = re.search(r'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}', out)
        return m.group(0) if m else None
    except Exception:
        return None

def _get_all_schemes():
    try:
        out = subprocess.check_output("powercfg /list", shell=True, text=True,
                                      encoding=OEM_ENCODING, creationflags=CREATE_NO_WINDOW, timeout=15)
        return set(g.lower() for g in re.findall(
            r'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}', out))
    except Exception:
        return set()

_RE_SVC = re.compile(r':\s*\d+\s+(STOPPED|START_PENDING|STOP_PENDING|RUNNING|CONTINUE_PENDING|PAUSE_PENDING|PAUSED)\b')

def _svc_running(name):
    if PSUTIL_AVAILABLE:
        try:
            s = psutil.win_service_get(name)
            return s.status() == 'running'
        except Exception:
            pass
    try:
        out = subprocess.check_output(f"sc query {name}", shell=True, text=True,
                                      encoding=OEM_ENCODING, creationflags=CREATE_NO_WINDOW, timeout=10)
        m = _RE_SVC.search(out)
        return (m.group(1) == "RUNNING") if m else None
    except Exception:
        return None

def _reg_start(svc):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, rf"SYSTEM\CurrentControlSet\Services\{svc}") as k:
            try: st, _ = winreg.QueryValueEx(k, "Start")
            except FileNotFoundError: st = 3
            try: dl, _ = winreg.QueryValueEx(k, "DelayedAutostart")
            except FileNotFoundError: dl = 0
        return int(st), int(dl)
    except Exception:
        return None

def _st_type(s, d):
    if s == 2 and d: return "delayed-auto"
    return {0: "boot", 1: "system", 2: "auto", 3: "demand", 4: "disabled"}.get(s, "demand")

def _hibernate_enabled():
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Power") as k:
            return bool(winreg.QueryValueEx(k, "HibernateEnabled")[0])
    except Exception:
        return os.path.exists(os.path.join(SYSTEM_DRIVE, "hiberfil.sys"))

def save_backup():
    print("  [+] Создание резервной копии...")
    try:
        with BackupLock():
            if os.path.exists(BACKUP_FILE):
                old = _read_backup()
                if _is_valid_backup(old):
                    print("      +-- [ПРОПУЩЕНО] Бэкап уже существует.")
                    return True

        b = {"version": 2, "created_schemes": [], "removed_appx": [], "services": {}}

        active = _get_scheme()
        if active: b["active_scheme"] = active

        for svc in ("DiagTrack", "SysMain", "WSearch"):
            r = _reg_start(svc)
            run = _svc_running(svc)
            if r and run is not None:
                b["services"][svc] = {"start_type": _st_type(*r), "is_running": run}

        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                r"Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects") as k:
                try:
                    b["visual_fx"] = int(winreg.QueryValueEx(k, "VisualFXSetting")[0])
                    b["visual_fx_exists"] = True
                except FileNotFoundError:
                    b["visual_fx_exists"] = False
        except FileNotFoundError:
            b["visual_fx_exists"] = False
        except Exception:
            b["visual_fx_exists"] = False

        b["hibernate"] = _hibernate_enabled()

        with BackupLock():
            if os.path.exists(BACKUP_FILE):
                old = _read_backup()
                if _is_valid_backup(old):
                    return True
            ok = _atomic_json(BACKUP_FILE, b)
            if ok:
                print("      +-- [OK]")
            return ok
    except Exception as e:
        print(f"  [!] Ошибка бэкапа: {e}")
        return False

def restore_backup():
    print("  [+] Восстановление...")
    try:
        with BackupLock():
            if not os.path.exists(BACKUP_FILE):
                return {"ok": False, "error": "Бэкап не найден"}
            b = _read_backup()
            if not _is_valid_backup(b):
                if isinstance(b, dict) and b.get("version") == 1:
                    print("  [!] Старый бэкап (v1) несовместим — удалите файл вручную.")
                    return {"ok": False, "error": "Старый бэкап (v1) несовместим"}
                print("  [!] Бэкап повреждён или неполный — откат невозможен.")
                return {"ok": False, "error": "Бэкап повреждён"}

        errors = []

        if b.get("active_scheme"):
            if not run_cmd(f'powercfg /setactive {b["active_scheme"]}', "Схема питания", False):
                errors.append("схема питания")

        for g in (b.get("created_schemes") or []):
            if not run_cmd(f'powercfg /delete {g}', None, False):
                errors.append(f"удаление схемы {g[:8]}")

        for svc, d in (b.get("services") or {}).items():
            st = d.get("start_type", "auto")
            if not run_cmd(f"sc config {svc} start= {st}", f"Служба {svc}", False):
                errors.append(f"служба {svc} (config)")
            if not run_cmd(f"sc {'start' if d.get('is_running') else 'stop'} {svc}",
                           None, False, allowed=[0, 1056, 1062]):
                errors.append(f"служба {svc} (state)")

        if "visual_fx_exists" in b:
            if b.get("visual_fx_exists") and b.get("visual_fx") is not None:
                if not run_cmd(
                    f'reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" '
                    f'/v "VisualFXSetting" /t REG_DWORD /d {b["visual_fx"]} /f', "Эффекты", False):
                    errors.append("визуальные эффекты")
            elif not b.get("visual_fx_exists"):
                run_cmd(
                    'reg delete "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" '
                    '/v "VisualFXSetting" /f', None, False, allowed=[0, 1])

        if b.get("hibernate") is not None:
            if not run_cmd(f"powercfg -h {'on' if b['hibernate'] else 'off'}", "Гибернация", False):
                errors.append("гибернация")

        if b.get("removed_appx"):
            print(f"  [!] Приложения для ручной проверки: {', '.join(b['removed_appx'])}")

        if not errors:
            print("      +-- [OK]")
            try:
                with BackupLock():
                    os.remove(BACKUP_FILE)
            except Exception:
                pass
            return {"ok": True}
        else:
            short = "; ".join(errors[:3])
            print(f"      +-- [ОШИБКА] Не всё восстановлено: {short}")
            return {"ok": False, "error": f"Не всё восстановлено: {short}"}
    except Exception as e:
        print(f"  [!] Ошибка отката: {e}")
        return {"ok": False, "error": str(e)}

# ============================================================
# ОПТИМИЗАЦИЯ
# ============================================================
def enable_ultimate():
    before = _get_all_schemes()
    res = _run_impl(f'powercfg -duplicatescheme {ULTIMATE_GUID}', 15, OEM_ENCODING)
    if res and res[0] == 0:
        new = _get_all_schemes() - before
        if new:
            g = next(iter(new))
            recorded = True
            try:
                with BackupLock():
                    if os.path.exists(BACKUP_FILE):
                        b = _read_backup()
                        if not isinstance(b.get("created_schemes"), list):
                            b["created_schemes"] = []
                        if g not in b["created_schemes"]:
                            b["created_schemes"].append(g)
                            if not _atomic_json(BACKUP_FILE, b):
                                recorded = False
            except Exception:
                recorded = False
            if not recorded:
                print("  [!] Не удалось записать схему в бэкап — активация отменена")
                run_cmd(f'powercfg /delete {g}', None, False)
                return False
            return run_cmd(f'powercfg /setactive {g}', "Ultimate", False)
    return run_cmd(f'powercfg /setactive {HIGH_PERF_GUID}', "High Perf", False)

def clean_temp_and_cache(clean_browsers=False, clean_temp=True):
    success = True
    if clean_temp:
        print("  [+] Очистка Temp...")
        paths = [p for p in (os.environ.get('TEMP'), ORIG_USER_TEMP, os.path.join(SYSTEM_ROOT, 'Temp'))
                 if p and os.path.isdir(p) and len(p) > 3]
        if not paths:
            print("      +-- [ПРЕДУПРЕЖДЕНИЕ] Папки Temp не найдены")
            success = False
        else:
            any_listed = False
            for p in paths:
                try: files = os.listdir(p)
                except Exception: continue
                any_listed = True
                for f in files:
                    try:
                        fp = os.path.join(p, f)
                        if os.path.isdir(fp): shutil.rmtree(fp, onerror=remove_readonly)
                        else: os.unlink(fp)
                    except Exception: pass
            if not any_listed:
                success = False

    if clean_browsers:
        print("  [+] Очистка кэша браузеров...")
        local = os.environ.get('LOCALAPPDATA', '')
        roaming = os.environ.get('APPDATA', '')
        cache_paths = [
            os.path.join(local, r"Google\Chrome\User Data\Default\Cache"),
            os.path.join(local, r"Google\Chrome\User Data\Default\Code Cache"),
            os.path.join(roaming, r"discord\Cache"),
            os.path.join(roaming, r"discord\Code Cache"),
            os.path.join(roaming, r"discord\GPUCache"),
            os.path.join(roaming, r"Telegram Desktop\tdata\user_data\cache"),
        ]
        for cp in cache_paths:
            if os.path.isdir(cp):
                try:
                    for f in os.listdir(cp):
                        fp = os.path.join(cp, f)
                        if os.path.isdir(fp): shutil.rmtree(fp, onerror=remove_readonly)
                        else: os.unlink(fp)
                except Exception:
                    success = False
    return success

def reset_network():
    print("  [+] Сброс сети...")
    errors = []
    if not run_cmd("ipconfig /flushdns", "Очистка DNS"): errors.append("DNS")
    if not run_cmd("netsh winsock reset", "Сброс Winsock"): errors.append("Winsock")
    if not run_cmd("netsh int ip reset", "Сброс IP"): errors.append("IP")
    if errors:
        return {"ok": False, "error": "Не удалось: " + ", ".join(errors)}
    return {"ok": True}

_APPX_NAME_RE = re.compile(r'^[A-Za-z0-9._\-]+$')

def debloat_selected(apps_to_remove):
    if not PS_EXE or not os.path.exists(PS_EXE):
        print("  [!] PowerShell не найден")
        return {"ok": False, "error": "PowerShell не найден"}

    if not save_backup():
        print("  [!] Не удалось создать бэкап — debloat отменён")
        return {"ok": False, "error": "Не удалось создать бэкап"}

    print(f"  [+] Debloat: {len(apps_to_remove)} приложений...")
    failed = []
    removed = []
    for a in apps_to_remove:
        if not isinstance(a, str) or not _APPX_NAME_RE.match(a):
            print(f"  [!] Пропущено небезопасное имя: {a!r}")
            failed.append(str(a))
            continue
        s = (f"$x=Get-AppxPackage *{a}* -AllUsers -EA 0; "
             f"$y=Get-AppxProvisionedPackage -Online -EA 0 | Where {{$_.DisplayName -like '*{a}*'}}; "
             f"if($x){{$x | Remove-AppxPackage -AllUsers -EA 0}}; "
             f"if($y){{$y | Remove-AppxProvisionedPackage -Online -EA 0 | Out-Null}}")
        cmd = f'"{PS_EXE}" -EP Bypass -NoP -NonInteractive -Enc {base64.b64encode(s.encode("utf-16le")).decode()}'
        res = _run_impl(cmd, 120, OEM_ENCODING, False)
        if res and res[0] == 0: removed.append(a)
        else: failed.append(a)

    if removed:
        try:
            with BackupLock():
                if os.path.exists(BACKUP_FILE):
                    b = _read_backup()
                    if not isinstance(b.get("removed_appx"), list):
                        b["removed_appx"] = []
                    b["removed_appx"].extend(removed)
                    _atomic_json(BACKUP_FILE, b)
        except Exception as e:
            print(f"  [!] Не удалось сохранить список Appx: {e}")

    if failed:
        return {"ok": False, "error": f"Не удалено: {len(failed)} из {len(apps_to_remove)}"}
    return {"ok": True}

# ============================================================
# GAME BOOST
# ============================================================
HEAVY_APPS = {
    "chrome.exe", "firefox.exe", "msedge.exe", "opera.exe", "brave.exe", "vivaldi.exe",
    "discord.exe", "telegram.exe", "whatsapp.exe", "skype.exe", "slack.exe",
    "obs64.exe", "obs32.exe", "spotify.exe", "thunderbird.exe", "steam.exe", "epicgameslauncher.exe"
}

def _empty_working_set(pid):
    try:
        k32 = ctypes.WinDLL('kernel32', use_last_error=True)
        psapi = ctypes.WinDLL('psapi', use_last_error=True)
        k32.OpenProcess.restype = wintypes.HANDLE
        k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        k32.CloseHandle.argtypes = [wintypes.HANDLE]
        psapi.EmptyWorkingSet.argtypes = [wintypes.HANDLE]
        h = k32.OpenProcess(0x0100 | 0x0400, False, pid)
        if h:
            psapi.EmptyWorkingSet(h)
            k32.CloseHandle(h)
    except Exception:
        pass

def _restore_boost_state_file():
    """Восстановить предыдущее состояние буста (если есть) и удалить файл."""
    if not os.path.exists(BOOST_STATE_FILE):
        return
    try:
        with open(BOOST_STATE_FILE, "r", encoding="utf-8") as f:
            old = json.load(f)
        if isinstance(old, dict):
            gpid = old.get("game_pid")
            gprio = old.get("game_prio")
            if gpid is not None and gprio is not None:
                try: psutil.Process(gpid).nice(gprio)
                except Exception: pass
            old_prios = old.get("priorities")
            if isinstance(old_prios, dict):
                for pid_str, info in old_prios.items():
                    try:
                        psutil.Process(int(pid_str)).nice(info.get("nice", psutil.NORMAL_PRIORITY_CLASS))
                    except Exception: pass
    except Exception:
        pass
    try: os.remove(BOOST_STATE_FILE)
    except Exception: pass

def game_boost(game_name):
    if not PSUTIL_AVAILABLE:
        return {"ok": False, "error": "psutil не установлен"}
    if not isinstance(game_name, str) or not game_name.strip():
        return {"ok": False, "error": "Пустое имя процесса"}

    with _boost_lock:
        name_norm = game_name.strip().lower()
        if not name_norm.endswith(".exe"):
            name_norm += ".exe"

        # Найти игру и тяжёлые процессы за один проход
        game_pid = None
        game_prio = None
        saved = {}
        for p in psutil.process_iter(['pid', 'name']):
            try:
                n = p.info['name'].lower()
                if n == name_norm:
                    if game_pid is None:
                        game_pid = p.info['pid']
                        try: game_prio = p.nice()
                        except Exception: game_prio = None
                elif n in HEAVY_APPS:
                    try:
                        saved[str(p.info['pid'])] = {"name": p.info['name'], "nice": p.nice()}
                    except Exception:
                        pass
            except Exception:
                continue

        if game_pid is None:
            print(f"  [!] Процесс '{name_norm}' не найден.")
            return {"ok": False, "error": f"Процесс '{name_norm}' не найден"}

        # Восстановить старый state (если был активен) — не теряем предыдущие приоритеты
        _restore_boost_state_file()

        print(f"  [+] Оптимизация {name_norm} (PID {game_pid})...")

        state = {
            "game_pid": game_pid,
            "game_name": name_norm,
            "game_prio": game_prio,
            "priorities": saved,
        }
        if not _atomic_json(BOOST_STATE_FILE, state):
            print("  [!] Не удалось сохранить состояние — откат невозможен. Операция отменена.")
            return {"ok": False, "error": "Не удалось сохранить состояние"}

        # Очистка working set у игры и тяжёлых процессов
        for p in psutil.process_iter(['pid', 'name', 'memory_info']):
            try:
                if p.info['pid'] == game_pid or p.info['name'].lower() in HEAVY_APPS:
                    mi = p.info.get('memory_info')
                    if mi and mi.rss > 50 * 1024 * 1024:
                        _empty_working_set(p.info['pid'])
            except Exception:
                continue

        # Приоритеты
        try:
            psutil.Process(game_pid).nice(psutil.HIGH_PRIORITY_CLASS)
        except Exception:
            pass

        for p in psutil.process_iter(['pid', 'name']):
            try:
                if p.info['name'].lower() in HEAVY_APPS:
                    p.nice(psutil.IDLE_PRIORITY_CLASS)
            except Exception:
                pass

        print("      +-- [OK] Приоритеты расставлены")
        return {"ok": True}

def restore_priorities():
    if not PSUTIL_AVAILABLE:
        return {"ok": False, "error": "psutil не установлен"}
    with _boost_lock:
        if not os.path.exists(BOOST_STATE_FILE):
            return {"ok": False, "error": "Нет активного Game Boost"}
        try:
            with open(BOOST_STATE_FILE, "r", encoding="utf-8") as f:
                state = json.load(f)
        except Exception as e:
            print(f"  [!] Не удалось прочитать состояние: {e}")
            try: os.remove(BOOST_STATE_FILE)
            except Exception: pass
            return {"ok": False, "error": "Состояние повреждено"}

        if not isinstance(state, dict):
            try: os.remove(BOOST_STATE_FILE)
            except Exception: pass
            return {"ok": False, "error": "Состояние повреждено"}

        gpid = state.get("game_pid")
        gprio = state.get("game_prio")
        if gpid is not None and gprio is not None:
            try: psutil.Process(gpid).nice(gprio)
            except Exception: pass

        prios = state.get("priorities")
        if isinstance(prios, dict):
            for pid_str, info in prios.items():
                try:
                    psutil.Process(int(pid_str)).nice(info.get("nice", psutil.NORMAL_PRIORITY_CLASS))
                except Exception:
                    pass

        try: os.remove(BOOST_STATE_FILE)
        except Exception: pass
        print("  [+] Приоритеты восстановлены.")
        return {"ok": True}

# ============================================================
# АВТОЗАГРУЗКА
# ============================================================
_appdata = os.environ.get('APPDATA')
_programdata = os.environ.get('ProgramData')
STARTUP_DIRS = []
if _appdata:
    STARTUP_DIRS.append((os.path.join(_appdata, r"Microsoft\Windows\Start Menu\Programs\Startup"),
                         "Папка (Пользователь)", "folder_user"))
if _programdata:
    STARTUP_DIRS.append((os.path.join(_programdata, r"Microsoft\Windows\Start Menu\Programs\Startup"),
                         "Папка (Система)", "folder_sys"))

def get_full_autostart():
    items = []
    for hive, src, typ in [(winreg.HKEY_CURRENT_USER, "Пользователь (HKCU)", "reg_hkcu"),
                           (winreg.HKEY_LOCAL_MACHINE, "Система (HKLM)", "reg_hklm")]:
        try:
            with winreg.OpenKey(hive, r"Software\Microsoft\Windows\CurrentVersion\Run") as k:
                i = 0
                while True:
                    try:
                        n, v, _ = winreg.EnumValue(k, i)
                        items.append({"name": n, "path": v, "source": src, "type": typ})
                        i += 1
                    except OSError: break
        except Exception:
            pass

    for d, src, typ in STARTUP_DIRS:
        if not os.path.isdir(d):
            continue
        try:
            files = os.listdir(d)
        except Exception:
            continue
        for f in files:
            lower = f.lower()
            if lower.endswith('.lnk') or lower.endswith('.exe'):
                items.append({"name": f, "path": os.path.join(d, f), "source": src, "type": typ})
    return items

def delete_autostart_item(item):
    if not isinstance(item, dict): return False
    try:
        if item.get('type') == 'reg_hkcu':
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                r"Software\Microsoft\Windows\CurrentVersion\Run",
                                0, winreg.KEY_SET_VALUE) as k:
                winreg.DeleteValue(k, item['name'])
            return True
        if item.get('type') == 'reg_hklm':
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                                r"Software\Microsoft\Windows\CurrentVersion\Run",
                                0, winreg.KEY_SET_VALUE) as k:
                winreg.DeleteValue(k, item['name'])
            return True
        if item.get('type') in ('folder_user', 'folder_sys'):
            valid_dir = next((d for d, s, t in STARTUP_DIRS if t == item['type']), None)
            if not valid_dir: return False
            try:
                valid_abs = os.path.normcase(os.path.abspath(valid_dir))
                item_abs = os.path.normcase(os.path.abspath(item['path']))
                if os.path.dirname(item_abs) != valid_abs:
                    return False
            except Exception:
                return False
            try: os.chmod(item_abs, stat.S_IWRITE)
            except Exception: pass
            os.remove(item_abs)
            return True
        return False
    except Exception:
        return False

# ============================================================
# HTML UI
# ============================================================
HTML_UI = r"""<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<style>
:root{--bg:#0e1116;--bg2:#161b22;--surface:#1c2229;--surface2:#242c35;--border:#2d3641;--text:#e6edf3;--text-dim:#8b949e;--accent:#4f8cff;--success:#3fb950;--warning:#d29922;--error:#f85149}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%}
body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);overflow:hidden;user-select:none;font-size:14px}
.app{display:grid;grid-template-columns:220px 1fr;height:100vh;opacity:0;animation:fadeIn 0.4s ease-out forwards}
@keyframes fadeIn{to{opacity:1}}
@keyframes slideUp{from{opacity:0;transform:translateY(15px)}to{opacity:1;transform:translateY(0)}}
@keyframes slideInRight{from{opacity:0;transform:translateX(20px)}to{opacity:1;transform:translateX(0)}}
@keyframes pulse{50%{opacity:.3}}
@keyframes spin{to{transform:rotate(360deg)}}
.sidebar{background:var(--bg2);border-right:1px solid var(--border);padding:20px 0;display:flex;flex-direction:column}
.logo{padding:0 20px 20px;font-size:18px;font-weight:700;color:var(--accent)}
.logo::before{content:"⚡ "}
.nav-item{padding:12px 20px;cursor:pointer;color:var(--text-dim);border-left:3px solid transparent;transition:all .2s ease;display:flex;align-items:center;gap:12px}
.nav-item:hover{background:var(--surface);color:var(--text);padding-left:24px}
.nav-item.active{background:var(--surface);color:var(--accent);border-left-color:var(--accent)}
.nav-spacer{flex:1}
.nav-danger{color:var(--error)}
.nav-danger.active{color:var(--error);border-left-color:var(--error)}
.main{display:flex;flex-direction:column;overflow:hidden}
.content{flex:1;overflow-y:auto;padding:30px 36px}
.content::-webkit-scrollbar{width:10px}
.content::-webkit-scrollbar-thumb{background:var(--border);border-radius:5px}
.tab{display:none}
.tab.active{display:block;animation:fadeIn 0.3s ease-out}
h1{font-size:24px;margin-bottom:6px}
h2{font-size:16px;margin:24px 0 12px;color:var(--text)}
.subtitle{color:var(--text-dim);margin-bottom:24px;font-size:13px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:24px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:16px;transition:transform .2s,box-shadow .2s;opacity:0;animation:slideUp 0.4s ease-out forwards}
.card:hover{transform:translateY(-3px);box-shadow:0 6px 15px rgba(0,0,0,0.3)}
.card:nth-child(1){animation-delay:0.05s}.card:nth-child(2){animation-delay:0.1s}.card:nth-child(3){animation-delay:0.15s}.card:nth-child(4){animation-delay:0.2s}
.card-label{color:var(--text-dim);font-size:11px;margin-bottom:6px;text-transform:uppercase}
.card-value{font-size:20px;font-weight:600}
.card-value.good{color:var(--success)}.card-value.bad{color:var(--error)}.card-value.warn{color:var(--warning)}
.section{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:20px;margin-bottom:16px;animation:slideUp 0.4s ease-out}
.check-item{display:flex;align-items:center;padding:10px 0;border-bottom:1px solid var(--border);cursor:pointer;transition:all .2s}
.check-item:hover{background:var(--surface2);padding-left:10px;border-radius:6px}
.check-item:last-child{border-bottom:none}
.check-item input{margin-right:12px;width:18px;height:18px;accent-color:var(--accent);cursor:pointer}
.check-item label{flex:1;cursor:pointer}
.check-item .desc{color:var(--text-dim);font-size:12px;margin-top:2px}
button{background:var(--accent);color:#fff;border:none;padding:10px 20px;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;transition:all .2s;font-family:inherit;display:inline-flex;align-items:center;gap:8px}
button:hover:not(:disabled){background:#3a6ecb;transform:translateY(-1px);box-shadow:0 4px 12px rgba(79,140,255,0.3)}
button:active:not(:disabled){transform:scale(0.97)}
button:disabled{opacity:.5;cursor:not-allowed}
button.danger{background:var(--error)}
button.danger:hover:not(:disabled){background:#c94040}
button.secondary{background:var(--surface2);color:var(--text)}
button.secondary:hover:not(:disabled){background:var(--border);box-shadow:none}
.btn-spinner{width:14px;height:14px;border:2px solid rgba(255,255,255,0.3);border-top-color:#fff;border-radius:50%;animation:spin 1s linear infinite;display:none}
button.loading .btn-spinner{display:block}
button.loading{opacity:0.8;pointer-events:none}
.input-row{display:flex;gap:10px;margin-bottom:12px}
input[type="text"]{flex:1;background:var(--bg2);border:1px solid var(--border);color:var(--text);padding:10px 14px;border-radius:8px;font-size:14px;outline:none;transition:all .2s}
input[type="text"]:focus{border-color:var(--accent);box-shadow:0 0 0 2px rgba(79,140,255,0.2)}
.log{background:#0a0d12;border:1px solid var(--border);border-radius:8px;padding:12px;font-family:monospace;font-size:12.5px;line-height:1.6;height:100%;overflow-y:auto;white-space:pre-wrap;user-select:text}
.log-line{padding:2px 0;animation:fadeIn 0.2s}
.log-line.err{color:var(--error)}.log-line.ok{color:var(--success)}.log-line.warn{color:var(--warning)}.log-line.header{color:var(--accent);font-weight:600;margin-top:8px}
.status-bar{padding:10px 36px;background:var(--bg2);border-top:1px solid var(--border);font-size:12px;color:var(--text-dim);display:flex;align-items:center;gap:12px}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--success);transition:background .3s}
.status-dot.busy{background:var(--warning);animation:pulse 1s infinite}
.autostart-row{display:flex;align-items:center;padding:12px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;margin-bottom:8px;animation:slideUp 0.3s}
.autostart-row .info{flex:1;overflow:hidden;margin:0 12px}
.autostart-row .name{font-weight:600;text-overflow:ellipsis;white-space:nowrap;overflow:hidden}
.autostart-row .path{font-size:11px;color:var(--text-dim);text-overflow:ellipsis;white-space:nowrap;overflow:hidden}
.autostart-row .badge{font-size:10px;background:var(--surface2);padding:2px 6px;border-radius:4px;color:var(--text-dim);white-space:nowrap}
.hint{background:rgba(79,140,255,.1);border-left:3px solid var(--accent);padding:12px 16px;border-radius:6px;font-size:13px;color:var(--text-dim);margin:12px 0}
.hint b{color:var(--text)}
.toast-container{position:fixed;bottom:24px;right:24px;z-index:9999;display:flex;flex-direction:column;gap:10px}
.toast{background:var(--surface);border-left:4px solid var(--accent);padding:12px 20px;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.5);animation:slideInRight 0.3s ease-out;display:flex;align-items:center;gap:12px;transition:opacity 0.3s}
.toast.success{border-left-color:var(--success)}
.toast.error{border-left-color:var(--error)}
</style>
</head>
<body>
<div class="app">
<div class="sidebar">
<div class="logo">Optimizer Pro</div>
<div class="nav-item active" data-tab="home"><span>📊</span> Главная</div>
<div class="nav-item" data-tab="optimize"><span>🚀</span> Оптимизация</div>
<div class="nav-item" data-tab="boost"><span>🎮</span> Game Boost</div>
<div class="nav-item" data-tab="debloat"><span>🧹</span> Debloat</div>
<div class="nav-item" data-tab="autostart"><span>⚡</span> Автозагрузка</div>
<div class="nav-item" data-tab="network"><span>🌐</span> Сеть</div>
<div class="nav-item" data-tab="log"><span>📝</span> Лог</div>
<div class="nav-spacer"></div>
<div class="nav-item nav-danger" data-tab="restore"><span>↩</span> Откатить всё</div>
</div>
<div class="main">
<div class="content">
<div class="tab active" id="tab-home">
<h1>Панель управления</h1>
<p class="subtitle">Обзор состояния системы</p>
<div class="cards" id="status-cards"></div>
<div class="section">
<h2>Быстрый старт</h2>
<div class="hint"><b>Полная оптимизация</b> выполнит все безопасные действия: точку восстановления, очистку Temp и кэшей, отключение гибернации, настройку питания и проверку SFC/DISM.</div>
<button onclick="fullOptimize(this)"><span class="btn-spinner"></span> 🚀 Запустить полную оптимизацию</button>
</div>
</div>
<div class="tab" id="tab-optimize">
<h1>Оптимизация</h1>
<p class="subtitle">Выберите нужные действия</p>
<div class="section">
<div class="check-item" onclick="toggle('opt-restore')"><input type="checkbox" id="opt-restore" checked onclick="event.stopPropagation()"><label>Создать точку восстановления Windows</label></div>
<div class="check-item" onclick="toggle('opt-clean')"><input type="checkbox" id="opt-clean" checked onclick="event.stopPropagation()"><label>Очистка Temp</label></div>
<div class="check-item" onclick="toggle('opt-browsers')"><input type="checkbox" id="opt-browsers" checked onclick="event.stopPropagation()"><label>Очистка кэша браузеров и мессенджеров</label></div>
<div class="check-item" onclick="toggle('opt-hibernate')"><input type="checkbox" id="opt-hibernate" checked onclick="event.stopPropagation()"><label>Отключить гибернацию</label></div>
<div class="check-item" onclick="toggle('opt-visualfx')"><input type="checkbox" id="opt-visualfx" checked onclick="event.stopPropagation()"><label>Отключить визуальные эффекты Windows</label></div>
<div class="check-item" onclick="toggle('opt-power')"><input type="checkbox" id="opt-power" checked onclick="event.stopPropagation()"><label>Максимальная производительность (Ultimate)</label></div>
<div class="check-item" onclick="toggle('opt-services')"><input type="checkbox" id="opt-services" checked onclick="event.stopPropagation()"><label>Отключить службы телеметрии</label></div>
<div class="check-item" onclick="toggle('opt-sfc')"><input type="checkbox" id="opt-sfc" onclick="event.stopPropagation()"><label>Проверка целостности (SFC / DISM)</label></div>
</div>
<button onclick="runSelective(this)"><span class="btn-spinner"></span> ▶ Запустить выбранное</button>
</div>
<div class="tab" id="tab-boost">
<h1>Game Boost</h1>
<p class="subtitle">Максимальный FPS и умный приоритет</p>
<div id="boost-idle">
<div class="section">
<h2>Введите имя процесса игры</h2>
<div class="input-row">
<input type="text" id="boost-name" placeholder="cs2.exe, valorant.exe, ...">
<button onclick="startBoost(this)"><span class="btn-spinner"></span> 🎮 Активировать</button>
</div>
<div class="hint">Приоритет игры будет поднят до <b>HIGH</b>. Браузеры и Discord будут опущены до <b>IDLE</b>.</div>
</div>
<div class="section">
<h2>Топ процессов по памяти</h2>
<div class="proc-list" id="proc-list" style="max-height:300px;overflow-y:auto;border:1px solid var(--border);border-radius:8px"></div>
<div style="margin-top:12px"><button class="secondary" onclick="refreshProcs()">🔄 Обновить</button></div>
</div>
</div>
<div id="boost-active" style="display:none"></div>
</div>
<div class="tab" id="tab-debloat">
<h1>Debloat (Выборочно)</h1>
<p class="subtitle">Удаление встроенных приложений</p>
<div class="hint">Приложения из Xbox <b>не удалены</b> по умолчанию, чтобы не сломать Game Pass.</div>
<div class="section" id="debloat-list"></div>
<button class="danger" onclick="runDebloat(this)"><span class="btn-spinner"></span> 🗑 Удалить выбранные</button>
</div>
<div class="tab" id="tab-autostart">
<h1>Автозагрузка</h1>
<p class="subtitle">Полный список (HKCU, HKLM, Папки Startup)</p>
<div id="autostart-list"></div>
</div>
<div class="tab" id="tab-network">
<h1>Сброс сети</h1>
<p class="subtitle">Решение проблем с пингом</p>
<div class="section">
<div class="hint">Сбрасывает кэш DNS, Winsock и IP-стек. <b>Перезагрузите ПК после сброса.</b></div>
<button onclick="resetNetwork(this)"><span class="btn-spinner"></span> 🌐 Выполнить сброс сети</button>
</div>
</div>
<div class="tab" id="tab-log" style="flex-direction:column;height:calc(100vh - 130px)">
<h1>Лог</h1>
<div class="log" id="log-container" style="flex:1"></div>
<div style="margin-top:16px;display:flex;gap:10px">
<button class="secondary" onclick="document.getElementById('log-container').innerHTML=''">Очистить</button>
<button class="secondary" onclick="openLog()">Открыть файл</button>
</div>
</div>
<div class="tab" id="tab-restore">
<h1>Откатить изменения</h1>
<p class="subtitle">Возврат к состоянию до оптимизации</p>
<div class="hint" id="restore-info">Проверка...</div>
<button class="danger" onclick="doRestore(this)"><span class="btn-spinner"></span> ↩ Восстановить</button>
</div>
</div>
<div class="status-bar">
<div class="status-dot" id="status-dot"></div>
<span id="status-text">Готово</span>
</div>
</div>
</div>
<div class="toast-container" id="toasts"></div>
<script>
const py = () => window.pywebview.api;
const DEBLOAT_APPS = ["BingWeather","GetHelp","Getstarted","MicrosoftOfficeHub","SkypeApp","XboxApp","ZuneMusic","MicrosoftSolitaireCollection","Clipchamp","Teams"];

function esc(s) {
    const d = document.createElement('div');
    d.textContent = String(s);
    return d.innerHTML;
}

document.querySelectorAll('.nav-item').forEach(el => {
    el.addEventListener('click', () => {
        document.querySelectorAll('.nav-item').forEach(e => e.classList.remove('active'));
        el.classList.add('active');
        switchTab(el.dataset.tab);
    });
});

function switchTab(name) {
    document.querySelectorAll('.tab').forEach(t => { t.classList.remove('active'); t.style.display = ''; });
    const t = document.getElementById('tab-' + name);
    if (t) {
        t.classList.add('active');
        t.style.display = name === 'log' ? 'flex' : 'block';
    }
    if (name === 'autostart') loadAutostart();
    if (name === 'boost') refreshProcs();
    if (name === 'restore') loadRestoreInfo();
    if (name === 'home') loadStatus();
}

function toggle(id) { const c = document.getElementById(id); if (c) c.checked = !c.checked; }

function toast(msg, type='') {
    const container = document.getElementById('toasts');
    const t = document.createElement('div');
    t.className = 'toast ' + type;
    t.textContent = msg;
    container.appendChild(t);
    setTimeout(() => { t.style.opacity = '0'; setTimeout(() => t.remove(), 300); }, 3000);
}

window.__log = (text) => {
    const c = document.getElementById('log-container');
    if (!c) return;
    const line = document.createElement('div');
    line.className = 'log-line';
    if (text.includes('[ОШИБКА]')) line.classList.add('err');
    else if (text.includes('[OK]')) line.classList.add('ok');
    else if (text.includes('[ПРЕДУПРЕЖДЕНИЕ]') || text.includes('[!]')) line.classList.add('warn');
    else if (text.startsWith('  [+]') || text.includes('===')) line.classList.add('header');
    line.textContent = text;
    c.appendChild(line);
    c.scrollTop = c.scrollHeight;
};

function setBusy(btn, busy, text) {
    document.getElementById('status-dot').className = 'status-dot ' + (busy ? 'busy' : '');
    document.getElementById('status-text').textContent = text || (busy ? 'Выполняется...' : 'Готово');
    if (btn) {
        if (busy) btn.classList.add('loading');
        else btn.classList.remove('loading');
        btn.disabled = busy;
    }
}

window.__done = (result) => {
    document.querySelectorAll('button').forEach(b => { b.disabled = false; b.classList.remove('loading'); });
    document.getElementById('status-dot').className = 'status-dot';
    document.getElementById('status-text').textContent = 'Готово';
    if (result && result.ok === false) toast(result.error || 'Ошибка', 'error');
    else toast('Операция завершена', 'success');
    if (document.getElementById('tab-home').classList.contains('active')) loadStatus();
    if (document.getElementById('tab-boost').classList.contains('active')) refreshProcs();
    if (document.getElementById('tab-restore').classList.contains('active')) loadRestoreInfo();
};

async function callAsync(btn, statusText, method, ...args) {
    setBusy(btn, true, statusText);
    try {
        const r = await py()[method](...args);
        if (r && r.started === false) {
            setBusy(btn, false);
            toast(r.error || 'Уже выполняется операция', 'error');
        }
    } catch (e) {
        setBusy(btn, false);
        toast('Сбой API: ' + e, 'error');
    }
}

async function loadStatus() {
    try {
        const s = await py().get_status();
        document.getElementById('status-cards').innerHTML = `
            <div class="card"><div class="card-label">ОЗУ свободно</div><div class="card-value good">${Number(s.ram_free) || 0} МБ</div></div>
            <div class="card"><div class="card-label">Права админа</div><div class="card-value ${s.admin ? 'good' : 'bad'}">${s.admin ? 'ДА' : 'НЕТ'}</div></div>
            <div class="card"><div class="card-label">Бэкап</div><div class="card-value ${s.has_backup ? 'good' : 'warn'}">${s.has_backup ? 'Есть' : 'Нет'}</div></div>
            <div class="card"><div class="card-label">Game Boost</div><div class="card-value ${s.has_boost ? 'warn' : 'good'}">${s.has_boost ? 'Активен' : 'Свободен'}</div></div>`;
    } catch(e) { toast('Ошибка обновления статуса', 'error'); }
}

async function fullOptimize(btn) {
    if (!confirm('Полная оптимизация включает SFC и DISM — это может занять 1–2 часа. Продолжить?')) return;
    window.__log('=== ПОЛНАЯ ОПТИМИЗАЦИЯ ===');
    await callAsync(btn, 'Полная оптимизация...', 'run_full_optimization');
}

async function runSelective(btn) {
    const flags = {
        restore: document.getElementById('opt-restore').checked,
        clean: document.getElementById('opt-clean').checked,
        browsers: document.getElementById('opt-browsers').checked,
        hibernate: document.getElementById('opt-hibernate').checked,
        visual_fx: document.getElementById('opt-visualfx').checked,
        power: document.getElementById('opt-power').checked,
        services: document.getElementById('opt-services').checked,
        sfc: document.getElementById('opt-sfc').checked,
    };
    await callAsync(btn, 'Выполнение...', 'run_selective', flags);
}

async function refreshProcs() {
    try {
        const data = await py().get_processes();
        const list = document.getElementById('proc-list');
        list.innerHTML = '<div style="padding:8px 12px;color:var(--text-dim);font-size:11px;background:var(--bg2);border-bottom:1px solid var(--border)">Процесс — ОЗУ</div>';
        for (const p of data.procs) {
            const row = document.createElement('div');
            row.style = "padding:8px 12px;border-bottom:1px solid var(--border);cursor:pointer;display:flex;justify-content:space-between;";
            row.onmouseover = () => row.style.background = 'var(--surface2)';
            row.onmouseout = () => row.style.background = 'transparent';
            row.innerHTML = `<span>${esc(p.name)} <span style="color:var(--text-dim);font-size:11px">(PID ${Number(p.pid)||0})</span></span><span style="color:var(--text-dim);font-size:12px">${Number(p.mem_mb)||0} МБ</span>`;
            row.onclick = () => { document.getElementById('boost-name').value = p.name; };
            list.appendChild(row);
        }
        const info = await py().get_boost_info();
        document.getElementById('boost-idle').style.display = info.active ? 'none' : 'block';
        const activeDiv = document.getElementById('boost-active');
        activeDiv.style.display = info.active ? 'block' : 'none';
        if (info.active) {
            activeDiv.innerHTML = `
                <div class="section">
                    <h2>Game Boost активен</h2>
                    <div class="hint">Игра: <b>${esc(info.game_name)}</b></div>
                    <button class="danger" onclick="restorePriors(this)"><span class="btn-spinner"></span> ⏹ Остановить</button>
                </div>`;
        }
    } catch(e) { toast('Ошибка загрузки процессов', 'error'); }
}

async function startBoost(btn) {
    const name = document.getElementById('boost-name').value.trim();
    if (!name) { toast('Введите имя', 'error'); return; }
    await callAsync(btn, 'Game Boost...', 'start_game_boost', name);
}

async function restorePriors(btn) {
    await callAsync(btn, 'Восстановление...', 'restore_priorities');
}

function initDebloat() {
    const container = document.getElementById('debloat-list');
    container.innerHTML = '';
    DEBLOAT_APPS.forEach(app => {
        const isXbox = app.includes('Xbox');
        const div = document.createElement('div');
        div.className = 'check-item';
        div.onclick = () => { const c = document.getElementById('deb-'+app); if (c) c.checked = !c.checked; };
        div.innerHTML = `<input type="checkbox" id="deb-${esc(app)}" ${isXbox ? '' : 'checked'} onclick="event.stopPropagation()"><label>${esc(app)} ${isXbox ? '<span style="color:var(--warning);font-size:11px">(Game Pass)</span>' : ''}</label>`;
        container.appendChild(div);
    });
}

async function runDebloat(btn) {
    const selected = DEBLOAT_APPS.filter(a => document.getElementById('deb-'+a)?.checked);
    if (!selected.length) { toast('Ничего не выбрано', 'error'); return; }
    if (!confirm(`Удалить ${selected.length} приложений?`)) return;
    await callAsync(btn, 'Debloat...', 'run_debloat', selected);
}

async function loadAutostart() {
    const list = document.getElementById('autostart-list');
    list.innerHTML = '<div class="hint">Загрузка...</div>';
    try {
        const data = await py().get_autostart();
        if (!data.items.length) { list.innerHTML = '<div class="hint">Пусто.</div>'; return; }
        list.innerHTML = '';
        for (const item of data.items) {
            const row = document.createElement('div');
            row.className = 'autostart-row';
            const badge = document.createElement('span');
            badge.className = 'badge'; badge.textContent = item.source;
            const info = document.createElement('div');
            info.className = 'info';
            const nameEl = document.createElement('div');
            nameEl.className = 'name'; nameEl.textContent = item.name;
            const pathEl = document.createElement('div');
            pathEl.className = 'path'; pathEl.textContent = item.path;
            info.appendChild(nameEl); info.appendChild(pathEl);
            const btn = document.createElement('button');
            btn.className = 'danger';
            btn.innerHTML = '<span class="btn-spinner"></span> Удалить';
            btn.onclick = () => delAutostart(btn, item);
            row.appendChild(badge); row.appendChild(info); row.appendChild(btn);
            list.appendChild(row);
        }
    } catch(e) { list.innerHTML = '<div class="hint" style="color:var(--error)">Ошибка загрузки</div>'; }
}

async function delAutostart(btn, item) {
    if (!confirm('Удалить: ' + item.name + '?')) return;
    setBusy(btn, true);
    try {
        const r = await py().delete_autostart(item);
        if (r.ok) { await loadAutostart(); toast('Удалено', 'success'); }
        else toast('Не удалось удалить', 'error');
    } catch(e) { toast('Сбой API', 'error'); }
    finally { setBusy(btn, false); }
}

async function resetNetwork(btn) {
    if (!confirm('Сброс сети требует перезагрузки ПК. Продолжить?')) return;
    await callAsync(btn, 'Сброс...', 'run_network_reset');
}

async function openLog() {
    try {
        const r = await py().open_log_file();
        if (!r.ok) toast(r.error || 'Не удалось открыть лог', 'error');
    } catch(e) { toast('Сбой API', 'error'); }
}

async function loadRestoreInfo() {
    try {
        const s = await py().get_status();
        const info = document.getElementById('restore-info');
        if (s.has_backup) info.innerHTML = '<b>Бэкап найден.</b> Можно откатить настройки.';
        else info.innerHTML = '<b>Бэкап не найден.</b>';
    } catch(e) { document.getElementById('restore-info').textContent = 'Ошибка проверки'; }
}

async function doRestore(btn) {
    if (!confirm('Восстановить настройки?')) return;
    window.__log('=== ОТКАТ ===');
    await callAsync(btn, 'Восстановление...', 'run_restore_backup');
}

window.addEventListener('pywebviewready', () => { loadStatus(); initDebloat(); window.__log('Optimizer Pro готов.'); });
</script>
</body>
</html>"""

# ============================================================
# API
# ============================================================
class _UiTee:
    def __init__(self, cb):
        self.cb = cb; self.buf = ""
    def write(self, s):
        if not s: return 0
        self.buf += s
        while '\n' in self.buf:
            line, self.buf = self.buf.split('\n', 1)
            line = line.replace('\r', '').rstrip()
            if line: self.cb(line)
        return len(s)
    def flush(self):
        if self.buf:
            line = self.buf.replace('\r', '').rstrip()
            if line: self.cb(line)
            self.buf = ""

class Api:
    def __init__(self):
        self._window = None
        self._busy = False
        self._lock = threading.Lock()

    def set_window(self, w): self._window = w

    def _emit(self, text):
        if not self._window: return
        try: self._window.evaluate_js(f"window.__log({json.dumps(text, ensure_ascii=False)})")
        except Exception: pass

    def _emit_done(self, result):
        if not self._window: return
        try: self._window.evaluate_js(f"window.__done({json.dumps(result, ensure_ascii=False)})")
        except Exception: pass

    def _run_async(self, fn, *args):
        with self._lock:
            if self._busy:
                return {"started": False, "error": "Уже выполняется операция"}
            self._busy = True

        def worker():
            old_stdout = sys.stdout
            sys.stdout = _UiTee(self._emit)
            try:
                res = fn(*args)
                if isinstance(res, dict):
                    self._emit_done(res)
                elif res is True:
                    self._emit_done({"ok": True})
                else:
                    self._emit_done({"ok": False, "error": "Операция завершилась с ошибкой"})
            except Exception as e:
                self._emit(f"[ОШИБКА] {e}")
                self._emit_done({"ok": False, "error": str(e)})
            finally:
                sys.stdout = old_stdout
                with self._lock:
                    self._busy = False

        threading.Thread(target=worker, daemon=True).start()
        return {"started": True}

    def get_status(self):
        return {
            "ram_free": get_free_ram(),
            "admin": is_admin(),
            "has_backup": _is_valid_backup(_read_backup()),
            "has_boost": _is_boost_valid(),
        }

    def get_processes(self):
        if not PSUTIL_AVAILABLE: return {"procs": []}
        procs = []
        for p in psutil.process_iter(['pid', 'name', 'memory_info']):
            try:
                mi = p.info.get('memory_info'); nm = p.info.get('name')
                if mi and nm:
                    procs.append({"name": nm, "pid": p.info['pid'], "mem_mb": mi.rss // 1024 // 1024})
            except Exception: continue
        procs.sort(key=lambda x: -x['mem_mb'])
        return {"procs": procs[:30]}

    def get_boost_info(self):
        if not _is_boost_valid(): return {"active": False}
        try:
            with open(BOOST_STATE_FILE, 'r', encoding='utf-8') as f:
                st = json.load(f)
            return {"active": True, "game_name": st.get("game_name", "?")}
        except Exception:
            return {"active": False}

    def get_autostart(self):
        return {"items": get_full_autostart()}

    def delete_autostart(self, item):
        return {"ok": delete_autostart_item(item)}

    def open_log_file(self):
        try:
            if not os.path.exists(LOG_FILE):
                with open(LOG_FILE, 'a', encoding='utf-8'):
                    pass
                return {"ok": False, "error": "Лог пуст"}
            if os.path.getsize(LOG_FILE) == 0:
                return {"ok": False, "error": "Лог пуст"}
            os.startfile(LOG_FILE)
            return {"ok": True}
        except Exception as e:
            return {"ok": False, "error": str(e)}

    def run_full_optimization(self):
        flags = {"restore": True, "clean": True, "browsers": True,
                 "hibernate": True, "visual_fx": True, "power": True,
                 "services": True, "sfc": True}
        return self._run_async(run_opt_backend, flags)

    def run_selective(self, flags):
        if not isinstance(flags, dict):
            return {"started": False, "error": "Неверный формат параметров"}
        return self._run_async(run_opt_backend, flags)

    def run_debloat(self, apps):
        if not isinstance(apps, list):
            return {"started": False, "error": "Неверный формат параметров"}
        return self._run_async(debloat_selected, apps)

    def run_restore_backup(self): return self._run_async(restore_backup)
    def start_game_boost(self, name): return self._run_async(game_boost, name)
    def restore_priorities(self): return self._run_async(restore_priorities)
    def run_network_reset(self): return self._run_async(reset_network)

# ============================================================
# БЭКЕНД ОПТИМИЗАЦИИ
# ============================================================
def run_opt_backend(f):
    f = dict(f)  # не мутируем вызывающий объект
    errors = []

    needs_backup = any(f.get(k) for k in ["services", "power", "hibernate", "visual_fx"])
    if needs_backup:
        if not save_backup():
            print("[!] Не удалось создать бэкап — обратимые операции пропущены")
            errors.append("бэкап")
            for k in ("services", "power", "hibernate", "visual_fx"):
                f[k] = False

    if f.get("restore"):
        if PS_EXE and os.path.exists(PS_EXE):
            print("  [+] Создание точки восстановления Windows...")
            ps = ("Enable-ComputerRestore -Drive 'C:\\'; "
                  "Checkpoint-Computer -Description 'Optimizer' "
                  "-RestorePointType 'MODIFY_SETTINGS' -EA Stop")
            b64 = base64.b64encode(ps.encode("utf-16le")).decode()
            if not run_cmd(f'"{PS_EXE}" -EP Bypass -NoP -NonInteractive -Enc {b64}', timeout=600):
                errors.append("точка восстановления")
        else:
            print("  [!] PowerShell недоступен — точка восстановления не создана")
            errors.append("PowerShell")

    if f.get("clean") or f.get("browsers"):
        if not clean_temp_and_cache(clean_browsers=f.get("browsers"), clean_temp=f.get("clean")):
            errors.append("очистка кэша")

    if f.get("hibernate"):
        if not run_cmd("powercfg -h off", "Гибернация"):
            errors.append("гибернация")

    if f.get("visual_fx"):
        if not run_cmd(
            'reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" '
            '/v "VisualFXSetting" /t REG_DWORD /d 2 /f', "Эффекты"):
            errors.append("эффекты")

    if f.get("power"):
        if not enable_ultimate():
            errors.append("питание")

    if f.get("services"):
        for s in ("DiagTrack", "SysMain", "WSearch"):
            if not run_cmd(f"sc config {s} start= disabled", None, False):
                errors.append(f"{s} config")
            if not run_cmd(f"sc stop {s}", None, False, allowed=[0, 1062]):
                errors.append(f"{s} stop")

    if f.get("sfc"):
        dism = os.path.join(NATIVE_SYS32, "dism.exe")
        sfc = os.path.join(NATIVE_SYS32, "sfc.exe")
        if not run_cmd(f'"{dism}" /online /cleanup-image /restorehealth', "DISM", timeout=3600):
            errors.append("DISM")
        if not run_cmd(f'"{sfc}" /scannow', "SFC", timeout=3600):
            errors.append("SFC")

    if errors:
        return {"ok": False, "error": "Не выполнено: " + ", ".join(errors[:4])}
    return {"ok": True}

# ============================================================
# КОНСОЛЬНЫЙ РЕЖИМ
# ============================================================
def main_console():
    os.system("cls")
    print("="*50)
    print("  OPTIMIZER PRO - КОНСОЛЬНЫЙ РЕЖИМ")
    print("="*50)
    if not PSUTIL_AVAILABLE:
        print("  [!] Для Game Boost установите: pip install psutil")

    try:
        while True:
            print("\n  1. Полная оптимизация")
            print("  2. Очистка Temp и кэша")
            print("  3. Сброс сети")
            print("  4. Активировать Game Boost")
            print("  5. Остановить Game Boost (восстановить приоритеты)")
            print("  6. Откат системных изменений")
            print("  7. Выход")

            choice = safe_input("\n  Выбор: ").strip()
            if choice == '1':
                print("  Запуск...")
                r = run_opt_backend({"restore": True, "clean": True, "browsers": True,
                                      "hibernate": True, "visual_fx": True, "power": True,
                                      "services": True, "sfc": True})
                print(f"  {'Готово.' if _res_ok(r) else 'Ошибка: ' + _res_err(r)}")
            elif choice == '2':
                r = clean_temp_and_cache(True, True)
                print(f"  {'Готово.' if r else 'Завершено с ошибками.'}")
            elif choice == '3':
                r = reset_network()
                print(f"  {'Готово (перезагрузите ПК).' if _res_ok(r) else 'Ошибка: ' + _res_err(r)}")
            elif choice == '4':
                name = safe_input("  Имя процесса игры: ").strip()
                if name:
                    r = game_boost(name)
                    print(f"  {'Game Boost активирован.' if _res_ok(r) else 'Ошибка: ' + _res_err(r)}")
            elif choice == '5':
                r = restore_priorities()
                print(f"  {'Приоритеты восстановлены.' if _res_ok(r) else 'Ошибка: ' + _res_err(r)}")
            elif choice == '6':
                r = restore_backup()
                print(f"  {'Готово.' if _res_ok(r) else 'Ошибка: ' + _res_err(r)}")
            elif choice == '7':
                break
            else:
                print("  Неверный ввод.")
            safe_input("\n  Enter для продолжения...")
            os.system("cls")
    except KeyboardInterrupt:
        print("\n  Прервано пользователем.")

# ============================================================
# ЗАПУСК
# ============================================================
if __name__ == "__main__":
    if not is_admin() and "--elevated" not in sys.argv:
        if run_as_admin():
            sys.exit(0)
        # UAC отклонён или запуск не удался
        try:
            ctypes.windll.user32.MessageBoxW(
                0,
                "Требуются права администратора.\n\n"
                "Запустите программу от имени администратора.",
                "Optimizer Pro", 0x10)
        except Exception:
            print("Требуются права администратора.")
        sys.exit(1)

    if '--console' in sys.argv:
        main_console()
    elif HAS_WEBVIEW:
        try:
            api = Api()
            window = webview.create_window(
                'Optimizer Pro', html=HTML_UI, js_api=api,
                width=1100, height=750, background_color='#0e1116'
            )
            api.set_window(window)
            webview.start()
        except Exception as e:
            print(f"Ошибка GUI: {e}")
            try:
                ctypes.windll.user32.MessageBoxW(
                    0, f"Не удалось запустить GUI:\n{e}", "Optimizer Pro", 0x10)
            except Exception:
                pass
    else:
        try:
            ctypes.windll.user32.MessageBoxW(
                0,
                "Установите pywebview для GUI:\n\npip install pywebview\n\n"
                "Или запустите с --console из cmd.exe.",
                "Optimizer Pro", 0x10)
        except Exception:
            pass