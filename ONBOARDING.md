# Getting four people into a race

Everyone on the same Wi-Fi, everyone on Linux. Three steps, no addresses to
type, nothing to unpack.

## 1. The host shares the game

The host is whoever has Unreal Engine and this project. Once, after any change
to the game:

```bash
Scripts/package.sh
```

Then, whenever friends need the game:

```bash
Scripts/share.sh
```

It prints one line, something like:

```
wget -qO- http://192.168.1.20:8000/install.sh | bash
```

## 2. Each friend pastes that line into a terminal

It downloads the game (about 420 MB), installs it in `~/BoatRace` and adds
**Boat Race** to their apps. They don't need Unreal Engine. When everyone has
it, the host presses Ctrl+C to stop sharing.

## 3. Play

| Who | What they do |
|---|---|
| Host | `Scripts/play.sh`, then press **H** |
| Friends | Open **Boat Race**. The host's race appears on the menu; press **1** to join |
| Host, when the lobby shows everyone | Press **ENTER** to start |

The menu finds races by itself: a host announces its race on the Wi-Fi once a
second, and the list updates as races start and end. Joining by typed address
(**J**) still works for anything the list can't see, such as a friend on
another network via Tailscale or ZeroTier.

## What each machine needs

| | Host | Friends |
|---|---|---|
| Linux | Yes | Yes |
| Graphics with Vulkan (`vulkaninfo --summary` shows a GPU) | Yes | Yes |
| Unreal Engine | Yes | **No** |
| Disk space | ~60 GB (the engine) | ~1 GB |

Integrated Intel or AMD graphics work on the low preset, which `setup.sh`
picks automatically.

## Friends on Windows

Windows and Linux players race together on the same Wi-Fi. There are two ways
in.

### No Windows PC to build on: run the Linux copy inside Windows

`share.sh` prints a web address as well as the Linux line. A Windows friend:

1. Opens that address in a browser and downloads **Install Boat Race.bat**.
2. Double-clicks it. If Windows says it protected the PC: *More info* then
   *Run anyway*, and *Yes* to the administrator question.
3. Waits. It turns on WSL (Windows' built-in Linux, about 1 GB), sets it to
   share the PC's network, installs graphics drivers, downloads the game from
   the host and puts **Boat Race** on the desktop. The very first time,
   Windows may need a restart: restart, then double-click the installer again.
4. Plays with the **Boat Race** icon. If it will not open or is very slow,
   **Boat Race (safe mode)** draws with the processor instead of the graphics
   card: slower, but it works anywhere.

Needs Windows 10 21H2 or Windows 11, with virtualisation turned on in the
BIOS (it usually is). On Windows 11 22H2 or later, races appear on the menu by
themselves; on older Windows, press **J** and type the host's address.

This route has not been tried on a real Windows PC yet. How fast it runs
depends on how well WSL drives that PC's graphics.

### A Windows PC with Unreal Engine: a real Windows build

On a Windows PC with **Unreal Engine 5.9** (the same version as the Linux
host) and **Visual Studio 2022** with *Game development with C++*: clone the
repository and double-click `Scripts\package_windows.bat`. It leaves
`dist\BoatRace-Windows.zip`. Copy that into `dist/` on the Linux host, and
`share.sh` offers it on the download page instead of the WSL installer:
friends unzip it and double-click **Play Boat Race.bat**. Rebuild it whenever
the game changes, or the versions will not match.

## Controls

| Key | Action |
|---|---|
| W / S | Throttle / reverse |
| A / D | Steer |
| SPACE or left mouse | Fire a torpedo |
| Q or right mouse | Drop a mine |
| R | Back to your last gate |
| ESC | Pause menu (also: settings, controls, leave, quit) |
| ENTER | Host only: start the race |
| TAB | Host, in the lobby: change the weather |
| M | Music on / off |
| L | Leave |

Gamepad: right trigger throttle, left trigger reverse, left stick steer,
**A** torpedo, **X** mine, **Y** reset, **Start** begin.

## How a race works

- Two laps of a 3 km circuit, sixteen gates, 90 m wide between the buoys.
- Pass the gates **in order**. Orange buoys mark the inside of the course,
  white the outside, and the start line is the pair of tall yellow ones. Your
  next gate pulses and the needle at the top of the screen points at it.
- Empty seats are taken by computer rivals (Kestrel, Marlin, Barracuda,
  Osprey), so there are always four boats. A friend who joins takes one.
- Five red lights come on a second apart; when they go out, go.
- The host picks the weather in the lobby with TAB: sunny, cloudy, rainy,
  stormy (lightning), windy (gusts push the boats), snowy, foggy, sunset or
  night.
- Shooting is a racing tactic, not a kill count. Wrecking someone costs them
  about eight seconds: four drifting, plus the run back up to speed.
- Your own blast cannot damage you, but it can shove you off line.
- Ammunition is in the floating crates on the course. They come back 15 seconds
  after being taken.
- Watch for the sea stacks, the floating logs, the two barges crossing the
  course in their channels, the ramps, the three whirlpools, and the narrows
  under the red bridge. Hard hits on rocks and the shore cost hull; ramming
  another boat hurts them, not you.

## When something goes wrong

| Problem | Cause and fix |
|---|---|
| The menu says "Looking for races..." and never lists the host | Not on the same Wi-Fi, a guest network that keeps devices apart, or a firewall. On the host: `sudo ufw allow 7777:7778/udp`. Otherwise press **J** and type the host's address (`hostname -I` on the host) |
| The pasted line says "connection refused" or hangs | `share.sh` isn't running on the host, or a firewall is blocking it: `sudo ufw allow 8000/tcp` |
| Joining drops straight back to the menu | The host's firewall is blocking UDP 7777: `sudo ufw allow 7777/udp` |
| "This race is full" | Four are already in |
| Boats jitter or rubber-band | Poor connection to the host. The host should be close to the router, or wired |
| The host leaves | The race ends for everyone. The host should be the most stable machine |
| A Windows player cannot see or join races | They did not allow the game through Windows Firewall. Windows Security > Firewall > *Allow an app* > tick BoatRace for Private networks |
| One player sees different weather or a different sea | They have an older copy. Run `package.sh` and `share.sh` again, and everyone re-pastes the line |
