#!/usr/bin/env python3
# Optimizer Pro - Final Release Version (Bulletproof)
import sys, os

if sys.platform != "win32":
    print("Эта программа предназначена только для Windows.")
    sys.exit(1)

import subprocess, ctypes, time, shutil, threading, signal, stat, base64, re, json, msvcrt, winreg, textwrap, traceback
from ctypes import wintypes

# ============================================================
# ГЛОБАЛЬНЫЕ ФЛАГИ И КОНСТАНТЫ
# ============================================================
try:
    IS_TTY = sys.stdout is not None and sys.stdout.isatty()
except Exception:
    IS_TTY = False

# Если программа запущена без консоли (например, через pythonw), принудительно её создаем
if not IS_TTY:
    try:
        ctypes.windll.kernel32.AllocConsole()
        sys.stdout = open("CONOUT$", "w", encoding="utf-8", buffering=1)
        sys.stdin = open("CONIN$", "r", encoding="utf-8")
        sys.stderr = open("CONOUT$", "w", encoding="utf-8", buffering=1)
        IS_TTY = True
    except Exception:
        # Без консоли интерактивная программа работать не может (иначе бесконечный цикл меню)
        sys.exit(1)

IS_OPTIMIZING = False
IS_RESTORING = False
EMERGENCY_DONE = False
_IN_HANDLER = False
VT_ENABLED = False

_current_proc = None
_proc_lock = threading.RLock()  # RLock: обработчик Ctrl+C может сработать, пока блокировка уже захвачена

SYSTEM_ROOT = os.environ.get('SystemRoot', r'C:\Windows')
BACKUP_DIR = os.path.join(SYSTEM_ROOT, 'Temp', 'OptimizerPro')
BACKUP_FILE = os.path.join(BACKUP_DIR, "backup.json")
LOG_FILE = os.path.join(BACKUP_DIR, "optimization_log.txt")
SYSTEM_DRIVE = os.environ.get('SystemDrive', 'C:') + "\\"
CREATE_NO_WINDOW = 0x08000000

ULTIMATE_GUID = "e9a42b02-d5df-448d-aa00-03f14749eb61"
HIGH_PERF_GUID = "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"

# 32-битный Python на 64-битной Windows видит SysWOW64 вместо System32 — для sfc/dism/PowerShell нужен Sysnative
IS_WOW64 = sys.maxsize <= 2**32 and bool(os.environ.get("PROCESSOR_ARCHITEW6432"))
NATIVE_SYS32 = os.path.join(SYSTEM_ROOT, "Sysnative" if IS_WOW64 else "System32")

PS_EXE = os.path.join(NATIVE_SYS32, 'WindowsPowerShell', 'v1.0', 'powershell.exe')
if not os.path.exists(PS_EXE):
    PS_EXE = shutil.which("powershell.exe") or PS_EXE

SPIN_CHARS = "|/-\\"

ORIG_USER_TEMP = None
if "--orig-temp" in sys.argv:
    idx = sys.argv.index("--orig-temp")
    if idx + 1 < len(sys.argv):
        ORIG_USER_TEMP = sys.argv[idx + 1]
        del sys.argv[idx:idx + 2]
    else:
        del sys.argv[idx]

# ============================================================
# НАСТРОЙКА КОНСОЛИ
# ============================================================
if IS_TTY:
    try:
        ctypes.windll.kernel32.SetConsoleOutputCP(65001)
        ctypes.windll.kernel32.SetConsoleCP(65001)
        k = ctypes.windll.kernel32
        h = k.GetStdHandle(-11)
        m = wintypes.DWORD()
        if k.GetConsoleMode(h, ctypes.byref(m)):
            if k.SetConsoleMode(h, m.value | 0x0004):
                VT_ENABLED = True
        if hasattr(sys.stdout, 'reconfigure'):
            sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

try: 
    cp = ctypes.windll.kernel32.GetOEMCP()
    OEM_ENCODING = f"cp{cp}" if cp else "cp866"
except Exception: 
    OEM_ENCODING = "cp866"

try:
    os.makedirs(BACKUP_DIR, exist_ok=True)
except Exception:
    pass

# ============================================================
# УТИЛИТЫ ВВОДА/ВЫВОДА
# ============================================================
def write_log(text):
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"{text}\n")
    except Exception:
        pass

def safe_input(prompt="", default=""):
    try:
        return input(prompt)
    except (EOFError, KeyboardInterrupt, RuntimeError, OSError):
        return default

def pause(msg="\n  Нажмите Enter..."):
    safe_input(msg)

def clear_screen():
    if not IS_TTY:
        return
    if VT_ENABLED:
        print("\033[H\033[J", end="", flush=True)
    else:
        os.system("cls")

