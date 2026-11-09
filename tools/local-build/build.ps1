# Gothic Trainer – sestavení přímo na tomto počítači.
#
# Spouští se přes Sestavit_trainer.bat. Při prvním spuštění nainstaluje
# Microsoft Visual Studio Build Tools (překladač C++ od Microsoftu, zdarma).
# Potom hlídá soubor src.zip: kdykoli se změní, trainer i minimapa se znovu
# sestaví a zkopírují do hry (system\Autorun\GothicTrainer.dll a GothicQuestNav.dll).
# Okno nech otevřené, dokud ladíme. Zavřít ho můžeš kdykoli.

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

$Root       = Split-Path -Parent $MyInvocation.MyCommand.Path
$GameDir    = Split-Path -Parent $Root
$Autorun    = Join-Path $GameDir 'system\Autorun'
$SrcZip     = Join-Path $Root 'src.zip'
$Incoming   = Join-Path $Root '_incoming'
$SrcDir     = Join-Path $Root '_src'
$BuildDir   = Join-Path $Root '_build'
$ToolsDir   = Join-Path $Root '_tools'
$LogFile    = Join-Path $Root 'build.log'
$StatusFile = Join-Path $Root 'status.txt'
$BuiltDll   = Join-Path $BuildDir 'GothicTrainer.dll'
$TargetDll  = Join-Path $Autorun 'GothicTrainer.dll'
$BuiltNav   = Join-Path $BuildDir 'GothicQuestNav.dll'
$TargetNav  = Join-Path $Autorun 'GothicQuestNav.dll'
$NavBackup  = Join-Path $GameDir 'GothicQuestNav\backups\pred-sestavenim'

$script:BuildId = '-'

function Say([string]$text, [string]$color = 'Gray') {
	Write-Host ('[{0:HH:mm:ss}] {1}' -f (Get-Date), $text) -ForegroundColor $color
}

function Set-Status([string]$state, [string]$detail) {
	$line = '{0}|{1:yyyy-MM-dd HH:mm:ss}|{2}|{3}' -f $state, (Get-Date), $script:BuildId, $detail
	[IO.File]::WriteAllText($StatusFile, $line + "`r`n", (New-Object Text.UTF8Encoding $false))
}

function Write-Log([string]$text) {
	Add-Content -Path $LogFile -Value $text -Encoding UTF8
}

# ---------------------------------------------------------------- Build Tools

function Find-VisualStudio {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (-not (Test-Path $vswhere)) { return $null }
	$found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
	if ($found) { return ([string]($found | Select-Object -First 1)).Trim() }
	return $null
}

function Install-BuildTools {
	Say 'Stahuji instalátor Visual Studio Build Tools 2022 od Microsoftu...' Cyan
	$installer = Join-Path $env:TEMP 'vs_BuildTools.exe'
	Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile $installer -UseBasicParsing
	Say 'Instaluji překladač C++ (několik GB, obvykle 10–30 minut).' Yellow
	Say 'Windows se zeptá na oprávnění – potvrď ho prosím (Ano).' Yellow
	$installArgs = @(
		'--passive', '--wait', '--norestart', '--nocache',
		'--add', 'Microsoft.VisualStudio.Workload.VCTools',
		'--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
		'--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project',
		'--includeRecommended',
		'--addProductLang', 'en-US'
	)
	$process = Start-Process -FilePath $installer -ArgumentList $installArgs -Verb RunAs -Wait -PassThru
	if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) {
		throw "Instalace Build Tools skončila s kódem $($process.ExitCode)."
	}
	Say 'Build Tools nainstalovány.' Green
}

