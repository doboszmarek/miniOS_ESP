@echo off
setlocal

rem ===================================================================
rem  flash.bat - wgrywa obraz miniOS na ESP32-S3 i otwiera monitor
rem
rem    flash              najnowszy obraz z bin\ (ostatni alfabetycznie), port COM6
rem    flash 01           obraz zaczynajacy sie od "01" (np. bin\01-hello.bin)
rem    flash 01 COM7      to samo, inny port
rem    flash - COM7       najnowszy obraz, inny port
rem
rem  Domyslny port na stale:  setx MINIOS_PORT COM7   (nowa konsola)
rem  Wyjscie z monitora: Ctrl+]
rem ===================================================================

set "BIN=%~dp0bin"
set "PAT=%~1"
if "%PAT%"=="-" set "PAT="

set "PORT=%~2"
if not defined PORT set "PORT=%MINIOS_PORT%"
if not defined PORT set "PORT=COM6"

set "IMG="
for /f "delims=" %%f in ('dir /b /o:n "%BIN%\%PAT%*.bin" 2^>nul') do set "IMG=%BIN%\%%f"

if not defined IMG (
    echo Nie znaleziono obrazu "%PAT%*.bin" w %BIN%
    echo Zrob "git pull" albo sprawdz nazwe kroku.
    exit /b 1
)

echo.
echo   Obraz: %IMG%
echo   Port:  %PORT%
echo.

python -m esptool --chip esp32s3 -p %PORT% -b 460800 write-flash 0x0 "%IMG%"
if errorlevel 1 (
    echo.
    echo Flashowanie nie powiodlo sie. Sprawdz port: python -m serial.tools.list_ports
    exit /b 1
)

rem Po resecie natywny port USB znika na chwile - czekamy, az wroci.
timeout /t 2 /nobreak >nul

python -m serial.tools.miniterm %PORT% 115200
