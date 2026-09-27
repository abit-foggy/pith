@echo off
REM install-nightly.cmd - installer for The Pith Programming Language (Command Prompt).
REM
REM Downloads the nightly build from GitHub and installs it under a
REM prefix (default: %LOCALAPPDATA%\pith).
REM
REM   curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install-nightly.cmd -o install-nightly.cmd && install-nightly.cmd
REM
REM Overrides:
REM   set PREFIX=C:\pith
REM   set PITH_REPO=owner/pith
REM   set PITH_TAG=nightly

setlocal enabledelayedexpansion

set "REPO=abit-foggy/pith"
set "PREFIX=%LOCALAPPDATA%\pith"
set "TAG=nightly"

if defined PITH_REPO set "REPO=%PITH_REPO%"
if defined PREFIX_OVERRIDE set "PREFIX=%PREFIX_OVERRIDE%"
if defined PITH_TAG set "TAG=%PITH_TAG%"

echo install-nightly.cmd: installing %TAG%

set "ARCH=x86_64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH=aarch64"

set "TARBALL=pith-%ARCH%-windows-nt-nightly.tar.gz"
set "URL=https://github.com/%REPO%/releases/download/%TAG%/%TARBALL%"

set "TMPDIR=%TEMP%\pith-install-%RANDOM%"
mkdir "%TMPDIR%" 2>nul

echo install-nightly.cmd: downloading %TARBALL%
set "AUTH_HEADER="
if defined GITHUB_TOKEN set "AUTH_HEADER=-H "Authorization: Bearer %GITHUB_TOKEN%""
if defined GH_TOKEN if not defined AUTH_HEADER set "AUTH_HEADER=-H "Authorization: Bearer %GH_TOKEN%""

curl -fsSL -H "User-Agent: pith-installer" %AUTH_HEADER% "%URL%" -o "%TMPDIR%\%TARBALL%" 2>nul
if errorlevel 1 (
    set "FALLBACK_URL=https://github.com/%REPO%/releases/download/%TAG%/pith-%ARCH%-windows-nt.tar.gz"
    echo install-nightly.cmd: trying pith-%ARCH%-windows-nt.tar.gz
    curl -fsSL -H "User-Agent: pith-installer" %AUTH_HEADER% "!FALLBACK_URL!" -o "%TMPDIR%\%TARBALL%" 2>nul
    if errorlevel 1 (
        where gh >nul 2>nul
        if not errorlevel 1 (
            echo install-nightly.cmd: trying gh release download
            gh release download %TAG% -R %REPO% -p %TARBALL% -O "%TMPDIR%\%TARBALL%" >nul 2>nul
            if errorlevel 1 (
                gh release download %TAG% -R %REPO% -p pith-%ARCH%-windows-nt.tar.gz -O "%TMPDIR%\%TARBALL%" >nul 2>nul
            )
        )
    )
)

if not exist "%TMPDIR%\%TARBALL%" (
    echo install-nightly.cmd: error: download failed: %URL% 1>&2
    rd /s /q "%TMPDIR%" 2>nul
    exit /b 1
)

echo install-nightly.cmd: extracting
tar -xzf "%TMPDIR%\%TARBALL%" -C "%TMPDIR%"

if exist "%TMPDIR%\pith.exe" (
    set "BIN=%TMPDIR%\pith.exe"
) else if exist "%TMPDIR%\pith" (
    set "BIN=%TMPDIR%\pith"
) else (
    echo install-nightly.cmd: error: archive is missing the pith binary 1>&2
    rd /s /q "%TMPDIR%" 2>nul
    exit /b 1
)

mkdir "%PREFIX%\bin" 2>nul
mkdir "%PREFIX%\lib\pith\tcc" 2>nul
mkdir "%PREFIX%\lib\pith\runtime" 2>nul
mkdir "%PREFIX%\include" 2>nul

copy /y "%BIN%" "%PREFIX%\bin\pith.exe" >nul

if exist "%TMPDIR%\libtcc1.a" copy /y "%TMPDIR%\libtcc1.a" "%PREFIX%\lib\pith\tcc\" >nul
if exist "%TMPDIR%\libruntime.a" copy /y "%TMPDIR%\libruntime.a" "%PREFIX%\lib\pith\runtime\" >nul
if exist "%TMPDIR%\include" for %%h in ("%TMPDIR%\include\*.h") do copy /y "%%h" "%PREFIX%\include\" >nul

echo %PATH% | findstr /C:"%PREFIX%\bin" >nul
if errorlevel 1 (
    echo install-nightly.cmd: NOTE: %PREFIX%\bin is not in your PATH, add it:
    echo   setx PATH "%%PATH%%;%PREFIX%\bin"
)

"%PREFIX%\bin\pith.exe" version
echo install-nightly.cmd: installed pith to %PREFIX%\bin\pith.exe
echo install-nightly.cmd: next: pith run yourscript.pi

rd /s /q "%TMPDIR%" 2>nul
endlocal