function Import-VsEnvironment([string]$vsPath) {
	$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars32.bat'
	if (-not (Test-Path $vcvars)) { throw "Chybí $vcvars" }
	$lines = & cmd.exe /s /c "`"$vcvars`" >nul 2>&1 && set"
	foreach ($line in $lines) {
		$eq = $line.IndexOf('=')
		if ($eq -gt 0) {
			[Environment]::SetEnvironmentVariable($line.Substring(0, $eq), $line.Substring($eq + 1), 'Process')
		}
	}
	$cmakeRoot = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
	foreach ($dir in @((Join-Path $cmakeRoot 'Ninja'), (Join-Path $cmakeRoot 'CMake\bin'))) {
		if (Test-Path $dir) { $env:PATH = "$dir;$env:PATH" }
	}
	# anglické hlášky překladače (kvůli logu)
	$env:VSLANG = '1033'
}

function Get-PortableTool([string]$name, [string]$url, [string]$exeRelative) {
	$dir = Join-Path $ToolsDir $name
	$exe = Join-Path $dir $exeRelative
	if (Test-Path $exe) { return (Split-Path -Parent $exe) }
	Say "Stahuji $name..." Cyan
	New-Item -ItemType Directory -Force -Path $dir | Out-Null
	$zip = Join-Path $ToolsDir "$name.zip"
	Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
	Add-Type -AssemblyName System.IO.Compression.FileSystem
	[IO.Compression.ZipFile]::ExtractToDirectory($zip, $dir)
	Remove-Item $zip -Force
	if (-not (Test-Path $exe)) { throw "Po rozbalení chybí $exe" }
	return (Split-Path -Parent $exe)
}

function Confirm-Tools {
	if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Překladač cl.exe nebyl nalezen.' }
	if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
		$bin = Get-PortableTool 'cmake' 'https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-windows-x86_64.zip' 'cmake-3.31.6-windows-x86_64\bin\cmake.exe'
		$env:PATH = "$bin;$env:PATH"
	}
	if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
		$bin = Get-PortableTool 'ninja' 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip' 'ninja.exe'
		$env:PATH = "$bin;$env:PATH"
	}
}

# ---------------------------------------------------------------- build

function Invoke-Tool([string]$exe, [string[]]$toolArgs) {
	$previous = $ErrorActionPreference
	$ErrorActionPreference = 'Continue'
	try {
		& $exe @toolArgs 2>&1 | ForEach-Object { "$_" } | Out-File -FilePath $LogFile -Append -Encoding UTF8
		return $LASTEXITCODE
	}
	finally {
		$ErrorActionPreference = $previous
	}
}

function Test-FileReady([string]$path) {
	try {
		$stream = [IO.File]::Open($path, 'Open', 'Read', 'None')
		$stream.Close()
		return $true
	}
	catch {
		return $false
	}
}

function Invoke-Build {
	$script:BuildId = '-'
	Set-Status 'BUILDING' ''
	Say 'Nový zdrojový kód – sestavuji trainer...' Cyan

	if (Test-Path $Incoming) { Remove-Item $Incoming -Recurse -Force }
	Add-Type -AssemblyName System.IO.Compression.FileSystem
	[IO.Compression.ZipFile]::ExtractToDirectory($SrcZip, $Incoming)
	$idFile = Join-Path $Incoming 'BUILD_ID.txt'
	if (Test-Path $idFile) { $script:BuildId = (Get-Content $idFile -Raw).Trim() }
	Set-Status 'BUILDING' ''

	# copies only changed files, so the next build is incremental
	& robocopy.exe $Incoming $SrcDir /MIR /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
	if ($LASTEXITCODE -ge 8) { throw "Kopírování zdrojáků selhalo (robocopy $LASTEXITCODE)." }
	Remove-Item $Incoming -Recurse -Force

	Set-Content -Path $LogFile -Value ("=== Build {0} ({1:yyyy-MM-dd HH:mm:ss}) ===" -f $script:BuildId, (Get-Date)) -Encoding UTF8

	if (-not (Test-Path (Join-Path $BuildDir 'build.ninja'))) {
		Write-Log '--- configure ---'
		$code = Invoke-Tool 'cmake.exe' @('-S', $SrcDir, '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl')
		if ($code -ne 0) {
			if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue }
			throw 'Konfigurace CMake selhala.'
		}
	}

	Write-Log '--- build ---'
	$code = Invoke-Tool 'cmake.exe' @('--build', $BuildDir, '--target', 'GothicTrainer', 'GothicQuestNav')
	if ($code -ne 0) { throw 'Kompilace selhala.' }
	if (-not (Test-Path $BuiltDll)) { throw 'Kompilace proběhla, ale GothicTrainer.dll chybí.' }
	if (-not (Test-Path $BuiltNav)) { throw 'Kompilace proběhla, ale GothicQuestNav.dll chybí.' }
	Write-Log '--- build OK ---'
}

function Copy-Dll([string]$from, [string]$to) {
	if (-not (Test-Path $from)) { return $true }
	if ((Test-Path $to) -and ((Get-FileHash $from).Hash -eq (Get-FileHash $to).Hash)) { return $true }
	try {
		Copy-Item -Path $from -Destination $to -Force
		return $true
	}
	catch {
		return $false
	}
}

function Install-Dll {
	if (-not (Test-Path $BuiltDll)) { return $false }
	if (-not (Test-Path $Autorun)) { New-Item -ItemType Directory -Force -Path $Autorun | Out-Null }
	# předchozí minimapa (třeba z jejího vlastního instalátoru) zůstane v záloze
	if ((Test-Path $BuiltNav) -and (Test-Path $TargetNav) -and ((Get-FileHash $BuiltNav).Hash -ne (Get-FileHash $TargetNav).Hash)) {
		try {
			New-Item -ItemType Directory -Force -Path $NavBackup | Out-Null
			Copy-Item -Path $TargetNav -Destination (Join-Path $NavBackup 'GothicQuestNav.dll') -Force
		}
		catch { }
	}
	$trainer = Copy-Dll $BuiltDll $TargetDll
	$minimap = Copy-Dll $BuiltNav $TargetNav
	return ($trainer -and $minimap)
}

# ---------------------------------------------------------------- main

$Host.UI.RawUI.WindowTitle = 'Gothic Trainer – sestavení'
Say 'Gothic Trainer – sestavení na tomto počítači' White
Say "Složka: $Root" DarkGray

try {
	$vs = Find-VisualStudio
	if (-not $vs) {
		Set-Status 'INSTALLING' 'Visual Studio Build Tools'
		Install-BuildTools
		$vs = Find-VisualStudio
	}
	if (-not $vs) { throw 'Visual Studio Build Tools se nepodařilo najít ani po instalaci.' }
	Say "Překladač: $vs" DarkGray
	Import-VsEnvironment $vs
	Confirm-Tools
	Set-Status 'READY' $vs
	Say 'Připraveno. Čekám na zdrojový kód (src.zip)...' Green
}
catch {
	Set-Status 'ERROR' $_.Exception.Message
	Say ('Chyba: ' + $_.Exception.Message) Red
	Read-Host 'Stiskni Enter pro zavření'
	exit 1
}

$lastStamp = ''
$pendingInstall = $false
while ($true) {
	try {
		if (Test-Path $SrcZip) {
			$item = Get-Item $SrcZip
			$stamp = '{0}|{1}' -f $item.LastWriteTimeUtc.Ticks, $item.Length
			if ($stamp -ne $lastStamp -and (Test-FileReady $SrcZip)) {
				Start-Sleep -Milliseconds 800
				$item = Get-Item $SrcZip
				$stamp = '{0}|{1}' -f $item.LastWriteTimeUtc.Ticks, $item.Length
				$lastStamp = $stamp
				try {
					Invoke-Build
					if (Install-Dll) {
						$pendingInstall = $false
						Set-Status 'OK' ($TargetDll + ';' + $TargetNav)
						Say 'Hotovo! Trainer i minimapa jsou ve hře – spusť Gothic, F10 trainer, M mapa.' Green
					}
					else {
						$pendingInstall = $true
						Set-Status 'WAITING_GAME' 'Hra běží, DLL nainstaluji po jejím zavření.'
						Say 'Sestaveno, ale hra běží. Až ji zavřeš, trainer i minimapa se nainstalují samy.' Yellow
					}
				}
				catch {
					Set-Status 'FAIL' $_.Exception.Message
					Say ('Chyba: ' + $_.Exception.Message + ' Podrobnosti jsou v build.log, opravuji to.') Red
				}
			}
		}
		if ($pendingInstall -and (Install-Dll)) {
			$pendingInstall = $false
			Set-Status 'OK' ($TargetDll + ';' + $TargetNav)
			Say 'Trainer i minimapa nainstalovány do hry.' Green
		}
	}
	catch {
		Say ('Chyba: ' + $_.Exception.Message) Red
	}
	Start-Sleep -Seconds 2
}
