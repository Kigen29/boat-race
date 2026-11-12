# Getting four people into a race

## The short version

1. One person is the **host**. They build the game once and send a copy to the
   other three.
2. Everyone runs the copy. The host clicks **Host**; the others click **Join**
   and type the host's address.
3. The host presses **ENTER** to start.

## What each player needs

| | Host | The other three |
|---|---|---|
| Linux | Yes | Yes |
| A graphics card with Vulkan | Yes | Yes |
| Unreal Engine installed | Yes | **No** |
| Disk space | ~60 GB (the engine) | ~1 GB |

Check any machine with `vulkaninfo --summary` (install `vulkan-tools` first).
If it prints a GPU, it can probably run the game. Integrated Intel or AMD
graphics will work on the low preset; a dedicated card is much better.

**Everyone has to be on Linux.** Unreal cannot build a Windows copy from a
Linux machine. If a friend is on Windows, someone with Windows, Unreal Engine
and Visual Studio has to package it there from the same project folder.

## Step 1 — The host builds the shareable copy

```bash
cd ~/Downloads/BoatRace
Scripts/build.sh
Scripts/fetch_assets.sh
Scripts/import_assets.sh
Scripts/package.sh
```

`package.sh` takes a while and produces `dist/BoatRace-Linux.tar.gz`. That one
file is what the others need — it runs without Unreal Engine installed.

## Step 2 — Send it to the other three

Anything that moves a ~1 GB file: a USB stick, Google Drive, WeTransfer, or on
the same network:

```bash
# on the host, from the dist folder
python3 -m http.server 8000
```

Then the others open `http://<host address>:8000` in a browser and download it.

Each of them unpacks it:

```bash
tar -xzf BoatRace-Linux.tar.gz
cd LinuxClient        # or whatever folder appears
./BoatRace.sh
```

## Step 3 — Find the host's address

On the host:

```bash
hostname -I | awk '{print $1}'     # same wifi or LAN, e.g. 192.168.1.20
curl -s ifconfig.me                 # over the internet
```

## Step 4 — Open the door

On the same network, the host allows the game's port:

```bash
sudo ufw allow 7777/udp
```

Over the internet, the host also forwards **UDP port 7777** on their router to
their computer. Router pages differ; look for "Port Forwarding" or "NAT". If
that is not possible, use a free virtual-LAN tool such as ZeroTier or Tailscale
on all four machines and use the address it gives the host.

## Step 5 — Play

| Who | What they do |
|---|---|
| Host | `./host.sh`, or run the game and press **H** |
| Everyone else | `./join.sh 192.168.1.20`, or run the game, press **J**, type the address, press ENTER |
| Host, once the lobby shows everyone | Press **ENTER** |

The lobby counts players, so the host can see when all four have arrived. A
fifth connection is refused.

## Controls

| Key | Action |
|---|---|
| W / S | Throttle / reverse |
| A / D | Steer |
| SPACE or left mouse | Fire a torpedo |
| Q or right mouse | Drop a mine |
| R | Back to your last gate |
| ENTER | Host only: start the race |
| L | Leave |

Gamepad: right trigger throttle, left trigger reverse, left stick steer,
**A** torpedo, **X** mine, **Y** reset, **Start** begin.

## How a race works

- Two laps of a 3 km circuit, sixteen gates, 90 m wide between the buoys.
- Pass the gates **in order**. Orange buoys mark the inside of the course,
  white the outside, and the start line is the pair of tall yellow ones. Your
  next gate pulses and the needle at the top of the screen points at it.
- Shooting is a racing tactic, not a kill count. Wrecking someone costs them
  about eight seconds: four drifting, plus the run back up to speed.
- Your own blast cannot damage you, but it can shove you off line.
- Ammunition is in the floating crates on the course. They come back 15 seconds
  after being taken.
- Watch for the rock spires, the two barges crossing the course, the ramps and
  the three whirlpools.

## When something goes wrong

| Problem | Cause and fix |
|---|---|
| Joining drops straight back to the menu | Wrong address, firewall, or port 7777 not forwarded. Check the host's address again |
| "This race is full" | Four are already in |
| Boats jitter or rubber-band | Poor connection to the host. The host should use a wired connection if possible |
| Everything is slow | Run `Scripts/low_spec.sh` before packaging, then rebuild and resend |
| The host leaves | The race ends for everyone. The host has to be the most stable machine |
| One player sees different weather or a different sea | Their `Config/DefaultGame.ini` differs. All four must use the same copy |
