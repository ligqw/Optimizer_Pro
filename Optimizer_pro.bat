@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
title Optimizer Pro
cd /d "%~dp0"

:: ============================================================
:: ПРОВЕРКА ПРАВ АДМИНИСТРАТОРА И САМОПЕРЕЗАПУСК
:: ============================================================
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo   Запрос прав администратора ^(UAC^)...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: ============================================================
:: КОНСТАНТЫ И ПУТИ
:: ============================================================
set "BACKUP_DIR=%SystemRoot%\Temp\OptimizerPro"
set "LOG_FILE=%BACKUP_DIR%\optimization_log.txt"
set "BACKUP_FLAG=%BACKUP_DIR%\backup.done"
set "SERVICES_FILE=%BACKUP_DIR%\services.txt"
set "APPS_FILE=%BACKUP_DIR%\removed_appx.txt"
set "SCHEME_FILE=%BACKUP_DIR%\power_scheme.txt"
set "HIB_FILE=%BACKUP_DIR%\hibernate.txt"
set "VFX_FILE=%BACKUP_DIR%\vfx.txt"
set "ULTIMATE_GUID=e9a42b02-d5df-448d-aa00-03f14749eb61"
set "HIGH_PERF_GUID=8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"
set "PS=powershell -NoProfile -ExecutionPolicy Bypass"

if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%" 2>nul
if not exist "%LOG_FILE%" echo === Optimizer Pro Log === >"%LOG_FILE%"

:: Широкое окно, чтобы длинные строки гарантированно не переносились
mode con: cols=100 lines=32 >nul 2>&1

:: Флаги текущей сессии
set "F_RESTORE=0"
set "F_CLEAN=0"
set "F_GAME=0"
set "F_HIB=0"
set "F_VFX=0"
set "F_POWER=0"
set "F_SERVICES=0"
set "F_DEBLOAT=0"
set "F_RAM=0"
set "F_SFC=0"
set "F_FULL=0"
set "ERR=0"

:: ============================================================
:: ГЛАВНОЕ МЕНЮ
:: ============================================================
:main_menu
cls
echo.
echo ====================================================
echo              УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ PRO
echo ====================================================
echo   1. Полная оптимизация (ВСЕ этапы)
echo   2. Выборочная оптимизация
echo   3. Управление автозагрузкой
echo   4. Debloat (Удаление мусора)
echo   5. Проверка системы (SFC / DISM)
echo   6. Открыть лог
echo   7. Откатить изменения
echo   8. Выход
echo ====================================================
choice /c 12345678 /n /m "  Выбор: "
if errorlevel 8 goto do_exit
if errorlevel 7 goto do_rollback
if errorlevel 6 goto do_view_log
if errorlevel 5 goto do_check_system
if errorlevel 4 goto do_debloat_standalone
if errorlevel 3 goto do_startup
if errorlevel 2 goto do_custom_opt
if errorlevel 1 goto do_full_opt
goto main_menu

:: ============================================================
:: ХЕЛПЕР: ЛОГИРОВАНИЕ
:: ============================================================
:log_it
echo %~1
>>"%LOG_FILE%" echo %~1
exit /b 0

:: ============================================================
:: ХЕЛПЕР: ВОПРОС Y/N -> ПЕРЕМЕННАЯ
:: ============================================================
:ask_yn
choice /c YN /n /m "%~1 (Y/N): "
if errorlevel 2 (set "%~2=0") else (set "%~2=1")
exit /b 0

:: ============================================================
:: ХЕЛПЕР: ОЧИСТКА ПАПКИ (с защитой бэкапа)
:: ============================================================
:clean_dir
set "DIR=%~1"
if not exist "%DIR%" exit /b 0
pushd "%DIR%" 2>nul
if errorlevel 1 exit /b 0
for /f "delims=" %%f in ('dir /a /b 2^>nul') do (
    if /i not "%%f"=="OptimizerPro" (
        if exist "%%f\" (
            rmdir /s /q "%%f" 2>nul
        ) else (
            del /f /q "%%f" 2>nul
        )
    )
)
popd
exit /b 0

:: ============================================================
:: БЭКАП
:: ============================================================
:save_backup
call :log_it "  [+] Создание резервной копии..."
if exist "%BACKUP_FLAG%" (
    call :log_it "      +-- [ПРОПУЩЕНО] Бэкап уже существует (сохранено исходное состояние)"
    exit /b 0
)

