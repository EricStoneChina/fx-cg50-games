param([string]$Compiler = 'gcc')

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Sdk = Join-Path $Repo 'PrizmSDK\PrizmSDK-win-0.6'
$Work = Join-Path $Repo '.build\minesweeper-host'
$Include = Join-Path $Work 'include'
$Executable = Join-Path $Work 'minesweeper-host.exe'
$CompilerPath = (Get-Command $Compiler -ErrorAction Stop).Source

# Keep host mocks checked against the real SDK syscall declarations.
New-Item -ItemType Directory -Force -Path "$Include\fxcg" | Out-Null
foreach ($Header in @('display.h', 'keyboard.h', 'rtc.h', 'file.h', 'heap.h', 'misc.h', 'system.h')) {
    Copy-Item -LiteralPath "$Sdk\include\fxcg\$Header" -Destination "$Include\fxcg\$Header"
}

$PreviousPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $CompilerPath) + ';' + $env:PATH
    & $CompilerPath -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter `
        -I $Include (Join-Path $PSScriptRoot 'minesweeper-host.c') -o $Executable
    if ($LASTEXITCODE -ne 0) { throw "Minesweeper host compilation failed: $LASTEXITCODE" }
    & $Executable
    if ($LASTEXITCODE -ne 0) { throw "Minesweeper host regression failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $PreviousPath
}