def set_window_size(cols=64, lines=24):
    if not IS_TTY:
        return
    try:
        os.system(f'mode con: cols={cols} lines={lines}')
    except Exception:
        pass

def _native_tool(name):
    p = os.path.join(NATIVE_SYS32, name)
    return f'"{p}"' if os.path.exists(p) else name

def _norm(p):
    return os.path.normcase(os.path.abspath(p)).rstrip('\\')

# ============================================================
# ПРАВА АДМИНИСТРАТОРА
# ============================================================
def is_admin(): 
    try: return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception: return False

def run_as_admin():
    try:
        s = ctypes.windll.shell32
        s.ShellExecuteW.restype = wintypes.HINSTANCE
        s.ShellExecuteW.argtypes = [wintypes.HWND, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_int]
        # Используем abspath для надежности
        args = ([os.path.abspath(sys.argv[0])] if not getattr(sys, 'frozen', False) else []) + sys.argv[1:]
        if os.environ.get("TEMP"):
            args += ["--orig-temp", os.environ["TEMP"]]
        res = s.ShellExecuteW(None, "runas", sys.executable, subprocess.list2cmdline(args), None, 1)
        # HINSTANCE = c_void_p, при значении 0 ctypes возвращает None
        return (res or 0) > 32
    except Exception:
        return False

# ============================================================
# УПРАВЛЕНИЕ ПРОЦЕССАМИ
# ============================================================
def _set_proc(p):
    global _current_proc
    with _proc_lock:
        _current_proc = p

def _kill_proc():
    global _current_proc
    with _proc_lock:
        p, _current_proc = _current_proc, None
    if p and p.poll() is None:
        try:
            subprocess.run(f"taskkill /f /t /pid {p.pid}", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=CREATE_NO_WINDOW, timeout=5)
        except Exception:
            try: p.kill()
            except Exception: pass

def _run_impl(cmd, timeout, enc, capture=True):
    if not cmd or not str(cmd).strip():
        return -1, "", "Empty command", False
    timeout = max(1, timeout)
    try:
        p = subprocess.Popen(
            cmd, shell=True, 
            stdout=subprocess.PIPE if capture else subprocess.DEVNULL, 
            stderr=subprocess.PIPE, 
            text=True, encoding=enc, errors='replace', 
            creationflags=CREATE_NO_WINDOW, stdin=subprocess.DEVNULL
        )
    except Exception as e:
        return -1, "", str(e), False
        
    _set_proc(p)
    try:
        dl = time.time() + timeout
        while True:
            rem = dl - time.time()
            if rem <= 0:
                _kill_proc()
                try: out, err = p.communicate(timeout=5)
                except Exception: out, err = "", ""
                return (p.returncode if p.returncode is not None else -1), out or "", err or "", True
            try:
                out, err = p.communicate(timeout=min(0.5, rem))
                return p.returncode, out or "", err or "", False
            except subprocess.TimeoutExpired:
                continue
            except Exception as e:
                try: p.kill()
                except Exception: pass
                return -1, "", f"Ошибка communicate: {e}", False
    finally:
        _set_proc(None)

# ============================================================
# ОБРАБОТКА СИГНАЛОВ (Ctrl+C)
# ============================================================
def _sig_handler(sig, frame):
    global IS_RESTORING, EMERGENCY_DONE, _IN_HANDLER
    # Проверяем откат ПЕРВЫМ, чтобы повторный Ctrl+C не убил процесс посреди восстановления
    if IS_RESTORING:
        print("\n  [!] Откат уже выполняется, подождите...")
        return
    if _IN_HANDLER: 
        os._exit(1) 
    _IN_HANDLER = True
    
    if VT_ENABLED: print("\033[?25h", end="", flush=True)
        
    try:
        print("\n\n  [!] ПРЕРВАНО ПОЛЬЗОВАТЕЛЕМ.")
        _kill_proc()
        if IS_OPTIMIZING and not EMERGENCY_DONE and os.path.exists(BACKUP_FILE):
            print("  [!] Аварийный откат...")
            IS_RESTORING = True
            try: 
                restore_backup(emergency=True)
                EMERGENCY_DONE = True
            except Exception as e: 
                print(f"  [ОШИБКА] Откат не удался: {e}")
            finally: 
                IS_RESTORING = False
    except Exception:
        pass
    try:
        sys.stdout.flush()
        sys.stderr.flush()
    except Exception:
        pass
    os._exit(1)

signal.signal(signal.SIGINT, _sig_handler)

# ============================================================
# СИСТЕМНЫЕ ФУНКЦИИ
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

