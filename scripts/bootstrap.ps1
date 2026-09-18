#Requires -Version 5.1
<#
.SYNOPSIS
    First-time setup for OpenVR-Emulation-Driver.

.DESCRIPTION
    Creates a Python virtual environment, installs Conan, fetches GTest
    (the only Conan-managed dependency), and configures the CMake Debug build.
    OpenVR is provided via a git submodule (libraries/openvr).

    Run once after cloning, and again whenever conanfile.py changes.

.EXAMPLE
    .\scripts\bootstrap.ps1
    .\scripts\bootstrap.ps1 -Release   # also configure the Release CMake preset
#>

param(
    [switch]$Release
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step([string]$msg) {
    Write-Host "`n==> $msg" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# Isolate Conan from the user's global ~/.conan2 config.
# Setting CONAN_HOME makes Conan use a project-local cache, so company-specific
# remotes or backup URLs in the user's global.conf don't affect contributors.
# ---------------------------------------------------------------------------
$originalConanHome = $env:CONAN_HOME  # may be $null if not set
$repoRoot = Split-Path $PSScriptRoot -Parent
$env:CONAN_HOME = Join-Path $repoRoot ".conan2"
Write-Host "CONAN_HOME set to: $env:CONAN_HOME" -ForegroundColor DarkGray

# Run everything from the repo root so `conan install .` and `cmake --preset`
# resolve conanfile.py and CMakePresets.json correctly.
Push-Location $repoRoot

# ---------------------------------------------------------------------------
# 0. Clean build directory
# ---------------------------------------------------------------------------
$buildDir = Join-Path $repoRoot "build"
$slnFile  = Join-Path $buildDir "OpenVR_Emulation_Driver.sln"
if (Test-Path $buildDir) {
    Write-Step "Removing old build directory..."
    # Close any process (e.g. Visual Studio) that has the .sln open so that
    # Remove-Item does not fail with "file in use".
    if (Test-Path $slnFile) {
        $slnNorm = (Resolve-Path $slnFile).Path.ToLower()
        Get-Process | Where-Object { $_.MainWindowHandle -ne 0 } | ForEach-Object {
            try {
                $proc = $_
                $proc.Modules | Where-Object {
                    $_.FileName -and $_.FileName.ToLower() -eq $slnNorm
                } | ForEach-Object {
                    Write-Host "  Closing $($proc.Name) (PID $($proc.Id)) which has the .sln open..." -ForegroundColor Yellow
                    $proc.CloseMainWindow() | Out-Null
                    $proc.WaitForExit(5000) | Out-Null
                    if (-not $proc.HasExited) { $proc.Kill() }
                }
            } catch { }
        }
        # Simpler fallback: close devenv.exe / devenv.com by window title
        Get-Process -Name "devenv" -ErrorAction SilentlyContinue | Where-Object {
            $_.MainWindowTitle -match [regex]::Escape((Split-Path $slnFile -Leaf))
        } | ForEach-Object {
            Write-Host "  Closing Visual Studio (PID $($_.Id))..." -ForegroundColor Yellow
            $_.CloseMainWindow() | Out-Null
            $_.WaitForExit(5000) | Out-Null
            if (-not $_.HasExited) { $_.Kill() }
        }
    }
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Path $buildDir | Out-Null
Write-Host "  Created fresh build directory." -ForegroundColor DarkGray

# ---------------------------------------------------------------------------
# 1. Require Python
# ---------------------------------------------------------------------------
Write-Step "Checking Python..."
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Error "Python 3 not found on PATH. Install it from https://python.org and re-run."
}
python --version

# ---------------------------------------------------------------------------
# 2. Create virtual environment
# ---------------------------------------------------------------------------
$venvDir = Join-Path $repoRoot ".venv"
if (-not (Test-Path "$venvDir\Scripts\Activate.ps1")) {
    Write-Step "Creating virtual environment at .venv..."
    python -m venv $venvDir
} else {
    Write-Step "Virtual environment already exists — skipping creation."
}

# ---------------------------------------------------------------------------
# 3. Activate venv
# ---------------------------------------------------------------------------
Write-Step "Activating virtual environment..."
& "$venvDir\Scripts\Activate.ps1"

# ---------------------------------------------------------------------------
# 4a. Install / upgrade Conan
# ---------------------------------------------------------------------------
Write-Step "Installing/upgrading Conan..."
pip install --upgrade conan

# ---------------------------------------------------------------------------
# 4b. Install clang-format and clang-tidy
# ---------------------------------------------------------------------------
Write-Step "Installing/upgrading clang-format and clang-tidy..."
pip install --upgrade clang-format clang-tidy

# ---------------------------------------------------------------------------
# 5. Ensure a default Conan profile exists
# ---------------------------------------------------------------------------
Write-Step "Checking Conan default profile..."
$profileCheck = conan profile show 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "No default profile found — detecting system settings..." -ForegroundColor Yellow
    conan profile detect
}

# ---------------------------------------------------------------------------
# 5a. Detect Visual Studio: require VS 2022 or VS 2019.
# ---------------------------------------------------------------------------
Write-Step "Detecting Visual Studio version..."
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Please install Visual Studio 2022 or 2019 and re-run."
}

