# Installs Boat Race on a Windows PC by running the Linux copy inside WSL
# (Windows Subsystem for Linux), because the game can only be built for
# Windows on a Windows PC. Scripts/share.sh serves this file with the host's
# address filled in; a friend runs it through "Install Boat Race.bat".
#
# What it does, each step skipped if already done:
#   1. turns on WSL and installs Ubuntu (asks to restart the PC once)
#   2. lets WSL share the PC's network ("mirrored" mode, Windows 11 22H2+),
#      so races on the Wi-Fi show up and others can join this PC
#   3. installs the graphics drivers WSL needs and a user to play as
#   4. downloads the game from the host and adds desktop icons
$ErrorActionPreference = 'Stop'
$From = 'http://@ADDRESS@:@PORT@'
$Distro = 'Ubuntu'

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Done($text) { Write-Host "    $text" -ForegroundColor Green }
function Quit($text) { Write-Host "`n$text" -ForegroundColor Yellow; Read-Host 'Press Enter to close'; exit 1 }

$build = [int](Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuildNumber
if ($build -lt 19044) { Quit 'This needs Windows 10 21H2 or Windows 11. Run Windows Update first.' }

# --- 1. WSL and Ubuntu ---------------------------------------------------------
Step 'Checking WSL'
$distros = (wsl.exe --list --quiet 2>$null) -replace "`0", '' | Where-Object { $_ -match '\S' }
if (-not ($distros -match "^$Distro")) {
	Write-Host '    Installing WSL and Ubuntu. This downloads about 1 GB.'
	wsl.exe --install -d $Distro --no-launch
	if ($LASTEXITCODE -ne 0) {
		Quit ("WSL did not install. Turn on virtualisation (VT-x / SVM) in the PC's BIOS if it is off,`n" +
			"then run Install Boat Race again.")
	}
}
$distros = (wsl.exe --list --quiet 2>$null) -replace "`0", '' | Where-Object { $_ -match '\S' }
if (-not ($distros -match "^$Distro")) {
	# Older Ubuntu packages only register on first launch, which would stop to
	# ask for a user name; register it directly instead.
	foreach ($launcher in 'ubuntu.exe', 'ubuntu2404.exe', 'ubuntu2204.exe') {
		if (Get-Command $launcher -ErrorAction SilentlyContinue) { & $launcher install --root | Out-Null; break }
	}
	$distros = (wsl.exe --list --quiet 2>$null) -replace "`0", '' | Where-Object { $_ -match '\S' }
	if (-not ($distros -match "^$Distro")) {
		Quit 'WSL is installed but Windows must restart first. Restart the PC, then run Install Boat Race again.'
	}
}
wsl.exe --update 2>$null | Out-Null
Done 'WSL and Ubuntu are ready'

# --- 2. Network --------------------------------------------------------------
Step 'Sharing the network with WSL'
$config = Join-Path $env:USERPROFILE '.wslconfig'
$text = if (Test-Path $config) { Get-Content $config -Raw } else { '' }
if ($build -ge 22621) {
	if ($text -notmatch 'networkingMode\s*=\s*mirrored') {
		if ($text -notmatch '\[wsl2\]') { $text = "[wsl2]`r`n" + $text }
		$text = $text -replace '\[wsl2\]', "[wsl2]`r`nnetworkingMode=mirrored"
		Set-Content $config $text
		wsl.exe --shutdown
	}
	# Let other PCs reach the game inside WSL (race list and joining this PC).
	try {
		Set-NetFirewallHyperVVMSetting -Name '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -DefaultInboundAction Allow
	} catch { }
	Done 'Races on this Wi-Fi will show up on the menu'
} else {
	Write-Host '    This Windows is older than 11 22H2: races will not be listed on the menu.' -ForegroundColor Yellow
	Write-Host '    You can still join: press J on the menu and type the host''s address.' -ForegroundColor Yellow
}

# --- 3. Drivers and a player account inside Ubuntu ----------------------------
Step 'Installing graphics drivers inside Ubuntu (a few minutes)'
$setup = @'
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq mesa-vulkan-drivers libvulkan1 vulkan-tools wget ca-certificates \
	libx11-6 libxcursor1 libxrandr2 libxi6 libxinerama1 libgl1 libpulse0 libasound2t64 >/dev/null 2>&1 \
	|| apt-get install -y -qq mesa-vulkan-drivers libvulkan1 vulkan-tools wget ca-certificates \
	libx11-6 libxcursor1 libxrandr2 libxi6 libxinerama1 libgl1 libpulse0 libasound2 >/dev/null
id racer >/dev/null 2>&1 || useradd -m -s /bin/bash racer
'@
wsl.exe -d $Distro -u root -- bash -c ($setup -replace "`r", '')
if ($LASTEXITCODE -ne 0) { Quit 'Installing the drivers failed. Check the internet connection and run it again.' }
Done 'Drivers installed'

# --- 4. The game ---------------------------------------------------------------
Step "Downloading Boat Race from $From"
$fetch = @"
set -e
cd /home/racer
rm -rf BoatRace.tmp && mkdir BoatRace.tmp
wget -q --show-progress -O game.tar.gz '$From/BoatRace-Linux.tar.gz'
tar -xzf game.tar.gz -C BoatRace.tmp --strip-components=1
rm -rf BoatRace && mv BoatRace.tmp BoatRace && rm -f game.tar.gz
chown -R racer:racer BoatRace
"@
wsl.exe -d $Distro -u root -- bash -c ($fetch -replace "`r", '')
if ($LASTEXITCODE -ne 0) { Quit "Could not download the game. Is share.sh still running on the host, and are you on the same Wi-Fi?" }
Done 'Downloaded'

Step 'Adding desktop icons'
$desktop = [Environment]::GetFolderPath('Desktop')
$shell = New-Object -ComObject WScript.Shell
function Shortcut($name, $arguments) {
	$link = $shell.CreateShortcut((Join-Path $desktop "$name.lnk"))
	$link.TargetPath = "$env:WINDIR\System32\wsl.exe"
	$link.Arguments = $arguments
	$link.WindowStyle = 7
	$link.Description = 'Four-player powerboat racing'
	$link.Save()
}
# Normal: the PC's own graphics card through WSL. Safe mode: drawn by the
# processor instead, slower but works on any PC whose graphics WSL cannot use.
Shortcut 'Boat Race' "-d $Distro -u racer -- /home/racer/BoatRace/BoatRace.sh -windowed -ResX=1600 -ResY=900"
Shortcut 'Boat Race (safe mode)' "-d $Distro -u racer -- env VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json /home/racer/BoatRace/BoatRace.sh -windowed -ResX=1024 -ResY=576"
Done 'Boat Race is on your desktop'

Write-Host "`nAll set. Double-click Boat Race on the desktop." -ForegroundColor Green
Write-Host 'If it does not open, or runs very slowly, try Boat Race (safe mode).'
Read-Host 'Press Enter to close'