def run_cmd(cmd, text=None, check=True, timeout=300, allowed=None):
    allowed = allowed or [0]
    if text:
        print(f"  [+] {text}...")
        write_log(f"  {text}")
    try:
        rc, out, err, tm = _run_impl(cmd, timeout, OEM_ENCODING)
        if tm:
            print("      +-- [ОШИБКА] Таймаут")
            write_log("      [ОШИБКА] Таймаут")
            return False
        if rc in allowed:
            if text:
                print("      +-- [OK]")
                write_log("      +-- [OK]")
            return True
        detail = str((err or "").strip() or (out or "").strip() or f"Код {rc}")
        write_log(f"      [ОШИБКА] {cmd}: {detail[:500]}")
        prefix = "ОШИБКА" if check else "ПРЕДУПРЕЖДЕНИЕ"
        print(f"      +-- [{prefix}] {detail[:80]}")
        return False
    except Exception as e:
        print(f"      +-- [ОШИБКА] {e}")
        return False

def run_cmd_spinner(cmd, text, timeout=7200):
    print(f"  [+] {text}...")
    write_log(f"  {text}")
    if VT_ENABLED:
        print("\033[?25l", end="", flush=True)
    ev = threading.Event()
    def spin():
        i = 0
        while not ev.is_set():
            if IS_TTY:
                print(f"\r      {SPIN_CHARS[i % 4]} Выполняется... ", end="", flush=True)
            time.sleep(0.1)
            i += 1
    t = threading.Thread(target=spin, daemon=True)
    t.start()
    
    ok, err = False, ""
    try:
        res = _run_impl(cmd, timeout, OEM_ENCODING, False)
        if not res or len(res) != 4:
            err = "Ошибка выполнения"
        else:
            rc, _, e, tm = res
            if tm: 
                err = "Таймаут"
            elif rc == 0: 
                ok = True
            else: 
                err = str((e or "").strip() or f"Код {rc}")
    except Exception as ex:
        err = str(ex)
    finally:
        ev.set()
        t.join(timeout=1)
        if IS_TTY:
            print("\r" + " " * 60 + "\r", end="", flush=True)
        if VT_ENABLED:
            print("\033[?25h", end="", flush=True)
            
    if ok:
        print("      +-- [OK]")
        write_log("      +-- [OK]")
    else:
        print(f"      +-- [ОШИБКА] {err[:80]}")
        write_log(f"      [ОШИБКА] {err}")
    return ok

def remove_readonly(func, path, exc_info):
    if not os.path.exists(path): return
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception:
        pass

# ============================================================
# БЛОКИРОВКА И БЭКАП
# ============================================================
class BackupLock:
    # Реентерабельная блокировка: аварийный откат из обработчика Ctrl+C
    # может выполняться, пока основной код уже держит блокировку
    _depth = 0
    _file = None
    _guard = threading.RLock()

    def __init__(self, timeout=15):
        self.p = BACKUP_FILE + ".lock"
        self.t = timeout

    def __enter__(self):
        with BackupLock._guard:
            if BackupLock._depth > 0:
                BackupLock._depth += 1
                return self
        end = time.time() + self.t
        while time.time() < end:
            f = None
            try:
                os.makedirs(BACKUP_DIR, exist_ok=True)
                f = open(self.p, "a+b")
                f.seek(0)
                msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
                with BackupLock._guard:
                    BackupLock._file = f
                    BackupLock._depth = 1
                return self
            except OSError:
                if f:
                    try: f.close()
                    except Exception: pass
                time.sleep(0.1)
        raise TimeoutError("Не удалось заблокировать файл бэкапа")

    def __exit__(self, *a):
        with BackupLock._guard:
            BackupLock._depth -= 1
            if BackupLock._depth > 0:
                return False
            f, BackupLock._file = BackupLock._file, None
            BackupLock._depth = 0
        if f:
            try:
                f.seek(0)
                msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
            except Exception:
                pass
            try: f.close()
            except Exception: pass
        return False

def _atomic_json(path, data):
    try:
        if os.path.exists(path):
            try: shutil.copy2(path, path + ".bak")
            except Exception: pass
        with open(path + ".tmp", "w", encoding="utf-8") as f: 
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.flush()
            os.fsync(f.fileno())
        os.replace(path + ".tmp", path)
        return True
    except Exception as e:
        write_log(f"Ошибка записи JSON: {e}")
        return False

