# =============================================================================
#  fx-CG50 mini-games one-click build script
#  Requires: PrizmSDK-win-0.6 extracted at PrizmSDK\PrizmSDK-win-0.6
#  Usage: powershell -ExecutionPolicy Bypass -File build.ps1
#  Output: output\2048-CG50.g3a and output\Minesweeper-CG50.g3a
# =============================================================================
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$SDK  = Join-Path $Root 'PrizmSDK\PrizmSDK-win-0.6'
$Make = Join-Path $SDK 'bin\make.exe'
$Out  = Join-Path $Root 'output'

if (-not (Test-Path $Make)) {
    Write-Host "ERROR: $Make not found. Extract PrizmSDK-win-0.6 first."
    exit 1
}

$env:FXCGSDK = $SDK
$env:PATH = "$SDK\bin;$env:PATH"

function Invoke-Build([string]$Dir, [string]$Target, [string]$Label) {
    Write-Host "==== Building $Label ===="
    $log = Join-Path $Root "_build_$Label.log"
    $err = "$log.err"
    $p = Start-Process -FilePath $Make -ArgumentList $Target -WorkingDirectory $Dir `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError $err
    if ($p.ExitCode -ne 0) {
        Write-Host "BUILD FAILED for $Label (exit $($p.ExitCode)). See $log / $err"
        exit $p.ExitCode
    }
    Write-Host "OK: $Label built."
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null

# 2048: legacy Makefile, target is 'build'
Invoke-Build (Join-Path $Root 'build\2048') 'build' '2048'
Copy-Item (Join-Path $Root 'build\2048\2048.g3a') (Join-Path $Out '2048-CG50.g3a') -Force

# Minesweeper: target is 'all'
Invoke-Build (Join-Path $Root 'build\minesweeper') 'all' 'minesweeper'
Copy-Item (Join-Path $Root 'build\minesweeper\minesweeper.g3a') (Join-Path $Out 'Minesweeper-CG50.g3a') -Force

Write-Host ""
Write-Host "All done. Artifacts:"
Get-ChildItem $Out -Filter *.g3a | Select-Object Name, @{n='Size(KB)';e={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
