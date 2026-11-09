param([string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Gothic II', [switch]$Minimap)
$ErrorActionPreference = 'Stop'
$gameRoot = [IO.Path]::GetFullPath($GameDirectory)
$gameExe = Join-Path $gameRoot 'system\Gothic2.exe'
if (!(Test-Path -LiteralPath $gameExe -PathType Leaf)) { throw "Toto neni slozka Gothic II: $gameRoot" }
$pluginName = if ($Minimap) { 'GothicQuestNav.dll' } else { 'GothicTrainer.dll' }
$packageDll = Join-Path $PSScriptRoot ('System\Autorun\' + $pluginName)
$packageSource = Join-Path $PSScriptRoot 'GothicTrainer\src.zip'
foreach ($required in @($packageDll, $packageSource)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw 'Nejprve rozbal cely instalacni ZIP do jedne slozky.' }
}
foreach ($gameProcess in @(Get-Process -Name Gothic2 -ErrorAction SilentlyContinue)) {
    if ($gameProcess.Path -and [IO.Path]::GetFullPath($gameProcess.Path) -eq $gameExe) {
        throw 'Nejprve zavri Gothic II, potom spust instalaci znovu.'
    }
}
$hashFile = Join-Path $PSScriptRoot 'SHA256SUMS.txt'
if (!(Test-Path -LiteralPath $hashFile)) { throw 'Chybi kontrolni soucty balicku.' }
foreach ($line in Get-Content -LiteralPath $hashFile) {
    if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw 'Poskozeny seznam kontrolnich souctu.' }
    $expectedHash = $Matches[1]
    $relativeFile = $Matches[2]
    $checkedFile = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot $relativeFile))
    if (!$checkedFile.StartsWith($PSScriptRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Neplatna cesta v balicku.' }
    if ((Get-FileHash -LiteralPath $checkedFile -Algorithm SHA256).Hash -ne $expectedHash) { throw "Poskozeny soubor: $relativeFile" }
}
$autorun = Join-Path $gameRoot 'system\Autorun'
$trainerRoot = Join-Path $gameRoot 'GothicTrainer'
$targetDll = Join-Path $autorun $pluginName
$targetSource = Join-Path $trainerRoot 'src.zip'
$backup = Join-Path $trainerRoot ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6))
New-Item -ItemType Directory -Force -Path $autorun, $trainerRoot, $backup | Out-Null
$hadDll = Test-Path -LiteralPath $targetDll
$hadSource = Test-Path -LiteralPath $targetSource
if ($hadDll) { Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backup $pluginName) }
if ($hadSource) { Copy-Item -LiteralPath $targetSource -Destination (Join-Path $backup 'src.zip') }
try {
    Copy-Item -LiteralPath $packageDll -Destination $targetDll -Force
    if ((Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $packageDll -Algorithm SHA256).Hash) { throw 'Kontrola nainstalovane DLL selhala.' }
    Copy-Item -LiteralPath $packageSource -Destination $targetSource -Force
}
catch {
    $installError = $_
    if ($hadDll) { Copy-Item -LiteralPath (Join-Path $backup $pluginName) -Destination $targetDll -Force -ErrorAction SilentlyContinue }
    if ($hadSource) { Copy-Item -LiteralPath (Join-Path $backup 'src.zip') -Destination $targetSource -Force -ErrorAction SilentlyContinue }
    throw $installError
}
if ($Minimap) {
    Write-Host 'Gothic QuestNav 0.5.0 (minimapa) je nainstalovany.' -ForegroundColor Green
    Write-Host 'M velka mapa, F6 minimapa, Shift+F6 dalsi ukol, F7 teren / pergamen. Cile ukazuje u vsech ukolu v deniku.'
} else {
    Write-Host 'Gothic Trainer 0.5.2 je nainstalovany.' -ForegroundColor Green
    Write-Host 'Spust Gothic pres Steam Mod Launcher se zapnutym Unionem. Trainer otevre F10.'
}
Write-Host "Predchozi verze: $backup"
