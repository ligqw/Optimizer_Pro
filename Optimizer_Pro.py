import os
import sys
import subprocess
import ctypes
import time
import shutil

# Проверка ОС
if sys.platform != "win32":
    print("[ОШИБКА] Эта программа предназначена только для Windows!")
    sys.exit(1)

# Глобальные переменные
LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "optimization_log.txt")

# Настройка консоли
kernel32 = ctypes.windll.kernel32
kernel32.SetConsoleOutputCP(65001)
kernel32.SetConsoleCP(65001)
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        return False

def run_as_admin():
    try:
        params = " ".join([f'"{arg}"' for arg in sys.argv])
        result = ctypes.windll.shell32.ShellExecuteW(
            None, "runas", sys.executable, params, None, 1
        )
        return result > 32
    except Exception as e:
        print(f"[!] Ошибка запроса прав: {e}")
        return False

def write_log(text, clear=False):
    mode = "w" if clear else "a"
    try:
        with open(LOG_FILE, mode, encoding="utf-16") as f:
            f.write(f"{text}\n")
    except Exception:
        pass

def run_cmd(cmd, log_text=None, check_error=True):
    """Выполняет команду и проверяет результат."""
    if log_text:
        print(f"  [+] {log_text}...")
        write_log(f"  {log_text}")
    
    try:
        # Захватываем вывод для проверки ошибок
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, encoding='cp866', errors='ignore')
        
        if check_error and result.returncode != 0:
            print("      └─ [ОШИБКА] Команда завершилась с кодом", result.returncode)
            write_log(f"      [ОШИБКА] Код: {result.returncode}. Вывод: {result.stderr or result.stdout}")
            return False
        else:
            print("      └─ [OK]")
            return True
    except Exception as e:
        print(f"      └─ [ОШИБКА] {e}")
        write_log(f"      [ОШИБКА] {e}")
        return False

def get_free_ram():
    """Бенчмарк: получение объема свободной ОЗУ в МБ."""
    try:
        out = subprocess.check_output('wmic OS get FreePhysicalMemory /Value', shell=True, text=True, encoding='cp866', errors='ignore')
        for line in out.split('\n'):
            if 'FreePhysicalMemory' in line:
                return int(line.split('=')[1].strip()) // 1024
    except Exception:
        return 0
    return 0

def open_log():
    if os.path.exists(LOG_FILE):
        os.startfile(LOG_FILE)
    else:
        print("  [!] Журнал работы еще не создан.")
        input("  Нажмите Enter...")

def clear_screen():
    os.system('mode con: cols=70 lines=28')
    os.system('cls')
    os.system('color 0B')

def restore_defaults():
    """Откат изменений: включение служб и сброс питания."""
    clear_screen()
    print("\n")
    print("         ┌──────────────────────────────────────────┐")
    print("         │        ВОССТАНОВЛЕНИЕ НАСТРОЕК           │")
    print("         └──────────────────────────────────────────┘\n")
    print("  [!] Внимание: Эта функция вернет стандартные настройки")
    print("      Windows, которые были изменены оптимизатором.")
    
    ans = input("\n  Вы уверены, что хотите откатить изменения? (Y/N): ").strip().upper()
    if ans != "Y":
        return

    print("\n  [~] Восстановление...")
    
    # Возврат служб
    run_cmd("sc config DiagTrack start= auto && net start DiagTrack", "Служба телеметрии (DiagTrack)", check_error=False)
    run_cmd("sc config NvTelemetryContainer start= auto && net start NvTelemetryContainer", "Служба NVIDIA (NvTelemetry)", check_error=False)
    run_cmd("sc config AMDCrashDefenderService start= auto", "Служба AMD (AMDCrashDefender)", check_error=False)
    
    # Включение GameDVR
    run_cmd('reg add "HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR" /v "AppCaptureEnabled" /t REG_DWORD /d 1 /f', "Включение GameDVR (Пользователь)", check_error=False)
    run_cmd('reg add "HKCU\\System\\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 1 /f', "Включение GameDVR (Система)", check_error=False)
    run_cmd('reg delete "HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR" /v "AllowGameDVR" /f', "Удаление блокировки GameDVR", check_error=False)

    # Сброс схем питания на стандартные
    run_cmd("powercfg -restoredefaultschemes", "Сброс схем питания Windows", check_error=False)
    
    print("\n  [УСПЕШНО] Стандартные настройки восстановлены.")
    input("  Нажмите Enter для возврата в меню...")

def main_menu():
    while True:
        clear_screen()
        print("\n")
        print("           ┌──────────────────────────────────────────┐")
        print("           │       УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ          │")
        print("           ├──────────────────────────────────────────┤")
        print("           │                                          │")
        print("           │  1. Полная оптимизация (Все этапы)       │")
        print("           │                                          │")
        print("           │  2. Выборочная оптимизация               │")
        print("           │                                          │")
        print("           │  3. Проверка системы (SFC / DISM)        │")
        print("           │                                          │")
        print("           │  4. Открыть лог (Журнал)                 │")
        print("           │                                          │")
        print("           │  5. Откатить изменения (Восстановить)    │")
        print("           │                                          │")
        print("           │  6. Выход                                │")
        print("           │                                          │")
        print("           └──────────────────────────────────────────┘")
        
        choice = input("\n  Выбор (1-6): ").strip()
        
        if choice == "1":
            options = {i: True for i in range(1, 8)}
            execute_optimization(options, is_full=True)
        elif choice == "2":
            custom_menu()
        elif choice == "3":
            execute_optimization({7: True}, is_full=False)
        elif choice == "4":
            open_log()
        elif choice == "5":
            restore_defaults()
        elif choice == "6":
            sys.exit(0)

