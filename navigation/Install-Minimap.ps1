param([string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Gothic II')
$ErrorActionPreference = 'Stop'
$gameRoot = [IO.Path]::GetFullPath($GameDirectory)
$gameExe = Join-Path $gameRoot 'System\Gothic2.exe'
if (!(Test-Path -LiteralPath $gameExe -PathType Leaf)) { throw "Toto neni slozka Gothic II: $gameRoot" }
foreach ($gameProcess in @(Get-Process -Name Gothic2 -ErrorAction SilentlyContinue)) {
    if ($gameProcess.Path -and [IO.Path]::GetFullPath($gameProcess.Path) -eq $gameExe) { throw 'Nejprve zavri Gothic II.' }
}
$dllRelative = 'System\Autorun\GothicQuestNav.dll'
$sourceRelative = 'GothicQuestNav\src.zip'
$packageDll = Join-Path $PSScriptRoot $dllRelative
$packageSource = Join-Path $PSScriptRoot $sourceRelative
foreach ($required in @($packageDll, $packageSource)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw 'Nejprve rozbal cely instalacni ZIP do jedne slozky.' }
}
$manifest = Join-Path $PSScriptRoot 'SHA256SUMS.txt'
if (!(Test-Path -LiteralPath $manifest)) { throw 'Chybi kontrolni soucty balicku.' }
$verified = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($line in Get-Content -LiteralPath $manifest) {
    if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw 'Poskozeny seznam kontrolnich souctu.' }
    $expectedHash = $Matches[1]; $relative = $Matches[2]
    $checkedFile = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot $relative))
    if (!$checkedFile.StartsWith($PSScriptRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Neplatna cesta v balicku.' }
    if ((Get-FileHash -LiteralPath $checkedFile -Algorithm SHA256).Hash -ne $expectedHash) { throw "Poskozeny soubor: $relative" }
    [void]$verified.Add($relative.Replace('/', '\'))
}
if (!$verified.Contains($dllRelative) -or !$verified.Contains($sourceRelative)) { throw 'Chybi kontrolni soucet DLL nebo zdroju.' }
$autorun = Join-Path $gameRoot 'System\Autorun'
$pluginRoot = Join-Path $gameRoot 'GothicQuestNav'
$targetDll = Join-Path $autorun 'GothicQuestNav.dll'
$targetSource = Join-Path $pluginRoot 'src.zip'
$backup = Join-Path $pluginRoot ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Force -Path $autorun, $pluginRoot, $backup | Out-Null
$hadDll = Test-Path -LiteralPath $targetDll
$hadSource = Test-Path -LiteralPath $targetSource
if ($hadDll) { Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backup 'GothicQuestNav.dll') }
if ($hadSource) { Copy-Item -LiteralPath $targetSource -Destination (Join-Path $backup 'src.zip') }
try {
    Copy-Item -LiteralPath $packageDll -Destination $targetDll -Force
    if ((Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $packageDll -Algorithm SHA256).Hash) { throw 'Kontrola DLL selhala.' }
    Copy-Item -LiteralPath $packageSource -Destination $targetSource -Force
}
catch {
    $installError = $_
    if ($hadDll) { Copy-Item -LiteralPath (Join-Path $backup 'GothicQuestNav.dll') -Destination $targetDll -Force -ErrorAction SilentlyContinue }
    else { Remove-Item -LiteralPath $targetDll -Force -ErrorAction SilentlyContinue }
    if ($hadSource) { Copy-Item -LiteralPath (Join-Path $backup 'src.zip') -Destination $targetSource -Force -ErrorAction SilentlyContinue }
    else { Remove-Item -LiteralPath $targetSource -Force -ErrorAction SilentlyContinue }
    throw $installError
}
Write-Host 'Gothic QuestNav 0.5.0 je nainstalovany. Trainer zustal beze zmeny.' -ForegroundColor Green
Write-Host 'Spust Gothic s aktivnim Unionem. M = velka mapa, F6 = minimapa, F7 = teren/pergamen, +/- = rozsah.'
Write-Host "Predchozi verze: $backup"
