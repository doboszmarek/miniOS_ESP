@echo off
setlocal enabledelayedexpansion

rem ===================================================================
rem  build.bat - kompiluje krok miniOS do bin\<krok>.bin
rem
rem    build 01           katalog zaczynajacy sie od "01" (01-hello)
rem    build 01-hello     to samo, pelna nazwa
rem
rem  Kompilator: tools\xtensa-esp-elf\ w repo albo xtensa-esp32s3-elf-gcc w PATH.
rem  Wynik:      bin\<krok>.bin  (+ build\<krok>\*.elf, *.map do podgladu)
rem ===================================================================

set "ROOT=%~dp0"
set "PAT=%~1"
if not defined PAT (
    echo Uzycie: build ^<krok^>    np. build 01
    exit /b 1
)

set "STEP="
for /f "delims=" %%d in ('dir /b /ad /o:n "%ROOT%%PAT%*" 2^>nul') do set "STEP=%%d"
if not defined STEP (
    echo Nie ma katalogu kroku "%PAT%*"
    exit /b 1
)

set "CC=%ROOT%tools\xtensa-esp-elf\bin\xtensa-esp32s3-elf-gcc.exe"
if not exist "%CC%" (
    where xtensa-esp32s3-elf-gcc >nul 2>nul
    if errorlevel 1 (
        echo Nie znaleziono kompilatora xtensa-esp32s3-elf-gcc.
        echo Rozpakuj toolchain do %ROOT%tools\  - instrukcja w README.md
        exit /b 1
    )
    set "CC=xtensa-esp32s3-elf-gcc"
)

set "SRC=%ROOT%%STEP%"
set "OUT=%ROOT%build\%STEP%"
if not exist "%OUT%" mkdir "%OUT%"

set "FILES="
for %%f in ("%SRC%\*.c" "%SRC%\*.S") do set FILES=!FILES! "%%f"

echo.
echo   Krok:  %STEP%
echo   Pliki:%FILES%
echo.

"%CC%" -Os -g -Wall -Wextra -ffreestanding -nostdlib -mlongcalls ^
    -T "%SRC%\link.ld" -Wl,-Map="%OUT%\%STEP%.map" -Wl,--gc-sections ^
    -o "%OUT%\%STEP%.elf" %FILES%
if errorlevel 1 (
    echo.
    echo Kompilacja nie powiodla sie.
    exit /b 1
)

python -m esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 4MB ^
    -o "%ROOT%bin\%STEP%.bin" "%OUT%\%STEP%.elf"
if errorlevel 1 exit /b 1

echo.
echo Gotowe: bin\%STEP%.bin    Teraz:  flash %PAT%
