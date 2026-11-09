param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'out\build\G2A-Release'),
    [switch]$Tests
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Chybi Visual Studio Build Tools s prekladacem C++ x86.' }
$vsPath = [string](& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (!$vsPath) { throw 'Chybi Visual Studio Build Tools s prekladacem C++ x86.' }
$vcvars = Join-Path $vsPath.Trim() 'VC\Auxiliary\Build\vcvars32.bat'
foreach ($line in (& cmd.exe /s /c "`"$vcvars`" >nul 2>&1 && set")) {
    $equals = $line.IndexOf('=')
    if ($equals -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0, $equals), $line.Substring($equals + 1), 'Process') }
}
$cmakeRoot = Join-Path $vsPath.Trim() 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = (Join-Path $cmakeRoot 'CMake\bin') + ';' + (Join-Path $cmakeRoot 'Ninja') + ';' + $env:PATH
$env:VSLANG = '1033'
$testOption = if ($Tests) { 'ON' } else { 'OFF' }
& cmake.exe -S $PSScriptRoot -B $BuildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_C_COMPILER=cl.exe' '-DCMAKE_CXX_COMPILER=cl.exe' "-DGOTHIC_TRAINER_BUILD_TESTS=$testOption"
if ($LASTEXITCODE -ne 0) { throw 'Konfigurace CMake selhala.' }
& cmake.exe --build $BuildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Kompilace selhala.' }
if ($Tests) {
    & ctest.exe --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Regresni testy selhaly.' }
}
Write-Host "Hotovo: $(Join-Path $BuildDirectory 'GothicTrainer.dll')" -ForegroundColor Green