def custom_menu():
    clear_screen()
    print("\n")
    print("         ┌──────────────────────────────────────────┐")
    print("         │        ВЫБОРОЧНАЯ НАСТРОЙКА              │")
    print("         └──────────────────────────────────────────┘")
    print(" * Ввод Y/N. Пустой Enter = N")
    print(" ──────────────────────────────────────────")
    
    opts = {}
    questions = {
        1: "Точка восстановления?",
        2: "Очистка кэша и Temp? ",
        3: "Очистка DNS?         ",
        4: "Питание и ядра CPU?  ",
        5: "Отключить телеметрию?",
        6: "Оптимизация ОЗУ?     ",
        7: "Проверка SFC/DISM?   "
    }
    
    for i in range(1, 8):
        ans = input(f" [{i}/7] {questions[i]} (Y/N): ").strip().upper()
        opts[i] = (ans == "Y")
        
    execute_optimization(opts, is_full=False)

def execute_optimization(opts, is_full=False):
    clear_screen()
    print("\n")
    print("         ┌──────────────────────────────────────────┐")
    print("         │         ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ           │")
    print("         └──────────────────────────────────────────┘\n")
    
    if is_full:
        write_log(f"=== ЛОГ - {time.strftime('%Y-%m-%d %H:%M:%S')} ===", clear=True)
    
    # Бенчмарк ДО
    ram_before = get_free_ram()
    print(f"  [i] Свободно ОЗУ до оптимизации: {ram_before} МБ\n")

    # 1. Точка восстановления
    if opts.get(1):
        run_cmd("net start VSS", "Запуск службы теневого копирования (VSS)", check_error=False)
        run_cmd('reg add "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore" /v "SystemRestorePointCreationFrequency" /t REG_DWORD /d 0 /f', "Снятие ограничений на создание точек", check_error=False)
        success = run_cmd('powershell -NoProfile -Command "Enable-ComputerRestore -Drive \'C:\\\'; Checkpoint-Computer -Description \'Optimizer_Pro\' -RestorePointType \'MODIFY_SETTINGS\'"', "Создание точки восстановления")
        if not success:
            print("      └─ [!] ВНИМАНИЕ: Точка восстановления не создана! Продолжение рискованно.")

    # 2. Очистка Temp и кэша
    if opts.get(2):
        print("  [+] Очистка Temp и кэша...")
        write_log("  Очистка Temp")
        temp_dirs = [os.environ.get('TEMP'), os.path.join(os.environ.get('SystemRoot', 'C:\\Windows'), 'Temp')]
        deleted_count = 0
        for t_dir in temp_dirs:
            if t_dir and os.path.exists(t_dir):
                for filename in os.listdir(t_dir):
                    filepath = os.path.join(t_dir, filename)
                    try:
                        if os.path.isfile(filepath) or os.path.islink(filepath):
                            os.unlink(filepath)
                            deleted_count += 1
                        elif os.path.isdir(filepath):
                            shutil.rmtree(filepath)
                            deleted_count += 1
                    except Exception:
                        pass # Пропускаем занятые файлы
        print(f"      └─ [OK] Удалено объектов: {deleted_count}")
        
        run_cmd("net stop wuauserv && net stop bits", "Остановка служб обновления", check_error=False)
        sw_dist = os.path.join(os.environ.get('SystemRoot', 'C:\\Windows'), 'SoftwareDistribution', 'Download')
        if os.path.exists(sw_dist):
            for filename in os.listdir(sw_dist):
                filepath = os.path.join(sw_dist, filename)
                try:
                    if os.path.isfile(filepath) or os.path.islink(filepath):
                        os.unlink(filepath)
                    elif os.path.isdir(filepath):
                        shutil.rmtree(filepath)
                except Exception:
                    pass
        run_cmd("net start bits && net start wuauserv", "Запуск служб обновления", check_error=False)

    # 3. Сброс DNS
    if opts.get(3):
        run_cmd("ipconfig /flushdns", "Сброс кэша DNS")

    # 4. Настройка питания
    if opts.get(4):
        print("  [+] Настройка питания...")
        write_log("  Питание")
        battery_check = subprocess.run('powershell -NoProfile -Command "if (Get-CimInstance Win32_Battery) { exit 1 }"', shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if battery_check.returncode == 1:
            print("      └─ [ИНФО] Обнаружен ноутбук. Настройка питания пропущена для экономии батареи.")
            write_log("      [ИНФО] Ноутбук. Пропущено.")
        else:
            run_cmd("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", "Включение схемы 'Высокая производительность'", check_error=False)
            run_cmd("powercfg /attributes 54533251-82bd-4e29-950d-4136c166a751 0cc5b647-c36e-417e-ba9d-d17442bc94de -ATTRIB_HIDE", "Разблокировка настроек процессора", check_error=False)
            run_cmd("powercfg /setacvalueindex 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c 54533251-82bd-4e29-950d-4136c166a751 0cc5b647-c36e-417e-ba9d-d17442bc94de 0", "Установка минимального состояния процессора (100%)", check_error=False)
            run_cmd("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", "Применение схемы питания", check_error=False)

    # 5. Отключение телеметрии
    if opts.get(5):
        run_cmd("sc config DiagTrack start= disabled && net stop DiagTrack", "Отключение телеметрии (DiagTrack)", check_error=False)
        run_cmd("sc config NvTelemetryContainer start= disabled && net stop NvTelemetryContainer", "Отключение NVIDIA Телеметрии", check_error=False)
        run_cmd("sc config AMDCrashDefenderService start= disabled && net stop AMDCrashDefenderService", "Отключение AMD Телеметрии", check_error=False)
        run_cmd('reg add "HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR" /v "AppCaptureEnabled" /t REG_DWORD /d 0 /f', "Отключение GameDVR (Пользователь)", check_error=False)
        run_cmd('reg add "HKCU\\System\\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 0 /f', "Отключение GameDVR (Система)", check_error=False)
        run_cmd('reg add "HKLM\\SOFTWARE\\Microsoft\\PolicyManager\\default\\ApplicationManagement\\AllowGameDVR" /v "value" /t REG_DWORD /d 0 /f', "Отключение GameDVR (Политика)", check_error=False)
        run_cmd('reg add "HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR" /v "AllowGameDVR" /t REG_DWORD /d 0 /f', "Блокировка GameDVR", check_error=False)

    # 6. Оптимизация ОЗУ (Безопасная)
    if opts.get(6):
        print("  [+] Оптимизация ОЗУ...")
        # Вместо убийства всех зависших процессов, просто очищаем рабочие наборы активных программ
        ps_ram = 'powershell -NoProfile -EncodedCommand "JGM9J1tEbGxJbXBvcnQoInBzYXBpLmRsbCIpXXB1YmxpYyBzdGF0aWMgZXh0ZXJuIGJvb2wgRW1wdHlXb3JraW5nU2V0KEludFB0ciBoKTsnOyR0PUFkZC1UeXBlIC1NZW1iZXJEZWZpbml0aW9uICRjIC1OYW1lICdNJyAtTmFtZXNwYWNlICdXJyAtUGFzc1RocnU7R2V0LVByb2Nlc3N8P3skXy5NYWluV2luZG93SGFuZGxlfXxGb3JFYWNoLU9iamVjdHt0cnl7JHQ6OkVtcHR5V29ya2luZ1NldCgkXy5IYW5kbGUpfWNhdGNoe319"'
        run_cmd(ps_ram, "Очистка рабочих наборов памяти (EmptyWorkingSet)", check_error=False)

    # 7. Проверка SFC / DISM
    if opts.get(7):
        print("\n  [+] Запуск системного сканирования (SFC / DISM)...")
        print("  [!] Шаг может занять 15-20 минут. Не закрывайте окно.")
        write_log("  DISM/SFC")
        
        def run_and_log(cmd, temp_file):
            with open(temp_file, "w", encoding="utf-8") as f:
                subprocess.run(cmd, shell=True, stdout=f, stderr=subprocess.STDOUT)
            with open(temp_file, "r", encoding="utf-8", errors="ignore") as f:
                write_log(f.read())
            os.remove(temp_file)

        run_and_log("dism /online /cleanup-image /scanhealth", "d1.tmp")
        run_and_log("dism /online /cleanup-image /restorehealth", "d2.tmp")
        run_and_log("sfc /scannow", "s1.tmp")

    # Бенчмарк ПОСЛЕ
    ram_after = get_free_ram()
    ram_diff = ram_after - ram_before
    
    print("\n         ┌──────────────────────────────────────────┐")
    print("         │         ОПТИМИЗАЦИЯ ЗАВЕРШЕНА!           │")
    print("         ├──────────────────────────────────────────┤")
    print(f"         │  * Свободно ОЗУ: {ram_after} МБ (было {ram_before})")
    if ram_diff > 0:
        print(f"         │  * Освобождено: +{ram_diff} МБ")
    print("         │  * Все подробности в лог-файле.          │")
    print("         │  * Рекомендуется перезагрузить ПК.       │")
    print("         └──────────────────────────────────────────┘\n")
    
    write_log(f"[КОНЕЦ] Успешно. ОЗУ до: {ram_before} МБ, после: {ram_after} МБ.")
    input("  Нажмите Enter для возврата в меню...")

if __name__ == "__main__":
    if not is_admin():
        print("[i] Требуются права администратора. Запрашиваю UAC...")
        if run_as_admin():
            sys.exit(0)
        else:
            print("[ОШИБКА] Не удалось получить права администратора.")
            input("Нажмите Enter для выхода...")
            sys.exit(1)

    main_menu()