def _read_backup():
    with open(BACKUP_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data if isinstance(data, dict) else {}

def _get_scheme():
    try:
        out = subprocess.check_output("powercfg /getactivescheme", shell=True, text=True, encoding=OEM_ENCODING, errors='replace', creationflags=CREATE_NO_WINDOW, stdin=subprocess.DEVNULL, timeout=15)
        m = re.search(r'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}', out)
        return m.group(0) if m else None
    except Exception:
        return None

def _get_all_schemes():
    try:
        out = subprocess.check_output("powercfg /list", shell=True, text=True, encoding=OEM_ENCODING, errors='replace', creationflags=CREATE_NO_WINDOW, stdin=subprocess.DEVNULL, timeout=15)
        return set(g.lower() for g in re.findall(r'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}', out))
    except Exception:
        return set()

# Подписи полей ("STATE"/"СОСТОЯНИЕ") локализованы, а имена состояний — всегда на английском
_RE_SVC = re.compile(r':\s*\d+\s+(STOPPED|START_PENDING|STOP_PENDING|RUNNING|CONTINUE_PENDING|PAUSE_PENDING|PAUSED)\b')
def _svc_running(n):
    try:
        out = subprocess.check_output(f"sc query {n}", shell=True, text=True, encoding=OEM_ENCODING, errors='replace', creationflags=CREATE_NO_WINDOW, stdin=subprocess.DEVNULL, timeout=10)
        m = _RE_SVC.search(out)
        return (m.group(1) == "RUNNING") if m else None
    except Exception:
        return None

def _wait_svc(n, state, timeout=30):
    for _ in range(timeout):
        if _svc_running(n) == (state == "RUNNING"):
            return True
        time.sleep(1)
    return False

def _reg_start(svc):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, rf"SYSTEM\CurrentControlSet\Services\{svc}") as k:
            try: st, _ = winreg.QueryValueEx(k, "Start")
            except Exception: st = 3
            try: dl, _ = winreg.QueryValueEx(k, "DelayedAutostart")
            except Exception: dl = 0
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
        pass
    try:
        return os.path.exists(os.path.join(SYSTEM_DRIVE, "hiberfil.sys"))
    except Exception:
        return None

VFX_KEY = r"Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects"

def save_backup():
    print("  [+] Создание резервной копии...")
    try:
        with BackupLock():
            if os.path.exists(BACKUP_FILE):
                try:
                    old = _read_backup()
                    if old.get("version") == 1: 
                        print("      +-- [ПРОПУЩЕНО] Бэкап уже существует (сохранено исходное состояние).")
                        return True
                except Exception:
                    pass
                try: os.remove(BACKUP_FILE)
                except Exception: pass
                
            b = {"version": 1, "created_schemes": [], "removed_appx": [], "services": {}}
            ok, miss = True, []
            
            active = _get_scheme()
            if active: b["active_scheme"] = active
            else: ok = False; miss.append("схема")
            
            for svc in ("DiagTrack", "SysMain", "WSearch", "wuauserv", "bits"):
                r, run = _reg_start(svc), _svc_running(svc)
                if r and run is not None:
                    b["services"][svc] = {"start_type": _st_type(*r), "is_running": run}
                else:
                    miss.append(svc)
                    
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, VFX_KEY) as k:
                    try: b["visual_fx"], b["visual_fx_exists"] = int(winreg.QueryValueEx(k, "VisualFXSetting")[0]), True
                    except FileNotFoundError: b["visual_fx"], b["visual_fx_exists"] = None, False
            except FileNotFoundError:
                b["visual_fx"], b["visual_fx_exists"] = None, False
            except Exception:
                miss.append("эффекты")
                
            hib = _hibernate_enabled()
            if hib is None: miss.append("гибернация")
            else: b["hibernate"] = hib
            
            if not _atomic_json(BACKUP_FILE, b):
                print("      +-- [ОШИБКА] Не удалось записать файл бэкапа")
                return False
            print(f"      +-- [OK] {'(не сохранено: ' + ', '.join(miss) + ')' if miss else ''}")
            return ok
    except Exception as e:
        print(f"      +-- [ОШИБКА] Бэкап: {e}")
        return False

