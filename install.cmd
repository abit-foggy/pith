@echo off
REM install.cmd - installer for The Pith Programming Language (Command Prompt).
REM
REM Downloads the latest release from GitHub and installs it under a
REM prefix (default: %LOCALAPPDATA%\pith).
REM
REM   curl -fsSL https://raw.githubusercontent.com/abit-foggy/pith/main/install.cmd -o install.cmd && install.cmd
REM
REM Overrides:
REM   set PREFIX=C:\pith
REM   set PITH_TAG=v0.1.0

setlocal enabledelayedexpansion

set "REPO=abit-foggy/pith"
set "PREFIX=%LOCALAPPDATA%\pith"
set "TAG="

if defined PITH_REPO set "REPO=%PITH_REPO%"
if defined PREFIX_OVERRIDE set "PREFIX=%PREFIX_OVERRIDE%"
if defined PITH_TAG set "TAG=%PITH_TAG%"

echo install.cmd: resolving the latest release of %REPO%

set "APIURL=https://api.github.com/repos/%REPO%/releases/latest"
set "TMPTAG="
for /f "delims=" %%i in ('curl -fsSL -H "User-Agent: pith-installer" "%APIURL%" 2^>nul ^| findstr /C:"\"tag_name\""') do (
    set "LINE=%%i"
    set "LINE=!LINE:*\"tag_name\": \"=!"
    set "TMPTAG=!LINE:\",=!"
    set "TMPTAG=!TMPTAG:\"=!"
)
if defined PITH_TAG set "TMPTAG=%PITH_TAG%"
if not defined TMPTAG (
    echo install.cmd: error: no releases found for %REPO% 1>&2
    echo install.cmd: build from source: https://github.com/%REPO% 1>&2
    exit /b 1
)
set "TAG=%TMPTAG%"

echo install.cmd: installing %TAG%

set "ARCH=x86_64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH=aarch64"

set "TARBALL=pith-%ARCH%-windows.tar.gz"
set "URL=https://github.com/%REPO%/releases/download/%TAG%/%TARBALL%"

set "TMPDIR=%TEMP%\pith-install-%RANDOM%"
mkdir "%TMPDIR%" 2>nul

echo install.cmd: downloading %TARBALL%
set "AUTH_HEADER="
if defined GITHUB_TOKEN set "AUTH_HEADER=-H "Authorization: Bearer %GITHUB_TOKEN%""
if defined GH_TOKEN if not defined AUTH_HEADER set "AUTH_HEADER=-H "Authorization: Bearer %GH_TOKEN%""

curl -fsSL -H "User-Agent: pith-installer" %AUTH_HEADER% "%URL%" -o "%TMPDIR%\%TARBALL%" 2>nul
if errorlevel 1 (
    set "FALLBACK_URL=https://github.com/%REPO%/releases/download/%TAG%/pith-%ARCH%-windows-nightly.tar.gz"
    echo install.cmd: trying pith-%ARCH%-windows-nightly.tar.gz
    curl -fsSL -H "User-Agent: pith-installer" %AUTH_HEADER% "!FALLBACK_URL!" -o "%TMPDIR%\%TARBALL%" 2>nul
    if errorlevel 1 (
        where gh >nul 2>nul
        if not errorlevel 1 (
            echo install.cmd: trying gh release download
            gh release download %TAG% -R %REPO% -p %TARBALL% -O "%TMPDIR%\%TARBALL%" >nul 2>nul
            if errorlevel 1 (
                gh release download %TAG% -R %REPO% -p pith-%ARCH%-windows-nightly.tar.gz -O "%TMPDIR%\%TARBALL%" >nul 2>nul
            )
        )
    )
)

if not exist "%TMPDIR%\%TARBALL%" (
    echo install.cmd: error: download failed: %URL% 1>&2
    rd /s /q "%TMPDIR%" 2>nul
    exit /b 1
)

echo install.cmd: extracting
tar -xzf "%TMPDIR%\%TARBALL%" -C "%TMPDIR%"

if exist "%TMPDIR%\pith.exe" (
    set "BIN=%TMPDIR%\pith.exe"
) else if exist "%TMPDIR%\pith" (
    set "BIN=%TMPDIR%\pith"
) else (
    echo install.cmd: error: archive is missing the pith binary 1>&2
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
    echo install.cmd: NOTE: %PREFIX%\bin is not in your PATH, add it:
    echo   setx PATH "%%PATH%%;%PREFIX%\bin"
)

"%PREFIX%\bin\pith.exe" version
echo install.cmd: installed pith to %PREFIX%\bin\pith.exe
echo install.cmd: next: pith run yourscript.pi

rd /s /q "%TMPDIR%" 2>nul
endlocal
