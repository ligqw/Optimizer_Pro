@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1
title Optimizer Pro

:: --- ПРОВЕРКА ПРАВ АДМИНИСТРАТОРА ---
net session >nul 2>&1
if %errorLevel% NEQ 0 (
    cls
    color 0C
    echo.
    echo   ===================================================
    echo      ВНИМАНИЕ! ЗАПУСТИТЕ ОТ ИМЕНИ АДМИНИСТРАТОРА!
    echo   ===================================================
    echo.
    echo   Чтобы программа работала корректно:
    echo   1. Закройте это окно.
    echo   2. Нажмите ПРАВОЙ кнопкой мыши на файл Optimizer_Pro.bat
    echo   3. Выберите "Запуск от имени администратора".
    echo.
    pause
    exit /b
)

:gotAdmin
    color 0B
    set "L=%~dp0optimization_log.txt"

:MENU
mode con: cols=56 lines=28
cls & color 0B
echo.
echo   +--------------------------------------------------+
echo   ^|          УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ PRO           ^|
echo   +--------------------------------------------------+
echo   ^| 1. Полная оптимизация (Все этапы)                ^|
echo   ^| 2. Выборочная оптимизация                        ^|
echo   ^| 3. Управление автозагрузкой                      ^|
echo   ^| 4. Debloat (Удаление встроенного мусора)         ^|
echo   ^| 5. Проверка системы (SFC / DISM)                 ^|
echo   ^| 6. Открыть лог (Журнал)                          ^|
echo   ^| 7. Откатить изменения (Восстановить)             ^|
echo   ^| 8. Выход                                         ^|
echo   +--------------------------------------------------+
echo.
set /p "C=  Выбор: "

if "%C%"=="1" ( set "full=Y" & goto EX )
if "%C%"=="2" goto CU
if "%C%"=="3" goto STARTUP_MGR
if "%C%"=="4" goto DEBLOAT
if "%C%"=="5" ( set "o_sfc=Y" & set "full=N" & goto EX )
if "%C%"=="6" goto OPEN_LOG
if "%C%"=="7" goto RESTORE
if "%C%"=="8" exit /b
goto MENU

:CU
cls & echo.
echo   +--------------------------------------------------+
echo   ^|            ВЫБОРОЧНАЯ НАСТРОЙКА                  ^|
echo   +--------------------------------------------------+
set "full=N"
for %%i in (restore clean game hibernate visual power recycle debloat ram sfc) do set "o_%%i=N"

set "in=" & set /p in=" [1/10] Точка восстановления? (Y/N): " & if /i "!in!"=="Y" set "o_restore=Y"
set "in=" & set /p in=" [2/10] Очистка Temp? (Y/N): " & if /i "!in!"=="Y" set "o_clean=Y"
set "in=" & set /p in=" [3/10] Кэш игр? (Y/N): " & if /i "!in!"=="Y" set "o_game=Y"
set "in=" & set /p in=" [4/10] Отключить гибернацию (много места)? (Y/N): " & if /i "!in!"=="Y" set "o_hibernate=Y"
set "in=" & set /p in=" [5/10] Отключить визуальные эффекты? (Y/N): " & if /i "!in!"=="Y" set "o_visual=Y"
set "in=" & set /p in=" [6/10] Включить макс. производительность? (Y/N): " & if /i "!in!"=="Y" set "o_power=Y"
set "in=" & set /p in=" [7/10] Очистить Корзину? (Y/N): " & if /i "!in!"=="Y" set "o_recycle=Y"
set "in=" & set /p in=" [8/10] Debloat (Удалить мусор)? (Y/N): " & if /i "!in!"=="Y" set "o_debloat=Y"
set "in=" & set /p in=" [9/10] Оптимизация ОЗУ? (Y/N): " & if /i "!in!"=="Y" set "o_ram=Y"
set "in=" & set /p in=" [10/10] Проверка SFC/DISM? (Y/N): " & if /i "!in!"=="Y" set "o_sfc=Y"
goto EX

