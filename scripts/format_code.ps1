#Requires -Version 5.1
<#
.SYNOPSIS
    Run clang-format over all driver source files.

.DESCRIPTION
    Formats every .hpp and .cpp under driver_files/src/ and tests/ in-place.
    Exits with code 1 and reports the changed files if any formatting
    was required (useful in CI).

.EXAMPLE
    .\scripts\format_code.ps1
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = git rev-parse --show-toplevel
Push-Location $projectRoot

try {
    $searchPaths = @("driver_files\src", "tests") | Where-Object { Test-Path $_ }
    $files = Get-ChildItem -Path $searchPaths -Recurse -Include "*.hpp","*.cpp" |
             Select-Object -ExpandProperty FullName

    if (-not $files) {
        Write-Host "No source files found." -ForegroundColor Yellow
        exit 0
    }

    Write-Host "Running clang-format on $($files.Count) file(s)..." -ForegroundColor Cyan
    & clang-format -i @files

    $changed = git diff --name-only -- "*.hpp" "*.cpp"
    if ($changed) {
        Write-Host "Formatting issues found — the following files were reformatted:" -ForegroundColor Yellow
        $changed | ForEach-Object { Write-Host "  $_" }
        Write-Host "Re-stage the modified files before committing." -ForegroundColor Yellow
        exit 1
    } else {
        Write-Host "Formatting OK — no changes required." -ForegroundColor Green
    }
} finally {
    Pop-Location
}
