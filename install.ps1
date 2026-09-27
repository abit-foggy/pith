# install.ps1 - installer for The Pith Programming Language (PowerShell).
#
# Downloads the latest release from GitHub and installs it under a
# prefix (default: %LOCALAPPDATA%\pith).
#
#   iwr https://raw.githubusercontent.com/abit-foggy/pith/main/install.ps1 -UseBasicParsing | iex
#
# Overrides:
#   $env:PREFIX = "C:\pith"; iwr ... | iex
#   $env:PITH_TAG = "v0.1.0"; iwr ... | iex

$ErrorActionPreference = "Stop"

$Repo = if ($env:PITH_REPO) { $env:PITH_REPO } else { "abit-foggy/pith" }
$Prefix = if ($env:PREFIX) { $env:PREFIX } else { "$env:LOCALAPPDATA\pith" }

Write-Host "install.ps1: resolving the latest release of $Repo"

$Headers = @{ "User-Agent" = "pith-installer" }
$Token = if ($env:GITHUB_TOKEN) { $env:GITHUB_TOKEN } elseif ($env:GH_TOKEN) { $env:GH_TOKEN } else { "" }
if ($Token) {
    $Headers["Authorization"] = "Bearer $Token"
}
$Rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -Headers $Headers
$Tag = if ($env:PITH_TAG) { $env:PITH_TAG } else { $Rel.tag_name }

if (-not $Tag) {
    Write-Host "install.ps1: error: no releases found for $Repo (build from source instead)" -ForegroundColor Red
    exit 1
}

Write-Host "install.ps1: installing $Tag"

$Arch = if ([Environment]::Is64BitOperatingSystem) {
    if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "aarch64" } else { "x86_64" }
} else { "i386" }
$Tarball = "pith-$Arch-windows.tar.gz"
$Url = "https://github.com/$Repo/releases/download/$Tag/$Tarball"
$FallbackTarball = "pith-$Arch-windows-nightly.tar.gz"
$FallbackUrl = "https://github.com/$Repo/releases/download/$Tag/$FallbackTarball"

$Tmp = Join-Path ([System.IO.Path]::GetTempPath()) "pith-install-$(Get-Random)"
New-Item -ItemType Directory -Path $Tmp -Force | Out-Null

try {
    Write-Host "install.ps1: downloading $Tarball"
    $Downloaded = $false
    try {
        Invoke-WebRequest -Uri $Url -OutFile (Join-Path $Tmp $Tarball) -Headers $Headers
        $Downloaded = $true
    } catch {
        # Fallback to nightly suffixed asset name
    }

    if (-not $Downloaded) {
        try {
            Write-Host "install.ps1: trying $FallbackTarball"
            Invoke-WebRequest -Uri $FallbackUrl -OutFile (Join-Path $Tmp $Tarball) -Headers $Headers
            $Downloaded = $true
        } catch {
            # Fallback to gh cli if available
        }
    }

    if (-not $Downloaded -and (Get-Command gh -ErrorAction SilentlyContinue)) {
        Write-Host "install.ps1: trying gh release download"
        gh release download $Tag -R $Repo -p $Tarball -O (Join-Path $Tmp $Tarball)
        if ($LASTEXITCODE -ne 0) {
            gh release download $Tag -R $Repo -p $FallbackTarball -O (Join-Path $Tmp $Tarball)
        }
        if (Test-Path (Join-Path $Tmp $Tarball)) {
            $Downloaded = $true
        }
    }

    if (-not $Downloaded) {
        Write-Host "install.ps1: error: failed to download release asset from $Url" -ForegroundColor Red
        exit 1
    }

    Write-Host "install.ps1: extracting"
    tar -xzf (Join-Path $Tmp $Tarball) -C $Tmp

    $Bin = Join-Path $Tmp "pith.exe"
    if (-not (Test-Path $Bin)) {
        $Bin = Join-Path $Tmp "pith"
    }
    if (-not (Test-Path $Bin)) {
        Write-Host "install.ps1: error: archive is missing the pith binary" -ForegroundColor Red
        exit 1
    }

    New-Item -ItemType Directory -Path "$Prefix\bin" -Force | Out-Null
    New-Item -ItemType Directory -Path "$Prefix\lib\pith\tcc" -Force | Out-Null
    New-Item -ItemType Directory -Path "$Prefix\lib\pith\runtime" -Force | Out-Null
    New-Item -ItemType Directory -Path "$Prefix\include" -Force | Out-Null

    Copy-Item $Bin "$Prefix\bin\pith.exe" -Force

    if (Test-Path (Join-Path $Tmp "libtcc1.a")) {
        Copy-Item (Join-Path $Tmp "libtcc1.a") "$Prefix\lib\pith\tcc\" -Force
    }
    if (Test-Path (Join-Path $Tmp "libruntime.a")) {
        Copy-Item (Join-Path $Tmp "libruntime.a") "$Prefix\lib\pith\runtime\" -Force
    }
    Get-ChildItem (Join-Path $Tmp "include") -Filter *.h -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item $_.FullName "$Prefix\include\" -Force }

    if ($env:PATH -notlike "*$Prefix\bin*") {
        Write-Host "install.ps1: NOTE: $Prefix\bin is not in your PATH, add it:"
        Write-Host '  [Environment]::SetEnvironmentVariable("Path", $env:Path + ";'$Prefix'\bin", "User")'
    }

    & "$Prefix\bin\pith.exe" version
    Write-Host "install.ps1: installed pith to $Prefix\bin\pith.exe"
    Write-Host "install.ps1: next: pith run yourscript.pi"
} finally {
    Remove-Item -Recurse -Force $Tmp -ErrorAction SilentlyContinue
}