$vs22 = & $vswhere -version "[17,18)" -latest -property installationVersion 2>$null
$vs19 = & $vswhere -version "[16,17)" -latest -property installationVersion 2>$null

if ($vs22) {
    $conanVersion = "194"
    Write-Host "  VS 2022 found ($vs22) — compiler.version=194" -ForegroundColor Green
} elseif ($vs19) {
    $conanVersion = "193"
    Write-Host "  VS 2019 found ($vs19) — compiler.version=193" -ForegroundColor Green
} else {
    Write-Error "Visual Studio 2022 or 2019 is required. Please install one and re-run."
}

$debugPreset   = "windows-msvc-debug"
$releasePreset = "windows-msvc-release"

$profilePath = Join-Path $env:CONAN_HOME "profiles\default"
$profileContent = Get-Content $profilePath -Raw
$profileDirty = $false

if ($profileContent -notmatch "compiler\.version=$conanVersion") {
    Write-Host "  Patching Conan profile: compiler.version → $conanVersion" -ForegroundColor Yellow
    $profileContent = $profileContent -replace 'compiler\.version=\d+', "compiler.version=$conanVersion"
    $profileDirty = $true
} else {
    Write-Host "  Conan profile compiler.version=$conanVersion — OK" -ForegroundColor Green
}

# ConanCenter ships gtest/1.17.0 binaries with cppstd=17.
if ($profileContent -notmatch "compiler\.cppstd=17") {
    Write-Host "  Patching Conan profile: compiler.cppstd → 17" -ForegroundColor Yellow
    $profileContent = $profileContent -replace 'compiler\.cppstd=\S+', "compiler.cppstd=17"
    $profileDirty = $true
} else {
    Write-Host "  Conan profile compiler.cppstd=17 — OK" -ForegroundColor Green
}

if ($profileDirty) {
    Set-Content $profilePath $profileContent
}


# ---------------------------------------------------------------------------
# 6. conan install
# ---------------------------------------------------------------------------
# Ensure the ConanCenter remote is registered in the project-local cache.
Write-Step "Configuring Conan remote..."
$remotes = conan remote list 2>&1
if ($remotes -notmatch "conancenter") {
    Write-Host "  Adding ConanCenter remote..." -ForegroundColor Yellow
    conan remote add conancenter https://center2.conan.io
} else {
    Write-Host "  ConanCenter remote already configured — OK" -ForegroundColor Green
}

# ConanCenter only publishes Release binaries. A single Release install is
# sufficient because Visual Studio is a multi-config generator: it handles
# Debug/Release for your own project code internally without needing separate
# Debug-built dependency libraries.
# --build=never: always use the pre-built ConanCenter binary; fail fast if missing.
Write-Step "Fetching dependencies (Release)..."
conan install . --build=never -s build_type=Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "conan install failed. Check the output above for details."
}

# ---------------------------------------------------------------------------
# 7. cmake --preset
# ---------------------------------------------------------------------------
Write-Step "Configuring CMake (Debug) using preset: $debugPreset..."
cmake --preset $debugPreset

if ($Release) {
    Write-Step "Configuring CMake (Release) using preset: $releasePreset..."
    cmake --preset $releasePreset
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

# Restore CONAN_HOME to whatever it was before bootstrap ran
$env:CONAN_HOME = $originalConanHome
Pop-Location
if ($originalConanHome) {
    Write-Host "CONAN_HOME restored to: $env:CONAN_HOME" -ForegroundColor DarkGray
} else {
    Write-Host "CONAN_HOME cleared (was not set before bootstrap)." -ForegroundColor DarkGray
}

Write-Host "`n==> Bootstrap complete!" -ForegroundColor Green

$sln = Join-Path $repoRoot "build\OpenVR_Emulation_Driver.sln"
Write-Host "    Opening $sln ..." -ForegroundColor Green
Start-Process $sln
