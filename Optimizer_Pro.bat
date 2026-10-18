@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title Оптимизатор Pro (Batch Edition)

:: --- АВТОЗАПРОС ПРАВ АДМИНИСТРАТОРА ---
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if '%errorlevel%' NEQ '0' (
    echo Запрос прав администратора...
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
    pushd "%CD%"
    CD /D "%~dp0"

:: --- ИНИЦИАЛИЗАЦИЯ ЛОГА ---
set "L=%~dp0optimization_log.txt"
if not exist "%L%" (
    powershell -NoProfile -Command "'' | Out-File -FilePath '%L%' -Encoding Unicode"
)
cmd /u /c echo === ЛОГ - %DATE% %TIME% === >> "%L%"

:MENU
mode con: cols=70 lines=28
cls & color 0B
echo.
echo.
echo           ┌──────────────────────────────────────────┐
echo           │       УНИВЕРСАЛЬНАЯ ОПТИМИЗАЦИЯ          │
echo           ├──────────────────────────────────────────┤
echo           │                                          │
echo           │  1. Полная оптимизация (Все этапы)       │
echo           │                                          │
echo           │  2. Выборочная оптимизация               │
echo           │                                          │
echo           │  3. Проверка системы (SFC / DISM)        │
echo           │                                          │
echo           │  4. Открыть лог (Журнал)                 │
echo           │                                          │
echo           │  5. Откатить изменения (Восстановить)    │
echo           │                                          │
echo           │  6. Выход                                │
echo           │                                          │
echo           └──────────────────────────────────────────┘
echo.
set /p "C=  Выбор (1-6): "

if "%C%"=="1" (
    for /l %%i in (1,1,7) do set "o%%i=Y"
    goto EX
)
if "%C%"=="2" goto CU
if "%C%"=="3" goto SYS_ONLY
if "%C%"=="4" goto VL
if "%C%"=="5" goto RESTORE
if "%C%"=="6" exit /b
goto MENU

:CU
cls & echo.
echo          ┌──────────────────────────────────────────┐
echo          │        ВЫБОРОЧНАЯ НАСТРОЙКА              │
echo          └──────────────────────────────────────────┘
echo  * Ввод Y/N. Пустой Enter = N
echo  ──────────────────────────────────────────
for /l %%i in (1,1,7) do set "o%%i=N"
set "in=" & set /p in=" [1/7] Точка восстановления? (Y/N): " & if /i "!in!"=="Y" set "o1=Y"
set "in=" & set /p in=" [2/7] Очистка кэша и Temp?  (Y/N): " & if /i "!in!"=="Y" set "o2=Y"
set "in=" & set /p in=" [3/7] Очистка DNS?          (Y/N): " & if /i "!in!"=="Y" set "o3=Y"
set "in=" & set /p in=" [4/7] Питание и ядра CPU?   (Y/N): " & if /i "!in!"=="Y" set "o4=Y"
set "in=" & set /p in=" [5/7] Отключить телеметрию? (Y/N): " & if /i "!in!"=="Y" set "o5=Y"
set "in=" & set /p in=" [6/7] Оптимизация ОЗУ?      (Y/N): " & if /i "!in!"=="Y" set "o6=Y"
set "in=" & set /p in=" [7/7] Проверка SFC/DISM?    (Y/N): " & if /i "!in!"=="Y" set "o7=Y"
goto EX

:SYS_ONLY
for /l %%i in (1,1,7) do set "o%%i=N"
set "o7=Y"
set "C=3"
goto EX

:EX
mode con: cols=70 lines=28
cls & color 0B
echo.
echo          ┌──────────────────────────────────────────┐
echo          │         ВЫПОЛНЕНИЕ ОПТИМИЗАЦИИ           │
echo          └──────────────────────────────────────────┘
echo.

:: БЕНЧМАРК ДО
for /f "tokens=2 delims==" %%I in ('wmic OS get FreePhysicalMemory /value 2^>nul') do set /a RAM_BEFORE=%%I/1024
echo   [i] Свободно ОЗУ до оптимизации: %RAM_BEFORE% МБ
echo.

if /i "%o1%"=="Y" (
    echo   [+] Точка восстановления...
    cmd /u /c echo  Точка восстановления >> "%L%"
    net start VSS >nul 2>&1
    reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SystemRestore" /v "SystemRestorePointCreationFrequency" /t REG_DWORD /d 0 /f >nul 2>&1
    powershell -NoProfile -Command "Enable-ComputerRestore -Drive 'C:\'; Checkpoint-Computer -Description 'Optimizer_Pro' -RestorePointType 'MODIFY_SETTINGS'" >nul 2>&1
    if !errorlevel! neq 0 (
        echo       └─ [ОШИБКА] Точка не создана!
    ) else (
        echo       └─ [OK]
    )
)

if /i "%o2%"=="Y" (
    echo   [+] Очистка Temp и кэша...
    cmd /u /c echo  Очистка Temp >> "%L%"
    del /f /q /s "%TEMP%\*" >nul 2>&1
    for /d %%p in ("%TEMP%\*") do rmdir /s /q "%%p" >nul 2>&1
    del /f /q /s "%SystemRoot%\Temp\*" >nul 2>&1
    for /d %%p in ("%SystemRoot%\Temp\*") do rmdir /s /q "%%p" >nul 2>&1
    net stop wuauserv >nul 2>&1 & net stop bits >nul 2>&1
    del /f /q /s "%SystemRoot%\SoftwareDistribution\Download\*" >nul 2>&1
    for /d %%p in ("%SystemRoot%\SoftwareDistribution\Download\*") do rmdir /s /q "%%p" >nul 2>&1
    net start bits >nul 2>&1 & net start wuauserv >nul 2>&1
    echo       └─ [OK]
)

if /i "%o3%"=="Y" (
    echo   [+] Сброс DNS...
    cmd /u /c echo  DNS >> "%L%"
    ipconfig /flushdns >nul 2>&1
    echo       └─ [OK]
)

if /i "%o4%"=="Y" (
    echo   [+] Настройка питания...
    cmd /u /c echo  Питание >> "%L%"
    powershell -NoProfile -Command "if (Get-CimInstance Win32_Battery) { exit 1 }" >nul 2>&1
    if !errorlevel! equ 1 (
        echo       └─ [ИНФО] Ноутбук. Пропущено.
    ) else (
        powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>&1
        powercfg /attributes 54533251-82bd-4e29-950d-4136c166a751 0cc5b647-c36e-417e-ba9d-d17442bc94de -ATTRIB_HIDE >nul 2>&1
        powercfg /setacvalueindex 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c 54533251-82bd-4e29-950d-4136c166a751 0cc5b647-c36e-417e-ba9d-d17442bc94de 0 >nul 2>&1
        powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>&1
        echo       └─ [OK]
    )
)

if /i "%o5%"=="Y" (
    echo   [+] Отключение телеметрии...
    cmd /u /c echo  Телеметрия >> "%L%"
    sc config DiagTrack start= disabled >nul 2>&1 & net stop DiagTrack >nul 2>&1
    sc config NvTelemetryContainer start= disabled >nul 2>&1 & net stop NvTelemetryContainer >nul 2>&1
    sc config AMDCrashDefenderService start= disabled >nul 2>&1 & net stop AMDCrashDefenderService >nul 2>&1
    reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\GameDVR" /v "AppCaptureEnabled" /t REG_DWORD /d 0 /f >nul 2>&1
    reg add "HKCU\System\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 0 /f >nul 2>&1
    reg add "HKLM\SOFTWARE\Microsoft\PolicyManager\default\ApplicationManagement\AllowGameDVR" /v "value" /t REG_DWORD /d 0 /f >nul 2>&1
    reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\GameDVR" /v "AllowGameDVR" /t REG_DWORD /d 0 /f >nul 2>&1
    echo       └─ [OK]
)

if /i "%o6%"=="Y" (
    echo   [+] Оптимизация ОЗУ (безопасная)...
    cmd /u /c echo  ОЗУ >> "%L%"
    powershell -NoProfile -EncodedCommand "JGM9J1tEbGxJbXBvcnQoInBzYXBpLmRsbCIpXXB1YmxpYyBzdGF0aWMgZXh0ZXJuIGJvb2wgRW1wdHlXb3JraW5nU2V0KEludFB0ciBoKTsnOyR0PUFkZC1UeXBlIC1NZW1iZXJEZWZpbml0aW9uICRjIC1OYW1lICdNJyAtTmFtZXNwYWNlICdXJyAtUGFzc1RocnU7R2V0LVByb2Nlc3N8P3skXy5NYWluV2luZG93SGFuZGxlfXxGb3JFYWNoLU9iamVjdHt0cnl7JHQ6OkVtcHR5V29ya2luZ1NldCgkXy5IYW5kbGUpfWNhdGNoe319" >nul 2>&1
    echo       └─ [OK]
)

if /i "%o7%"=="Y" (
    echo   [+] Запуск SFC / DISM...
    echo       [!] Это займет 15-20 минут. Не закрывайте окно.
    cmd /u /c echo  DISM/SFC >> "%L%"
    
    dism /online /cleanup-image /scanhealth > "%TEMP%\d1.tmp" 2>&1
    powershell -NoProfile -Command "Get-Content '%TEMP%\d1.tmp' -ErrorAction SilentlyContinue | Out-File '%L%' -Append -Encoding Unicode"
    
    dism /online /cleanup-image /restorehealth > "%TEMP%\d2.tmp" 2>&1
    powershell -NoProfile -Command "Get-Content '%TEMP%\d2.tmp' -ErrorAction SilentlyContinue | Out-File '%L%' -Append -Encoding Unicode"
    
    sfc /scannow > "%TEMP%\s1.tmp" 2>&1
    powershell -NoProfile -Command "Get-Content '%TEMP%\s1.tmp' -ErrorAction SilentlyContinue | Out-File '%L%' -Append -Encoding Unicode"
    
    del /f /q "%TEMP%\d1.tmp" "%TEMP%\d2.tmp" "%TEMP%\s1.tmp" >nul 2>&1
    echo       └─ [OK]
)

:: БЕНЧМАРК ПОСЛЕ
for /f "tokens=2 delims==" %%I in ('wmic OS get FreePhysicalMemory /value 2^>nul') do set /a RAM_AFTER=%%I/1024
set /a RAM_DIFF=%RAM_AFTER% - %RAM_BEFORE%

echo.
echo          ┌──────────────────────────────────────────┐
echo          │         ОПТИМИЗАЦИЯ ЗАВЕРШЕНА!           │
echo          ├──────────────────────────────────────────┤
echo          │  * Свободно ОЗУ: %RAM_AFTER% МБ (было %RAM_BEFORE%)
if %RAM_DIFF% GTR 0 (
    echo          │  * Освобождено: +%RAM_DIFF% МБ
)
echo          │  * Рекомендуется перезагрузить ПК.        │
echo          └──────────────────────────────────────────┘
echo.
cmd /u /c echo [КОНЕЦ] Успешно. ОЗУ до: %RAM_BEFORE%, после: %RAM_AFTER% >> "%L%"

if "%C%"=="3" (
    pause & goto MENU
) else (
    pause & goto MENU
)

:RESTORE
mode con: cols=70 lines=28
cls & color 0B
echo.
echo          ┌──────────────────────────────────────────┐
echo          │        ВОССТАНОВЛЕНИЕ НАСТРОЕК           │
echo          └──────────────────────────────────────────┘
echo.
echo   [!] Внимание: Эта функция вернет стандартные настройки
echo       Windows, которые были изменены оптимизатором.
echo.
set /p "ans=  Вы уверены, что хотите откатить изменения? (Y/N): "
if /i not "%ans%"=="Y" goto MENU

echo.
echo   [~] Восстановление...
cmd /u /c echo  ОТКАТ ИЗМЕНЕНИЙ >> "%L%"

sc config DiagTrack start= auto >nul 2>&1 & net start DiagTrack >nul 2>&1
sc config NvTelemetryContainer start= auto >nul 2>&1 & net start NvTelemetryContainer >nul 2>&1
sc config AMDCrashDefenderService start= auto >nul 2>&1
reg add "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\GameDVR" /v "AppCaptureEnabled" /t REG_DWORD /d 1 /f >nul 2>&1
reg add "HKCU\System\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 1 /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\GameDVR" /v "AllowGameDVR" /f >nul 2>&1
powercfg -restoredefaultschemes >nul 2>&1

echo.
echo   [УСПЕШНО] Стандартные настройки восстановлены.
pause
goto MENU

:VL
if exist "%L%" (
    start "" "%L%"
) else (
    echo  [!] Журнал работы еще не создан.
    pause
)
goto MENU