:: Схема питания
set "CUR_SCHEME="
for /f "delims=" %%a in ('%PS% -Command "$s=(powercfg /getactivescheme) -join ' '; [regex]::Match($s,'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}').Value"') do set "CUR_SCHEME=%%a"
if defined CUR_SCHEME (
    >"%SCHEME_FILE%" echo !CUR_SCHEME!
) else (
    >"%SCHEME_FILE%" echo NONE
)

:: Гибернация
set "HIB_STATE=0x0"
for /f "tokens=3" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Power" /v HibernateEnabled 2^>nul ^| findstr /i "REG_DWORD"') do set "HIB_STATE=%%a"
if /i "!HIB_STATE!"=="0x0" (
    if exist "%SystemDrive%\hiberfil.sys" set "HIB_STATE=0x1"
)
>"%HIB_FILE%" echo !HIB_STATE!

:: VisualFXSetting
set "VFX_EXISTS=0"
set "VFX_VAL="
for /f "tokens=3" %%a in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v VisualFXSetting 2^>nul ^| findstr /i "REG_DWORD"') do (
    set "VFX_EXISTS=1"
    set "VFX_VAL=%%a"
)
>"%VFX_FILE%" echo !VFX_EXISTS!^|!VFX_VAL!

:: Службы (старт + состояние)
> "%SERVICES_FILE%" (
    for %%s in (DiagTrack SysMain WSearch wuauserv bits) do (
        set "ST=auto"
        set "RN=0"
        for /f "tokens=3" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Services\%%s" /v Start 2^>nul ^| findstr /i "REG_DWORD"') do (
            if /i "%%a"=="0x2" set "ST=auto"
            if /i "%%a"=="0x3" set "ST=demand"
            if /i "%%a"=="0x4" set "ST=disabled"
        )
        sc query %%s 2>nul ^| findstr /i "RUNNING" >nul && set "RN=1"
        echo %%s^|!ST!^|!RN!
    )
)

>"%BACKUP_FLAG%" echo 1
call :log_it "      +-- [OK]"
exit /b 0

:: ============================================================
:: ВОССТАНОВЛЕНИЕ
:: ============================================================
:restore_backup
call :log_it "  [+] Восстановление..."

if not exist "%BACKUP_FLAG%" (
    call :log_it "      +-- [ОШИБКА] Бэкап не найден"
    exit /b 1
)

:: Схема питания
if exist "%SCHEME_FILE%" (
    set "OLD_SCHEME="
    set /p OLD_SCHEME=<"%SCHEME_FILE%"
    if defined OLD_SCHEME if /i not "!OLD_SCHEME!"=="NONE" (
        powercfg /setactive "!OLD_SCHEME!" >nul 2>&1
        if !errorlevel! equ 0 (
            call :log_it "      +-- [OK] Схема питания восстановлена"
        ) else (
            call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] Не удалось восстановить схему"
        )
    )
)

:: Гибернация
if exist "%HIB_FILE%" (
    set "OLD_HIB="
    set /p OLD_HIB=<"%HIB_FILE%"
    if /i "!OLD_HIB!"=="0x1" (
        powercfg -h on >nul 2>&1
        call :log_it "      +-- [OK] Гибернация включена"
    ) else (
        powercfg -h off >nul 2>&1
        call :log_it "      +-- [OK] Гибернация отключена"
    )
)

:: Визуальные эффекты
if exist "%VFX_FILE%" (
    set "VFX_LINE="
    set /p VFX_LINE=<"%VFX_FILE%"
    for /f "tokens=1,2 delims=|" %%a in ("!VFX_LINE!") do (
        if "%%a"=="1" (
            reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v VisualFXSetting /t REG_DWORD /d %%b /f >nul 2>&1
        ) else (
            reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v VisualFXSetting /f >nul 2>&1
        )
    )
    call :log_it "      +-- [OK] Визуальные эффекты восстановлены"
)

:: Службы
if exist "%SERVICES_FILE%" (
    for /f "tokens=1,2,3 delims=|" %%a in ('type "%SERVICES_FILE%" 2^>nul') do (
        sc config %%a start= %%b >nul 2>&1
        if "%%c"=="1" (
            sc start %%a >nul 2>&1
        ) else (
            sc stop %%a >nul 2>&1
        )
    )
    call :log_it "      +-- [OK] Службы восстановлены"
)

:: Appx
if exist "%APPS_FILE%" (
    call :log_it "  [^!] ВНИМАНИЕ: Удалённые Appx нельзя вернуть автоматически:"
    for /f "delims=" %%a in ('type "%APPS_FILE%" 2^>nul') do call :log_it "      - %%a"
)

