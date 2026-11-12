param(
    [switch]$List,
    [string]$Port,
    [switch]$BoardConfirmed,
    [switch]$Monitor
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)

$pio = Get-Command pio.exe,platformio.exe -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
$prefix = @()
if ($pio) {
    $executable = $pio.Source
} elseif (Test-Path -LiteralPath (Join-Path $env:USERPROFILE '.platformio/penv/Scripts/platformio.exe')) {
    $executable = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/platformio.exe'
} else {
    $python = Get-Command python.exe -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $python) { throw 'Falta PlatformIO. Instala PlatformIO Core y vuelve a abrir este archivo.' }
    $executable = $python.Source
    $prefix = @('-m', 'platformio')
    & $executable @prefix --version
    if ($LASTEXITCODE -ne 0) { throw 'Falta PlatformIO. Instala PlatformIO Core y vuelve a intentarlo.' }
}

& $executable @prefix device list
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($List) { exit 0 }

Write-Host 'Usa un cable USB de datos y cierra otros monitores serie antes de cargar.'
Write-Host 'Confirma el modelo de la placa y su perfil en platformio.ini antes de continuar.'
if (-not $BoardConfirmed -and (Read-Host 'Escribe SI si ya confirmaste la placa') -ne 'SI') {
    Write-Host 'Carga cancelada.'
    exit 0
}
if (-not $Port) { $Port = Read-Host 'Puerto de la placa (por ejemplo COM5)' }
if ($Port -notmatch '^COM[1-9][0-9]*$') { throw 'Indica el puerto COM de la placa que aparece en la lista.' }

& $executable @prefix run -e esp32 -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Revisa el puerto, el cable y las instrucciones BOOT/RESET del modelo confirmado.'
    exit $LASTEXITCODE
}
if ($Monitor -or (-not $BoardConfirmed -and (Read-Host 'Escribe SI para abrir el monitor serie') -eq 'SI')) {
    & $executable @prefix device monitor -e esp32 --port $Port --echo
    exit $LASTEXITCODE
}