def restore_backup(emergency=False):
    if not emergency: print("  [+] Восстановление...")
    try:
        with BackupLock():
            if not os.path.exists(BACKUP_FILE): return False
            try:
                b = _read_backup()
                if not b: return False
            except Exception:
                return False
            
            ok = True
            if b.get("active_scheme"): 
                if not run_cmd(f'powercfg /setactive {b["active_scheme"]}', "Схема питания", False): ok = False
            else: ok = False
            
            current_active = (_get_scheme() or "").lower()
            for g in b.get("created_schemes", []):
                if isinstance(g, str) and g.lower() != current_active:
                    run_cmd(f'powercfg /delete {g}', None, False)
                
            srvs = b.get("services", {})
            if isinstance(srvs, dict):
                for svc, d in srvs.items():
                    if not isinstance(d, dict): continue
                    if not run_cmd(f"sc config {svc} start= {d.get('start_type', 'auto')}", f"Служба {svc}", False): ok = False
                    target = "start" if d.get("is_running") else "stop"
                    if not run_cmd(f"sc {target} {svc}", None, False, allowed=[0, 1056, 1062]): ok = False
                    _wait_svc(svc, "RUNNING" if d.get("is_running") else "STOPPED", 10)
                    
            if "visual_fx_exists" in b:
                if b.get("visual_fx_exists") and b.get("visual_fx") is not None: 
                    if not run_cmd(f'reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d {b["visual_fx"]} /f', "Визуальные эффекты", False): ok = False
                else:
                    # Параметра изначально не было — удаляем созданный (код 1 = параметра нет)
                    run_cmd('reg delete "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" /v "VisualFXSetting" /f', None, False, allowed=[0, 1])
                    
            if b.get("hibernate") is not None: 
                if not run_cmd(f"powercfg -h {'on' if b['hibernate'] else 'off'}", "Гибернация", False): ok = False
                
            if b.get("removed_appx"):
                print("  [!] ВНИМАНИЕ: Удаленные приложения Appx нельзя вернуть автоматически:")
                print(f"      {', '.join(map(str, b['removed_appx']))}")
                
            if ok: 
                if not emergency: print("      +-- [OK]")
                try: os.remove(BACKUP_FILE)
                except Exception: pass
            elif not emergency: print("      +-- [ОШИБКА] Не все настройки восстановлены.")
            return ok
    except Exception as e:
        print(f"      +-- [ОШИБКА] {e}")
        return False

# ============================================================
# ДЕЙСТВИЯ ОПТИМИЗАЦИИ
# ============================================================
def enable_ultimate():
    print("  [+] Настройка питания...")
    before = _get_all_schemes()
    res = _run_impl(f'powercfg -duplicatescheme {ULTIMATE_GUID}', 15, OEM_ENCODING)
    
    if res and res[0] == 0:
        after = _get_all_schemes()
        new = after - before
        if new:
            g = next(iter(new))
            try:
                with BackupLock():
                    if os.path.exists(BACKUP_FILE):
                        b = _read_backup()
                        if isinstance(b.get("created_schemes"), list) and g not in b["created_schemes"]:
                            b["created_schemes"].append(g)
                            _atomic_json(BACKUP_FILE, b)
            except Exception:
                pass
            
            if run_cmd(f'powercfg /setactive {g}', "Ultimate", False): 
                return True
            
    print("      [!] Ultimate не удался, включаю High Perf...")
    return run_cmd(f'powercfg /setactive {HIGH_PERF_GUID}', "High Perf", False)

def clean_temp():
    print("  [+] Остановка служб обновлений...")
    w, bt = _svc_running("wuauserv"), _svc_running("bits")
    if w: run_cmd("sc stop wuauserv", None, False, allowed=[0, 1062])
    if bt: run_cmd("sc stop bits", None, False, allowed=[0, 1062])
    stopped = (not w or _wait_svc("wuauserv", "STOPPED")) and (not bt or _wait_svc("bits", "STOPPED"))
    
    print("  [+] Очистка Temp...")
    paths, seen = [], set()
    for p in (os.environ.get('TEMP'), ORIG_USER_TEMP, os.path.join(SYSTEM_ROOT, 'Temp')):
        if p and _norm(p) not in seen:
            seen.add(_norm(p))
            paths.append(p)
            
    if stopped:
        sd = os.path.join(SYSTEM_ROOT, 'SoftwareDistribution', 'Download')
        if _norm(sd) not in seen:
            seen.add(_norm(sd))
            paths.append(sd)
    
    forbidden = {
        _norm(SYSTEM_DRIVE),
        _norm(SYSTEM_ROOT),
        _norm(os.path.join(SYSTEM_ROOT, 'System32')),
        _norm(os.environ.get('ProgramFiles', 'C:\\Program Files'))
    }
    # Папка с бэкапом и логом лежит внутри Windows\Temp — её удалять нельзя
    protected = _norm(BACKUP_DIR)
    
    skipped = 0
    for p in paths:
        p_abs = os.path.abspath(p).rstrip('\\')
        if not os.path.isdir(p_abs) or _norm(p_abs) in forbidden or len(p_abs) <= 3:
            continue
        try:
            files = os.listdir(p_abs)
        except (PermissionError, OSError):
            continue
        for f in files:
            try:
                fp = os.path.join(p_abs, f)
                if _norm(fp) == protected:
                    continue
                if os.path.islink(fp):
                    continue
                if os.path.isdir(fp):
                    shutil.rmtree(fp, onerror=remove_readonly)
                else:
                    os.unlink(fp)
            except Exception:
                skipped += 1
                
    print(f"      +-- [OK] (пропущено {skipped})")
    if w: run_cmd("sc start wuauserv", None, False, allowed=[0, 1056])
    if bt: run_cmd("sc start bits", None, False, allowed=[0, 1056])
    return True