del "%BACKUP_FLAG%" >nul 2>&1
call :log_it "      +-- [OK] Восстановление завершено"
exit /b 0

:: ============================================================
:: ПОЛНАЯ ОПТИМИЗАЦИЯ
:: ============================================================
:do_full_opt
cls
echo.
echo   Включить Debloat и отключение служб? (Y/N)
choice /c YN /n /m "  Выбор: "
if errorlevel 2 (
    set "F_SERVICES=0"
    set "F_DEBLOAT=0"
) else (
    set "F_SERVICES=1"
    set "F_DEBLOAT=1"
)
set "F_RESTORE=1"
set "F_CLEAN=1"
set "F_GAME=1"
set "F_HIB=1"
set "F_VFX=1"
set "F_POWER=1"
set "F_RAM=1"
set "F_SFC=1"
set "F_FULL=1"
goto run_optimization

:: ============================================================
:: ВЫБОРОЧНАЯ ОПТИМИЗАЦИЯ
:: ============================================================
:do_custom_opt
cls
echo.
echo ====================================================
echo              ВЫБОРОЧНАЯ ОПТИМИЗАЦИЯ
echo ====================================================
set "F_RESTORE=0"  & call :ask_yn "  Точка восстановления?" "F_RESTORE"
set "F_CLEAN=0"    & call :ask_yn "  Очистка Temp?" "F_CLEAN"
set "F_GAME=0"     & call :ask_yn "  Кэш игр?" "F_GAME"
set "F_HIB=0"      & call :ask_yn "  Отключить гибернацию?" "F_HIB"
set "F_VFX=0"      & call :ask_yn "  Отключить эффекты?" "F_VFX"
set "F_POWER=0"    & call :ask_yn "  Макс. производительность?" "F_POWER"
set "F_SERVICES=0" & call :ask_yn "  Отключить службы?" "F_SERVICES"
set "F_DEBLOAT=0"  & call :ask_yn "  Debloat (удалить мусор)?" "F_DEBLOAT"
set "F_RAM=0"      & call :ask_yn "  Оптимизировать ОЗУ?" "F_RAM"
set "F_SFC=0"      & call :ask_yn "  Проверка SFC/DISM?" "F_SFC"
set "F_FULL=0"
goto run_optimization

:: ============================================================
:: ПРОВЕРКА СИСТЕМЫ (из меню)
:: ============================================================
:do_check_system
set "F_RESTORE=0"
set "F_CLEAN=0"
set "F_GAME=0"
set "F_HIB=0"
set "F_VFX=0"
set "F_POWER=0"
set "F_SERVICES=0"
set "F_DEBLOAT=0"
set "F_RAM=0"
set "F_SFC=1"
set "F_FULL=0"
goto run_optimization

:: ============================================================
:: ВЫПОЛНЕНИЕ
:: ============================================================
:run_optimization
cls
echo.
echo ====================================================
echo              ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ
echo ====================================================
echo.

set "NEED_BACKUP=0"
if "%F_SERVICES%"=="1" set "NEED_BACKUP=1"
if "%F_VFX%"=="1" set "NEED_BACKUP=1"
if "%F_POWER%"=="1" set "NEED_BACKUP=1"
if "%F_HIB%"=="1" set "NEED_BACKUP=1"
if "%F_DEBLOAT%"=="1" set "NEED_BACKUP=1"

set "ERR=0"
if "%NEED_BACKUP%"=="1" (
    call :save_backup
    if !errorlevel! neq 0 (
        echo   [^!] КРИТИЧЕСКАЯ ОШИБКА: бэкап не создан, откат будет невозможен.
        choice /c YN /n /m "  Продолжить? (Y/N): "
        if errorlevel 2 goto opt_end_cancel
        set "ERR=1"
    )
)

if "%F_RESTORE%"=="1" call :do_restore_point
if "%F_CLEAN%"=="1"   call :do_clean_temp
if "%F_GAME%"=="1"    call :do_clean_game
if "%F_HIB%"=="1"     call :do_hibernate
if "%F_VFX%"=="1"     call :do_visual_fx
if "%F_POWER%"=="1"   call :do_power
if "%F_SERVICES%"=="1" call :do_services
if "%F_DEBLOAT%"=="1" call :do_debloat
if "%F_RAM%"=="1"     call :do_ram
if "%F_SFC%"=="1"     call :do_sfc_dism

