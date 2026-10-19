import os
import sys
import subprocess
import ctypes
import time
import shutil
import winreg

# Принудительно включаем UTF-8 для консоли, чтобы не было кракозябр
if sys.platform == "win32":
    ctypes.windll.kernel32.SetConsoleOutputCP(65001)
    ctypes.windll.kernel32.SetConsoleCP(65001)
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')

LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "optimization_log.txt")

def is_admin():
    try: return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except: return False

def run_as_admin():
    try:
        params = " ".join([f'"{arg}"' for arg in sys.argv])
        result = ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, params, None, 1)
        return result > 32
    except: return False

def write_log(text, clear=False):
    mode = "w" if clear else "a"
    try:
        with open(LOG_FILE, mode, encoding="utf-16") as f:
            f.write(f"{text}\n")
    except: pass

def run_cmd(cmd, log_text=None, check_error=True):
    if log_text:
        print(f"  [+] {log_text}...")
        write_log(f"  {log_text}")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, encoding='cp866', errors='ignore')
        if check_error and result.returncode != 0:
            print("      +-- [ОШИБКА]")
            return False
        else:
            print("      +-- [OK]")
            return True
    except Exception as e:
        print(f"      +-- [ОШИБКА] {e}")
        return False

def get_free_ram():
    try:
        out = subprocess.check_output('wmic OS get FreePhysicalMemory /Value', shell=True, text=True, encoding='cp866', errors='ignore')
        for line in out.split('\n'):
            if 'FreePhysicalMemory' in line: return int(line.split('=')[1].strip()) // 1024
    except: return 0
    return 0

# --- ОПТИМИЗАЦИЯ ---
def disable_hibernation():
    run_cmd("powercfg -h off", "Отключение гибернации (освобождение места)")

def set_visual_effects():
    run_cmd('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d 2 /f', "Отключение визуальных эффектов")