def clean_game():
    print("\n  [!] ВНИМАНИЕ: Steam/Epic/Riot будут принудительно закрыты.")
    if safe_input("      Продолжить? (Y/N): ").strip().upper() != "Y":
        return True
        
    # 128 — процесс не найден, это не ошибка
    run_cmd("taskkill /f /im steam.exe /im epicgameslauncher.exe /im riotclient.exe", None, False, allowed=[0, 128])
    
    print("  [+] Очистка кэша игр...")
    err = False
    for p in [os.path.expandvars(r"%LOCALAPPDATA%\Steam\htmlcache"), 
              os.path.expandvars(r"%LOCALAPPDATA%\EpicGamesLauncher\Saved\webcache"), 
              os.path.expandvars(r"%LOCALAPPDATA%\D3DSCache")]:
        if os.path.isdir(p):
            try: files = os.listdir(p)
            except Exception: continue
            for f in files:
                try:
                    fp = os.path.join(p, f)
                    if os.path.islink(fp): continue
                    if os.path.isdir(fp): shutil.rmtree(fp, onerror=remove_readonly)
                    else: os.unlink(fp)
                except Exception:
                    err = True
                    
    print(f"      +-- [OK] {'(некоторые файлы заблокированы)' if err else ''}")
    return not err

def debloat(ask=True):
    if not PS_EXE or not os.path.exists(PS_EXE): 
        print("  [!] ОШИБКА: PowerShell не найден.")
        return False
    if ask and safe_input("  Удалить встроенные приложения? (Y/N): ").strip().upper() != "Y": 
        return True
        
    print("  [+] Debloat...")
    apps = ["BingWeather", "GetHelp", "Getstarted", "MicrosoftOfficeHub", "SkypeApp", "XboxApp", "ZuneMusic"]
    removed, failed = 0, []
    for a in apps:
        # exit 2 — приложение не установлено, exit 1 — удалить не удалось, exit 0 — удалено
        s = (f"$x=Get-AppxPackage *{a}* -AllUsers -EA 0; "
             f"$y=Get-AppxProvisionedPackage -Online -EA 0 | Where {{$_.DisplayName -like '*{a}*'}}; "
             f"if(-not $x -and -not $y){{exit 2}}; "
             f"if($x){{$x | Remove-AppxPackage -AllUsers -EA 0}}; "
             f"if($y){{$y | Remove-AppxProvisionedPackage -Online -EA 0 | Out-Null}}; "
             f"$x=Get-AppxPackage *{a}* -AllUsers -EA 0; "
             f"$y=Get-AppxProvisionedPackage -Online -EA 0 | Where {{$_.DisplayName -like '*{a}*'}}; "
             f"if($x -or $y){{exit 1}}else{{exit 0}}")
        cmd = f'"{PS_EXE}" -EP Bypass -NoP -NonInteractive -Enc {base64.b64encode(s.encode("utf-16le")).decode()}'
        
        try:
            res = _run_impl(cmd, 120, OEM_ENCODING, False)
            if res and res[0] == 0: 
                removed += 1
                write_log(f"      Удалено: {a}")
                try:
                    with BackupLock():
                        if os.path.exists(BACKUP_FILE):
                            b = _read_backup()
                            if isinstance(b.get("removed_appx"), list) and a not in b["removed_appx"]:
                                b["removed_appx"].append(a)
                                _atomic_json(BACKUP_FILE, b)
                except Exception:
                    pass
            elif res and res[0] != 2:
                failed.append(a)
        except Exception:
            failed.append(a)
            
    if failed:
        print(f"      +-- [ПРЕДУПРЕЖДЕНИЕ] Удалено: {removed}, не удалось: {', '.join(failed)}")
        write_log(f"      [ОШИБКА] Debloat не удалил: {', '.join(failed)}")
        return False
    print(f"      +-- [OK] (удалено: {removed})")
    return True