echo.
echo ====================================================
if !ERR! equ 0 (
    echo              УСПЕШНО ЗАВЕРШЕНО
) else (
    echo              ЗАВЕРШЕНО С ОШИБКАМИ
)
echo ====================================================
echo.
pause
goto main_menu

:opt_end_cancel
call :log_it "  [^!] Оптимизация отменена пользователем"
pause
goto main_menu

:: ============================================================
:: ДЕЙСТВИЕ: ТОЧКА ВОССТАНОВЛЕНИЯ
:: ============================================================
:do_restore_point
call :log_it "  [+] Создание точки восстановления..."
%PS% -Command "try { Enable-ComputerRestore -Drive '%SystemDrive%\'; Checkpoint-Computer -Description 'Optimizer Pro' -RestorePointType 'MODIFY_SETTINGS' -EA Stop; exit 0 } catch { exit 1 }" >nul 2>&1
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] Не удалось создать точку"
)
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: ОЧИСТКА TEMP
:: ============================================================
:do_clean_temp
call :log_it "  [+] Остановка служб обновлений..."
sc stop wuauserv >nul 2>&1
sc stop bits >nul 2>&1
ping -n 3 127.0.0.1 >nul

call :log_it "  [+] Очистка Temp..."
call :clean_dir "%TEMP%"
call :clean_dir "%USERPROFILE%\AppData\Local\Temp"
call :clean_dir "%SystemRoot%\Temp"
call :clean_dir "%SystemRoot%\SoftwareDistribution\Download"
call :log_it "      +-- [OK]"

sc start wuauserv >nul 2>&1
sc start bits >nul 2>&1
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: КЭШ ИГР
:: ============================================================
:do_clean_game
echo.
echo   [^!] ВНИМАНИЕ: Steam/Epic/Riot будут принудительно закрыты.
choice /c YN /n /m "  Продолжить? (Y/N): "
if errorlevel 2 exit /b 0

taskkill /f /im steam.exe >nul 2>&1
taskkill /f /im epicgameslauncher.exe >nul 2>&1
taskkill /f /im riotclient.exe >nul 2>&1

call :log_it "  [+] Очистка кэша игр..."
call :clean_dir "%LOCALAPPDATA%\Steam\htmlcache"
call :clean_dir "%LOCALAPPDATA%\EpicGamesLauncher\Saved\webcache"
call :clean_dir "%LOCALAPPDATA%\D3DSCache"
call :log_it "      +-- [OK]"
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: ГИБЕРНАЦИЯ
:: ============================================================
:do_hibernate
call :log_it "  [+] Отключение гибернации..."
powercfg -h off >nul 2>&1
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ОШИБКА] Не удалось отключить"
    set "ERR=1"
)
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: ВИЗУАЛЬНЫЕ ЭФФЕКТЫ
:: ============================================================
:do_visual_fx
call :log_it "  [+] Отключение эффектов..."
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v VisualFXSetting /t REG_DWORD /d 2 /f >nul 2>&1
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ОШИБКА]"
    set "ERR=1"
)
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: ПИТАНИЕ
:: ============================================================
:do_power
call :log_it "  [+] Настройка питания..."
set "NEW_SCHEME="
for /f "delims=" %%a in ('%PS% -Command "$o=(powercfg -duplicatescheme %ULTIMATE_GUID% 2>&1) -join ' '; $m=[regex]::Match($o,'[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}'); if($m.Success){$m.Value}"') do set "NEW_SCHEME=%%a"

if defined NEW_SCHEME (
    powercfg /setactive !NEW_SCHEME! >nul 2>&1
    if !errorlevel! equ 0 (
        call :log_it "      +-- [OK] Ultimate Performance активирована"
        exit /b 0
    )
)

