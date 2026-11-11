param([switch]$Force)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)

$inputs = @(Get-ChildItem -Path include,src/core,src/windows -File -Recurse) + @(Get-Item -LiteralPath $PSCommandPath)
$existing = Get-Item -LiteralPath Calcu.exe -ErrorAction SilentlyContinue
if (-not $Force -and $existing -and -not ($inputs | Where-Object LastWriteTimeUtc -GT $existing.LastWriteTimeUtc)) {
    exit 0
}

$candidates = @()
if ($env:CALCU_CXX) { $candidates += $env:CALCU_CXX }
$candidates += 'g++.exe', 'clang++.exe',
    'C:\msys64\clang64\bin\clang++.exe', 'C:\msys64\ucrt64\bin\g++.exe', 'C:\msys64\mingw64\bin\g++.exe'
$compiler = $null
foreach ($name in $candidates) {
    $tool = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue
    if ($tool) { $compiler = $tool.Source; break }
}
if (-not $compiler) {
    Write-Host 'Falta un compilador C++17 para Windows (MinGW-w64 o MSYS2 CLANG64).'
    Write-Host 'Si tienes Calcu.exe, puedes abrirlo directamente. Consulta el README para recompilar.'
    exit 1
}
$env:Path = (Split-Path -Parent $compiler) + ';' + $env:Path
$sources = @(Get-ChildItem -Path src/core,src/windows -File -Filter '*.cpp' | ForEach-Object FullName)
$binary = Join-Path $PWD 'Calcu.exe'
$tempExe = Join-Path $PWD "Calcu.build.$PID.exe"
try {
    & $compiler -std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -DNOMINMAX -I include @sources -static -municode -mwindows -lwinhttp -lcrypt32 -lgdi32 -o $tempExe
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar. Revisa los mensajes anteriores.' }
    if ([IO.File]::Exists($binary)) {
        [IO.File]::Replace($tempExe, $binary, [System.Management.Automation.Language.NullString]::Value)
    } else {
        [IO.File]::Move($tempExe, $binary)
    }
} catch {
    Write-Host $_.Exception.Message
    Write-Host 'Si Calcu ya esta abierto, cierralo y vuelve a intentar.'
    exit 1
} finally {
    [IO.File]::Delete($tempExe)
}