# ============================================================
# ГЛАВНАЯ ЛОГИКА
# ============================================================
def run_opt(f):
    global IS_OPTIMIZING
    clear_screen()
    print("\n  +----------------------------------------------------+\n  |            ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ                  |\n  +----------------------------------------------------+\n")
    
    if not any(v for k, v in f.items() if k != "is_full"): 
        print("  [!] Не выбрано ни одного действия.")
        return pause()
        
    sys_chg = any(f.get(k) for k in ("services", "visual_fx", "power", "hibernate", "debloat"))
    err = False
    no_backup_ok = False
    
    IS_OPTIMIZING = True
    try:
        if sys_chg and not save_backup(): 
            print("  [!] КРИТИЧЕСКАЯ ОШИБКА: бэкап не создан. Откат будет невозможен.")
            if safe_input("  Продолжить? (Y/N): ").strip().upper() != "Y":
                IS_OPTIMIZING = False
                return pause()
            err = True
            no_backup_ok = True
            
        b = {}
        try:
            with BackupLock():
                if os.path.exists(BACKUP_FILE):
                    b = _read_backup()
        except Exception:
            b = {}
        
        def can_change(has_backup_data):
            if not sys_chg or no_backup_ok or has_backup_data:
                return True
            print("      +-- [ПРОПУЩЕНО] Нет данных для отката")
            return False
        
        rb = get_free_ram()
        
        if f.get("restore"): 
            if PS_EXE and os.path.exists(PS_EXE):
                drive = SYSTEM_DRIVE.replace("'", "''")
                ps = f"Enable-ComputerRestore -Drive '{drive}'; Checkpoint-Computer -Description 'Optimizer' -RestorePointType 'MODIFY_SETTINGS' -EA Stop"
                b64 = base64.b64encode(ps.encode("utf-16le")).decode()
                if not run_cmd(f'"{PS_EXE}" -EP Bypass -NoP -NonInteractive -Enc {b64}', "Создание точки восстановления", timeout=600):
                    err = True
            else:
                print("  [!] PowerShell не найден, точка восстановления не создана.")
                err = True
                
        if f.get("clean") and not clean_temp(): err = True
        if f.get("game_cache") and not clean_game(): err = True
        
        if f.get("hibernate"):
            if can_change(b.get("hibernate") is not None):
                if not run_cmd("powercfg -h off", "Отключение гибернации"): err = True
                
        if f.get("visual_fx"):
            if can_change("visual_fx_exists" in b):
                if not run_cmd('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d 2 /f', "Отключение эффектов"): err = True
                
        if f.get("power"):
            if can_change(bool(b.get("active_scheme"))):
                if not enable_ultimate(): err = True
        
        if f.get("services"):
            print("  [+] Отключение служб...")
            srv_b = b.get("services", {}) if isinstance(b.get("services"), dict) else {}
            for s in ("DiagTrack", "SysMain", "WSearch"):
                if not sys_chg or no_backup_ok or s in srv_b:
                    if not run_cmd(f"sc config {s} start= disabled", None, False): err = True
                    if not run_cmd(f"sc stop {s}", None, False, allowed=[0, 1062]): err = True
                else:
                    print(f"      +-- [ПРОПУЩЕНО] {s}: нет данных для отката")
            print("      +-- [OK]")
                    
        if f.get("debloat") and not debloat(not f.get("is_full")): err = True
        
        if f.get("ram"):
            if not run_cmd("rundll32.exe advapi32.dll,ProcessIdleTasks", "Оптимизация ОЗУ", False): err = True
        
        if f.get("sfc"):
            dism, sfc = _native_tool("dism.exe"), _native_tool("sfc.exe")
            if not run_cmd_spinner(f"{dism} /online /cleanup-image /scanhealth", "Проверка DISM"): err = True
            if not run_cmd_spinner(f"{dism} /online /cleanup-image /restorehealth", "Восстановление DISM"): err = True
            if not run_cmd_spinner(f"{sfc} /scannow", "Проверка SFC"): err = True
            
        ra = get_free_ram()
        diff = f"(+{ra-rb} МБ)" if ra > rb else ""
        print(f"\n  +----------------------------------------------------+\n  | {'ЗАВЕРШЕНО С ОШИБКАМИ' if err else 'УСПЕШНО ЗАВЕРШЕНО'} |\n  +----------------------------------------------------+\n  | ОЗУ: {ra} МБ (было {rb}) {diff}\n  +----------------------------------------------------+")
    finally: 
        IS_OPTIMIZING = False
    pause()