call :log_it "      [^!] Ultimate не удался, пробую High Performance..."
powercfg /setactive %HIGH_PERF_GUID% >nul 2>&1
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK] High Performance активирована"
) else (
    call :log_it "      +-- [ОШИБКА] Не удалось сменить схему"
    set "ERR=1"
)
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: СЛУЖБЫ
:: ============================================================
:do_services
call :log_it "  [+] Отключение служб..."
for %%s in (DiagTrack SysMain WSearch) do (
    sc config %%s start= disabled >nul 2>&1
    sc stop %%s >nul 2>&1
)
call :log_it "      +-- [OK]"
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: DEBLOAT
:: ============================================================
:do_debloat
if not exist "%APPS_FILE%" type nul >"%APPS_FILE%"
call :log_it "  [+] Debloat..."
for %%a in (BingWeather GetHelp Getstarted MicrosoftOfficeHub SkypeApp XboxApp ZuneMusic) do (
    %PS% -Command "try { Get-AppxPackage *%%a* -AllUsers -EA SilentlyContinue | Remove-AppxPackage -AllUsers -EA SilentlyContinue; Get-AppxProvisionedPackage -Online -EA SilentlyContinue | Where-Object {$_.DisplayName -like '*%%a*'} | Remove-AppxProvisionedPackage -Online -EA SilentlyContinue | Out-Null; exit 0 } catch { exit 1 }" >nul 2>&1
    if !errorlevel! equ 0 (
        >>"%APPS_FILE%" echo %%a
        call :log_it "      +-- [OK] Удалено: %%a"
    ) else (
        call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] Не удалось: %%a"
    )
)
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: ОЗУ
:: ============================================================
:do_ram
call :log_it "  [+] Оптимизация ОЗУ..."
rundll32.exe advapi32.dll,ProcessIdleTasks
call :log_it "      +-- [OK]"
exit /b 0

:: ============================================================
:: ДЕЙСТВИЕ: SFC / DISM
:: ============================================================
:do_sfc_dism
call :log_it "  [+] Проверка DISM (ScanHealth)..."
call :log_it "      Пожалуйста, подождите, это может занять несколько минут..."
dism /online /cleanup-image /scanhealth
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] DISM ScanHealth завершился с кодом !errorlevel!"
)

call :log_it "  [+] Восстановление DISM (RestoreHealth)..."
call :log_it "      Пожалуйста, подождите..."
dism /online /cleanup-image /restorehealth
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] DISM RestoreHealth завершился с кодом !errorlevel!"
)

call :log_it "  [+] Проверка SFC..."
call :log_it "      Пожалуйста, подождите..."
sfc /scannow
if !errorlevel! equ 0 (
    call :log_it "      +-- [OK]"
) else (
    call :log_it "      +-- [ПРЕДУПРЕЖДЕНИЕ] SFC завершился с кодом !errorlevel!"
)
exit /b 0

:: ============================================================
:: DEBLOAT (из меню)
:: ============================================================
:do_debloat_standalone
cls
echo.
echo ====================================================
echo                    DEBLOAT
echo ====================================================
choice /c YN /n /m "  Удалить встроенные приложения? (Y/N): "
if errorlevel 2 goto main_menu
call :do_debloat
pause
goto main_menu

:: ============================================================
:: АВТОЗАГРУЗКА
:: ============================================================
:do_startup
cls
echo.
echo ====================================================
echo                    АВТОЗАГРУЗКА
echo ====================================================
echo.
set "COUNT=0"
for /f "delims=" %%a in ('%PS% -Command "$p=Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -EA SilentlyContinue; if($p){$p.PSObject.Properties ^| Where-Object {$_.Name -notmatch '^PS'} ^| ForEach-Object {$_.Name}}" 2^>nul') do (
    set /a COUNT+=1
    set "NAME_!COUNT!=%%a"
    echo   !COUNT!. %%a
)

if !COUNT! equ 0 (
    echo   Автозагрузка пуста.
    pause
    goto main_menu
)

echo.
set /p "CH=  Номер для удаления (Enter - назад): "
if not defined CH goto main_menu
if !CH! lss 1 goto main_menu
if !CH! gtr !COUNT! goto main_menu

call set "TARGET=%%NAME_!CH!%%"
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "!TARGET!" /f >nul 2>&1
if !errorlevel! equ 0 (
    echo   [+] Удалено: !TARGET!
) else (
    echo   [^!] Ошибка удаления: %TARGET%
)
pause
goto main_menu

:: ============================================================
:: ПРОСМОТР ЛОГА
:: ============================================================
:do_view_log
if exist "%LOG_FILE%" (
    start "" notepad "%LOG_FILE%"
) else (
    echo   Лог пуст.
    pause
)
goto main_menu

:: ============================================================
:: ОТКАТ
:: ============================================================
:do_rollback
cls
echo.
if not exist "%BACKUP_FLAG%" (
    echo   Бэкап не найден — откатывать нечего.
    pause
    goto main_menu
)
echo   [^!] Будет выполнен откат всех изменений. Продолжить?
choice /c YN /n /m "  Выбор: "
if errorlevel 2 goto main_menu
call :restore_backup
echo.
echo   Готово. Нажмите любую клавишу...
pause >nul
goto main_menu

:: ============================================================
:: ВЫХОД
:: ============================================================
:do_exit
echo.
echo   До свидания!
timeout /t 2 >nul
exit /b 0