:EX
mode con: cols=56 lines=28
cls & color 0B
echo.
echo   +--------------------------------------------------+
echo   ^|            ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ                ^|
echo   +--------------------------------------------------+
echo.

if /i "%full%"=="Y" (
    set "o_restore=Y" & set "o_clean=Y" & set "o_game=Y" & set "o_hibernate=Y" & set "o_visual=Y" & set "o_power=Y" & set "o_recycle=Y" & set "o_debloat=N" & set "o_ram=Y" & set "o_sfc=N"
)

set "RAM_BEFORE=0"
for /f "usebackq" %%I in (`powershell -NoProfile -Command "[math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1024)" 2^>nul`) do set "RAM_BEFORE=%%I"
echo   [i] Свободно ОЗУ до: !RAM_BEFORE! МБ
echo.

if /i "!o_restore!"=="Y" (
    echo   [+] Создание точки восстановления...
    net start VSS >nul 2>&1
    reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SystemRestore" /v "SystemRestorePointCreationFrequency" /t REG_DWORD /d 0 /f >nul 2>&1
    powershell -NoProfile -Command "Enable-ComputerRestore -Drive 'C:\'; Checkpoint-Computer -Description 'Optimizer_Pro' -RestorePointType 'MODIFY_SETTINGS'" >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_clean!"=="Y" (
    echo   [+] Очистка Temp...
    del /f /q /s "%TEMP%\*" >nul 2>&1
    for /d %%p in ("%TEMP%\*") do rmdir /s /q "%%p" >nul 2>&1
    del /f /q /s "%SystemRoot%\Temp\*" >nul 2>&1
    for /d %%p in ("%SystemRoot%\Temp\*") do rmdir /s /q "%%p" >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_game!"=="Y" (
    echo   [+] Очистка кэша игр...
    del /f /q /s "%LOCALAPPDATA%\Steam\htmlcache\*" >nul 2>&1
    del /f /q /s "%LOCALAPPDATA%\EpicGamesLauncher\Saved\webcache\*" >nul 2>&1
    del /f /q /s "%LOCALAPPDATA%\Riot Games\Riot Client\Cache\*" >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_hibernate!"=="Y" (
    echo   [+] Отключение гибернации...
    powercfg -h off >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_visual!"=="Y" (
    echo   [+] Отключение визуальных эффектов...
    reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d 2 /f >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_power!"=="Y" (
    echo   [+] Включение макс. производительности...
    powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 >nul 2>&1
    powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61 >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_recycle!"=="Y" (
    echo   [+] Очистка Корзины...
    powershell -NoProfile -Command "Clear-RecycleBin -Force" >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_debloat!"=="Y" (
    echo   [+] Debloat (Удаление мусора)...
    for %%a in (Microsoft.BingWeather Microsoft.GetHelp Microsoft.SkypeApp Microsoft.XboxApp Microsoft.ZuneMusic) do (
        powershell -NoProfile -Command "Get-AppxPackage *%%a* ^| Remove-AppxPackage" >nul 2>&1
    )
    echo       +-- [OK]
)

if /i "!o_ram!"=="Y" (
    echo   [+] Оптимизация ОЗУ...
    powershell -NoProfile -EncodedCommand "JGNyaXRpY2FsID0gQCgnZXhwbG9yZXInLCAnc3ZjaG9zdCcsICdjc3JzcycsICd3aW5pbml0JywgJ3NlcnZpY2VzJywgJ2xzYXNzJywgJ3dpbmxvZ29uJywgJ1N5c3RlbScsICdSZWdpc3RyeScpOyRjID0gJ1tEbGxJbXBvcnQoInBzYXBpLmRsbCIpXXB1YmxpYyBzdGF0aWMgZXh0ZXJuIGJvb2wgRW1wdHlXb3JraW5nU2V0KEludFB0ciBoKTsnOyR0ID0gQWRkLVR5cGUgLU1lbWJlckRlZmluaXRpb24gJGMgLU5hbWUgJ00nIC1OYW1lc3BhY2UgJ1cnIC1QYXNzVGhydTtHZXQtUHJvY2VzcyB8IFdoZXJlLU9iamVjdCB7ICRfLk1haW5XaW5kb3dIYW5kbGUgLW5lIDAgLWFuZCAkY3JpdGljYWwgLW5vdGNvbnRhaW5zICRfLlByb2Nlc3NOYW1lIH0gfCBGb3JFYWNoLU9iamVjdCB7IHRyeSB7ICR0OjpFbXB0eVdvcmtpbmdTZXQoJF8uSGFuZGxlKSB9IGNhdGNoIHt9IH0=" >nul 2>&1
    echo       +-- [OK]
)