def main():
    global IS_RESTORING
    set_window_size(64, 24)
    
    while True:
        clear_screen()
        print("\n  +----------------------------------------------------+\n  |          УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ PRO             |\n  +----------------------------------------------------+\n  |  1. Полная оптимизация (ВСЕ этапы)\n  |  2. Выборочная оптимизация\n  |  3. Управление автозагрузкой\n  |  4. Debloat (Удаление мусора)\n  |  5. Проверка системы (SFC / DISM)\n  |  6. Открыть лог\n  |  7. Откатить изменения\n  |  8. Выход\n  +----------------------------------------------------+")
        # При закрытом stdin (EOF) — выход, а не бесконечный цикл
        c = safe_input("  Выбор: ", default="8").strip()
        
        if c == "1":
            if safe_input("  Включить Debloat и отключение служб? (Y/N): ").strip().upper() == "Y":
                run_opt({
                    "is_full": True, "restore": True, "clean": True, 
                    "game_cache": True, "hibernate": True, "visual_fx": True, 
                    "power": True, "services": True, "debloat": True, 
                    "ram": True, "sfc": True
                })
        elif c == "2":
            try:
                flags = {
                    "restore": safe_input("  Точка восстановления? (Y/N): ").strip().upper() == "Y",
                    "clean": safe_input("  Очистка Temp? (Y/N): ").strip().upper() == "Y",
                    "game_cache": safe_input("  Кэш игр? (Y/N): ").strip().upper() == "Y",
                    "hibernate": safe_input("  Отключить гибернацию? (Y/N): ").strip().upper() == "Y",
                    "visual_fx": safe_input("  Отключить эффекты? (Y/N): ").strip().upper() == "Y",
                    "power": safe_input("  Макс. производительность? (Y/N): ").strip().upper() == "Y",
                    "services": safe_input("  Отключить службы? (Y/N): ").strip().upper() == "Y",
                    "debloat": safe_input("  Debloat (Удалить мусор)? (Y/N): ").strip().upper() == "Y",
                    "ram": safe_input("  Оптимизировать ОЗУ? (Y/N): ").strip().upper() == "Y",
                    "sfc": safe_input("  Проверка SFC/DISM? (Y/N): ").strip().upper() == "Y"
                }
                run_opt(flags)
            except Exception as e:
                write_log(f"Ошибка выборочной оптимизации: {e}")
                pause(f"  Ошибка: {e}")
                continue
                
        elif c == "3":
            clear_screen()
            print("\n  +----------------------------------------------------+\n  |                 АВТОЗАГРУЗКА                       |\n  +----------------------------------------------------+")
            apps = []
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Run") as k:
                    i = 0
                    while True:
                        try:
                            name, _, _ = winreg.EnumValue(k, i)
                            if name: 
                                safe_name = textwrap.shorten(name, width=40, placeholder='...')
                                apps.append(name)
                                print(f"  {len(apps)}. {safe_name}")
                            i += 1
                        except OSError: break
            except Exception as e: 
                pause(f"  Ошибка: {e}")
                continue
            
            if apps:
                ch = safe_input("\n  Номер для удаления (Enter - назад): ").strip()
                if ch.isdigit() and 0 < int(ch) <= len(apps):
                    try:
                        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Run", 0, winreg.KEY_SET_VALUE) as k:
                            winreg.DeleteValue(k, apps[int(ch)-1])
                        print("  [+] Удалено.")
                    except Exception as e:
                        print(f"  [!] {e}")
            else:
                print("  Автозагрузка пуста.")
            pause()
            
        elif c == "4":
            debloat()
            pause()
            
        elif c == "5":
            run_opt({"sfc": True})
            
        elif c == "6":
            try:
                if os.path.exists(LOG_FILE): os.startfile(LOG_FILE)
                else: pause("  Лог пуст")
            except Exception as e:
                pause(f"  Ошибка: {e}")
                
        elif c == "7":
            if not os.path.exists(BACKUP_FILE):
                pause("  Бэкап не найден.")
                continue
            IS_RESTORING = True
            try:
                ok = restore_backup()
            finally:
                IS_RESTORING = False
            pause("  Восстановлено успешно." if ok else "  ОШИБКА при восстановлении.")
            
        elif c == "8":
            sys.exit(0)

# ============================================================
# ТОЧКА ВХОДА (ЗАЩИЩЕНА ОТ ЗАКРЫТИЯ)
# ============================================================
if __name__ == "__main__":
    try:
        if not is_admin():
            print("  Запрос прав администратора (UAC)...")
            if run_as_admin():
                print("  Окно закроется, откроется новое с правами админа.")
                time.sleep(2)
                sys.exit(0)
            else:
                print("\n  [ОШИБКА] Не удалось получить права администратора.")
                print("  Возможно, вы отклонили запрос UAC или у вас нет прав.")
                safe_input("\n  Нажмите Enter для выхода...")
                sys.exit(1)
        
        main()
    except Exception:
        print("\n\n  [!] КРИТИЧЕСКАЯ ОШИБКА:")
        traceback.print_exc()
        write_log(traceback.format_exc())
        safe_input("\n  Нажмите Enter для выхода...")
        sys.exit(1)