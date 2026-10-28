param([string]$Compiler = 'gcc')

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Sdk = Join-Path $Repo 'PrizmSDK\PrizmSDK-win-0.6'
$Work = Join-Path $Repo '.build\2048-host'
$Include = Join-Path $Work 'include'
$Executable = Join-Path $Work '2048-host.exe'
$CompilerPath = (Get-Command $Compiler -ErrorAction Stop).Source

# Use the SDK's syscall declarations with host implementations in the test.
New-Item -ItemType Directory -Force -Path "$Include\fxcg" | Out-Null
foreach ($Header in @('display.h', 'keyboard.h', 'rtc.h', 'file.h', 'system.h')) {
    Copy-Item -LiteralPath "$Sdk\include\fxcg\$Header" -Destination "$Include\fxcg\$Header"
}

$PreviousPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $CompilerPath) + ';' + $env:PATH
    & $CompilerPath -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-int-to-pointer-cast `
        -I $Include (Join-Path $PSScriptRoot '2048-host.c') -o $Executable
    if ($LASTEXITCODE -ne 0) { throw "2048 host compilation failed: $LASTEXITCODE" }
    Push-Location $Work
    try {
        & $Executable
        if ($LASTEXITCODE -ne 0) { throw "2048 host regression failed: $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
} finally {
    $env:PATH = $PreviousPath
}
