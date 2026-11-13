# Builds a stand-alone Windows copy of Boat Race: dist\BoatRace-Windows.zip.
#
# Unreal cannot build Windows games on Linux, so this runs on a Windows PC
# with Unreal Engine 5.9 (the same version the Linux host uses, so the two can
# race each other) and Visual Studio 2022 with the "Game development with C++"
# workload. Run it from a fresh clone; it does every step:
#   1. finds Unreal Engine        (or set UE_ROOT to its folder)
#   2. compiles the game
#   3. fetches the Higgsfield art and imports it with the Blender models
#   4. cooks and packages for Win64
#   5. adds host.bat / join.bat and zips it
$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $Project 'BoatRace.uproject'

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Fail($text) { Write-Host "`n$text" -ForegroundColor Red; exit 1 }

# --- 1. Unreal Engine -------------------------------------------------------
Step 'Looking for Unreal Engine 5.9'
$UE = $env:UE_ROOT
if (-not $UE -or -not (Test-Path "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")) {
	$UE = $null
	$roots = @("$env:ProgramFiles\Epic Games", 'C:\Epic Games', 'D:\Epic Games', 'E:\Epic Games', 'D:\Program Files\Epic Games')
	foreach ($root in $roots) {
		if (-not (Test-Path $root)) { continue }
		$found = Get-ChildItem $root -Directory -Filter 'UE_5.9*' -ErrorAction SilentlyContinue |
			Sort-Object Name -Descending | Select-Object -First 1
		if ($found -and (Test-Path "$($found.FullName)\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")) { $UE = $found.FullName; break }
	}
}
if (-not $UE) {
	Fail ("Unreal Engine 5.9 was not found. Install it from the Epic Games Launcher (the same 5.9 version as the`n" +
		"Linux host), or set UE_ROOT, e.g.:  set UE_ROOT=D:\Epic Games\UE_5.9  and run this again.")
}
Write-Host "    $UE"
$Editor = "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

# --- 2. Compile ------------------------------------------------------------
Step 'Compiling the game (first time takes a few minutes)'
& "$UE\Engine\Build\BatchFiles\Build.bat" BoatRaceEditor Win64 Development "-project=$ProjectFile" -waitmutex
if ($LASTEXITCODE -ne 0) { Fail 'Compiling failed. Is Visual Studio 2022 with "Game development with C++" installed?' }

# --- 3. Art ----------------------------------------------------------------
# The Blender models (Art\Models\*.glb) are in the repository. The Higgsfield
# files are downloaded; without them the game still runs, with plain water.
Step 'Fetching the Higgsfield art'
$Assets = Join-Path $Project 'HiggsfieldAssets'
New-Item -ItemType Directory -Force $Assets | Out-Null
$Base = 'https://d8j0ntlcm91z4.cloudfront.net/user_3Lx3cyzN8Du6AYsTXY3qZCtf2tI'
$Files = @{
	'SM_Boat.glb'        = 'hf_20261111_141457_e242c595-23ed-43df-b7ba-c2322d8f0bb7.glb'
	'T_Title.png'        = 'hf_20261111_141144_22dfdde9-05bd-4cd6-ab38-af8bf8c5026d.png'
	'T_WaterNormal.png'  = 'hf_20261111_141438_af1b61f7-5df6-4ffc-928e-49bd6d937e5f.png'
}
foreach ($name in $Files.Keys) {
	$target = Join-Path $Assets $name
	if (Test-Path $target) { continue }
	try { Invoke-WebRequest "$Base/$($Files[$name])" -OutFile $target -UseBasicParsing; Write-Host "    $name" }
	catch { Write-Host "    could not download $name (carrying on)" -ForegroundColor Yellow }
}

if (-not (Test-Path (Join-Path $Project 'Content\Models\SM_RaceBoat.uasset'))) {
	Step 'Importing the 3D models and materials (opens Unreal headless, about 5 minutes)'
	& $Editor $ProjectFile -run=pythonscript "-script=$Project\Scripts\import_assets.py" -unattended -nosplash -AllowCommandletRendering
	if (-not (Test-Path (Join-Path $Project 'Content\Models\SM_RaceBoat.uasset'))) {
		Write-Host '    The import did not finish; the game will use simple shapes.' -ForegroundColor Yellow
	}
}

# --- 4. Package ------------------------------------------------------------
Step 'Cooking and packaging for Windows'
$Dist = Join-Path $Project 'dist'
& "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$ProjectFile" -noP4 -utf8output `
	-platform=Win64 -clientconfig=Development -build -cook -map=/Engine/Maps/Entry -stage -pak `
	-archive "-archivedirectory=$Dist"
if ($LASTEXITCODE -ne 0) { Fail 'Packaging failed; the log above says why.' }

$Game = Get-ChildItem $Dist -Recurse -Filter 'BoatRace.exe' | Where-Object { $_.DirectoryName -notmatch 'Binaries' } | Select-Object -First 1
if (-not $Game) { Fail "Packaging finished but BoatRace.exe was not found under $Dist" }
$GameDir = $Game.DirectoryName

# --- 5. Launchers and zip ----------------------------------------------------
Set-Content -Encoding ASCII (Join-Path $GameDir 'Play Boat Race.bat') "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" BoatRace.exe -windowed -ResX=1600 -ResY=900`r`n"
Set-Content -Encoding ASCII (Join-Path $GameDir 'host.bat') "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" BoatRace.exe /Engine/Maps/Entry?listen -windowed -ResX=1600 -ResY=900`r`n"
Set-Content -Encoding ASCII (Join-Path $GameDir 'join.bat') "@echo off`r`ncd /d `"%~dp0`"`r`nif `"%~1`"==`"`" (set /p ADDR=Host address: ) else (set ADDR=%~1)`r`nstart `"`" BoatRace.exe %ADDR% -windowed -ResX=1600 -ResY=900`r`n"
Set-Content -Encoding ASCII (Join-Path $GameDir 'HOW_TO_PLAY.txt') @'
BOAT RACE - up to 4 players, Windows and Linux together

Double-click "Play Boat Race.bat". The first time, Windows asks whether to
let it through the firewall: tick "Private networks" and press Allow, or
nobody can join you and you will not see their races.

On the menu, races on your Wi-Fi are listed: press 1 to join one.
H hosts a race. ENTER races solo against three computer rivals.
TAB changes the weather. M turns the music off.

W / S   throttle / reverse        A / D   steer
SPACE   torpedo                   Q       mine
R       back to the last gate     L       leave the race
'@

$Zip = Join-Path $Dist 'BoatRace-Windows.zip'
if (Test-Path $Zip) { Remove-Item $Zip }
Step 'Zipping'
Compress-Archive -Path $GameDir -DestinationPath $Zip
Write-Host "`nWindows copy:  $Zip  ($([math]::Round((Get-Item $Zip).Length / 1MB)) MB)" -ForegroundColor Green
Write-Host 'Copy it into dist\ on the Linux host and Scripts/share.sh offers it to Windows friends,'
Write-Host 'or send the zip any other way: friends unzip it and run "Play Boat Race.bat".'