def enable_ultimate_power():
    subprocess.run("powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    run_cmd("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61", "Включение макс. производительности")

def empty_recycle_bin():
    run_cmd('powershell -NoProfile -Command "Clear-RecycleBin -Force"', "Очистка Корзины")

def clean_game_launchers():
    print("  [+] Очистка кэша Steam, Epic Games, Riot...")
    paths = [
        os.path.expandvars(r"%LOCALAPPDATA%\Steam\htmlcache"),
        os.path.expandvars(r"%LOCALAPPDATA%\EpicGamesLauncher\Saved\webcache"),
        os.path.expandvars(r"%LOCALAPPDATA%\Riot Games\Riot Client\Cache")
    ]
    for path in paths:
        if os.path.exists(path):
            for filename in os.listdir(path):
                filepath = os.path.join(path, filename)
                try:
                    if os.path.isfile(filepath) or os.path.islink(filepath): os.unlink(filepath)
                    elif os.path.isdir(filepath): shutil.rmtree(filepath)
                except: pass
    print("      +-- [OK]")

def safe_ram_clean():
    print("  [+] Очистка ОЗУ...")
    ps_script = "$critical = @('explorer', 'svchost', 'csrss', 'wininit', 'services', 'lsass', 'winlogon', 'System', 'Registry'); $c = '[DllImport(\"psapi.dll\")]public static extern bool EmptyWorkingSet(IntPtr h);'; $t = Add-Type -MemberDefinition $c -Name 'M' -Namespace 'W' -PassThru; Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $critical -notcontains $_.ProcessName } | ForEach-Object { try { $t::EmptyWorkingSet($_.Handle) } catch {} }"
    import base64
    encoded = base64.b64encode(ps_script.encode('utf-16le')).decode('utf-8')
    subprocess.run(f'powershell -NoProfile -EncodedCommand {encoded}', shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("      +-- [OK]")

def debloat_windows():
    print("  [+] Debloat (Удаление встроенных приложений)...")
    apps = ["*Microsoft.BingWeather*", "*Microsoft.GetHelp*", "*Microsoft.Getstarted*", "*Microsoft.MicrosoftOfficeHub*", "*Microsoft.SkypeApp*", "*Microsoft.XboxApp*", "*Microsoft.ZuneMusic*"]
    for app in apps:
        subprocess.run(f'powershell -NoProfile -Command "Get-AppxPackage {app} | Remove-AppxPackage"', shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("      +-- [OK]")

def manage_startup():
    clear_screen()
    print("\n  +----------------------------------------------------+")
    print("  |        ПРОГРАММЫ В АВТОЗАГРУЗКЕ (HKCU)             |")
    print("  +----------------------------------------------------+")
    key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
    try:
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path, 0, winreg.KEY_READ)
        i = 0
        while True:
            try:
                name, value, _ = winreg.EnumValue(key, i)
                print(f"  | - {name[:30]:<48} |")
                i += 1
            except OSError: break
        winreg.CloseKey(key)
    except Exception as e:
        print(f"  | Ошибка чтения: {str(e)[:30]:<32} |")
    print("  +----------------------------------------------------+")
    input("\n  Нажмите Enter для возврата...")

# --- ЛОГИКА ---
def run_optimization(flags):
    clear_screen()
    print("\n  +----------------------------------------------------+")
    print("  |            ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ                  |")
    print("  +----------------------------------------------------+\n")
    ram_before = get_free_ram()
    print(f"  [i] Свободно ОЗУ до: {ram_before} МБ\n")

    if flags.get("restore"):
        print("  [+] Создание точки восстановления...")
        run_cmd("net start VSS", check_error=False)
        run_cmd('reg add "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore" /v "SystemRestorePointCreationFrequency" /t REG_DWORD /d 0 /f', check_error=False)
        run_cmd('powershell -NoProfile -Command "Enable-ComputerRestore -Drive \'C:\\\'; Checkpoint-Computer -Description \'Optimizer\' -RestorePointType \'MODIFY_SETTINGS\'"', check_error=False)
    
    if flags.get("clean"):
        print("  [+] Очистка Temp...")
        temp_dirs = [os.environ.get('TEMP'), os.path.join(os.environ.get('SystemRoot', 'C:\\Windows'), 'Temp')]
        for t_dir in temp_dirs:
            if t_dir and os.path.exists(t_dir):
                for filename in os.listdir(t_dir):
                    filepath = os.path.join(t_dir, filename)
                    try:
                        if os.path.isfile(filepath) or os.path.islink(filepath): os.unlink(filepath)
                        elif os.path.isdir(filepath): shutil.rmtree(filepath)
                    except: pass
        print("      +-- [OK]")

    if flags.get("game_cache"): clean_game_launchers()
    if flags.get("hibernate"): disable_hibernation()
    if flags.get("visual_fx"): set_visual_effects()
    if flags.get("power"): enable_ultimate_power()
    if flags.get("recycle"): empty_recycle_bin()
    if flags.get("debloat"): debloat_windows()
    if flags.get("ram"): safe_ram_clean()

    if flags.get("sfc"):
        print("  [+] Проверка SFC/DISM (Это займет 15-20 минут)...")
        run_cmd("dism /online /cleanup-image /scanhealth", check_error=False)
        run_cmd("dism /online /cleanup-image /restorehealth", check_error=False)
        run_cmd("sfc /scannow", check_error=False)

    ram_after = get_free_ram()
    print("\n  +----------------------------------------------------+")
    print("  |            ОПТИМИЗАЦИЯ ЗАВЕРШЕНА!                  |")
    print("  +----------------------------------------------------+")
    print(f"  |  * ОЗУ после: {ram_after} МБ (было {ram_before})")
    if ram_after > ram_before:
        print(f"  |  * Освобождено: +{ram_after - ram_before} МБ")
    print("  +----------------------------------------------------+")
    input("\n  Нажмите Enter для возврата...")

def clear_screen():
    os.system('mode con: cols=56 lines=28')
    os.system('cls')
    os.system('color 0B')

def main_menu():
    while True:
        clear_screen()
        print("\n  +----------------------------------------------------+")
        print("  |          УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ PRO             |")
        print("  +----------------------------------------------------+")
        print("  |  1. Полная оптимизация (Все этапы)                 |")
        print("  |  2. Выборочная оптимизация                         |")
        print("  |  3. Управление автозагрузкой                       |")
        print("  |  4. Debloat (Удаление встроенного мусора)          |")
        print("  |  5. Проверка системы (SFC / DISM)                  |")
        print("  |  6. Открыть лог (Журнал)                           |")
        print("  |  7. Откатить изменения (Восстановить)              |")
        print("  |  8. Выход                                          |")
        print("  +----------------------------------------------------+")
        
        choice = input("  Выбор: ").strip()
        
        if choice == "1":
            run_optimization({
                "restore": True, "clean": True, "game_cache": True, 
                "hibernate": True, "visual_fx": True, "power": True, 
                "recycle": True, "ram": True
            })
        elif choice == "2":
            flags = {
                "restore": input("  Точка восстановления? (Y/N): ").upper() == "Y",
                "clean": input("  Очистка Temp? (Y/N): ").upper() == "Y",
                "game_cache": input("  Кэш игр? (Y/N): ").upper() == "Y",
                "hibernate": input("  Отключить гибернацию? (Y/N): ").upper() == "Y",
                "visual_fx": input("  Отключить визуальные эффекты? (Y/N): ").upper() == "Y",
                "power": input("  Включить макс. производительность? (Y/N): ").upper() == "Y",
                "recycle": input("  Очистить корзину? (Y/N): ").upper() == "Y",
                "debloat": input("  Debloat (Удалить мусор)? (Y/N): ").upper() == "Y",
                "ram": input("  Очистить ОЗУ? (Y/N): ").upper() == "Y",
                "sfc": input("  Проверка SFC/DISM? (Y/N): ").upper() == "Y"
            }
            run_optimization(flags)
        elif choice == "3": manage_startup()
        elif choice == "4":
            if input("  Удалить встроенный мусор? (Y/N): ").upper() == "Y":
                debloat_windows()
                input("\n  Нажмите Enter...")
        elif choice == "5": run_optimization({"sfc": True})
        elif choice == "6":
            if os.path.exists(LOG_FILE): os.startfile(LOG_FILE)
            else: input("  Лог не создан.")
        elif choice == "7":
            print("\n  [~] Сброс служб и питания...")
            run_cmd("sc config DiagTrack start= auto && net start DiagTrack", check_error=False)
            run_cmd("sc config SysMain start= auto && net start SysMain", check_error=False)
            run_cmd("sc config WSearch start= auto && net start WSearch", check_error=False)
            run_cmd("powercfg -restoredefaultschemes", check_error=False)
            run_cmd("powercfg -h on", check_error=False)
            run_cmd('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d 0 /f', check_error=False)
            input("\n  Настройки сброшены. Нажмите Enter...")
        elif choice == "8": sys.exit(0)

if __name__ == "__main__":
    if not is_admin():
        print("Запрос UAC...")
        if run_as_admin(): sys.exit(0)
        else: sys.exit(1)
    main_menu()