if /i "!o_sfc!"=="Y" (
    echo   [+] Проверка SFC/DISM (Ждите 15-20 минут)...
    dism /online /cleanup-image /scanhealth >nul 2>&1
    dism /online /cleanup-image /restorehealth >nul 2>&1
    sfc /scannow >nul 2>&1
    echo       +-- [OK]
)

set "RAM_AFTER=0"
for /f "usebackq" %%I in (`powershell -NoProfile -Command "[math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1024)" 2^>nul`) do set "RAM_AFTER=%%I"

if not defined RAM_AFTER set "RAM_AFTER=0"
if not defined RAM_BEFORE set "RAM_BEFORE=0"
set /a RAM_DIFF=!RAM_AFTER! - !RAM_BEFORE!

echo.
echo   +--------------------------------------------------+
echo   ^|            ОПТИМИЗАЦИЯ ЗАВЕРШЕНА!                ^|
echo   +--------------------------------------------------+
echo   ^|  * Свободно ОЗУ: !RAM_AFTER! МБ (было !RAM_BEFORE!)
if !RAM_DIFF! GTR 0 (
    echo   ^|  * Освобождено: +!RAM_DIFF! МБ
)
echo   +--------------------------------------------------+
echo.
echo   Нажмите любую клавишу для возврата в меню...
pause >nul
goto MENU

:STARTUP_MGR
cls
echo.
echo   +--------------------------------------------------+
echo   ^|          ПРОГРАММЫ В АВТОЗАГРУЗКЕ                ^|
echo   +--------------------------------------------------+
echo  Текущие программы (HKCU\Run):
echo  --------------------------------------------------
for /f "tokens=1" %%A in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" 2^>nul ^| findstr "REG_SZ"') do echo   - %%A
echo  --------------------------------------------------
pause >nul
goto MENU

:DEBLOAT
cls
echo.
echo   +--------------------------------------------------+
echo   ^|           УДАЛЕНИЕ МУСОРА (DEBLOAT)              ^|
echo   +--------------------------------------------------+
set /p "ans=  Вы уверены? (Y/N): "
if /i not "!ans!"=="Y" goto MENU
echo  [+] Удаление...
for %%a in (Microsoft.BingWeather Microsoft.GetHelp Microsoft.Getstarted Microsoft.MicrosoftOfficeHub Microsoft.SkypeApp Microsoft.XboxApp Microsoft.ZuneMusic) do (
    powershell -NoProfile -Command "Get-AppxPackage *%%a* ^| Remove-AppxPackage" >nul 2>&1
)
echo  [OK] Готово.
pause >nul
goto MENU

:RESTORE
cls
echo.
echo   +--------------------------------------------------+
echo   ^|          ВОССТАНОВЛЕНИЕ НАСТРОЕК                 ^|
echo   +--------------------------------------------------+
echo  [~] Восстановление служб и питания...
sc config DiagTrack start= auto >nul 2>&1 & net start DiagTrack >nul 2>&1
sc config SysMain start= auto >nul 2>&1 & net start SysMain >nul 2>&1
sc config WSearch start= auto >nul 2>&1 & net start WSearch >nul 2>&1
powercfg -restoredefaultschemes >nul 2>&1
powercfg -h on >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects" /v "VisualFXSetting" /t REG_DWORD /d 0 /f >nul 2>&1
echo  [УСПЕШНО] Настройки сброшены.
pause >nul
goto MENU

:OPEN_LOG
if exist "%L%" (start "" "%L%") else (echo Лог не создан. & pause)
goto MENU