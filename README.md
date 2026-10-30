# Fallout 4 VR - F4SE Plugin Ports

A collection of essential Fallout 4 Script Extender (F4SE) plugins natively ported, bug-fixed, and performance-optimized for **Fallout 4 VR** (Runtime `1.2.72.0` / F4SEVR `0.6.21+`).

---

## Included Plugins

### 1. Bullet Counted Reload System (BCR) - VR (v2.05)
A complete port of **Bullet Counted Reload System** by *shavkacagarikia* to Fallout 4 VR.

- **True Round-by-Round Reloading**: Weapons like the Far Harbor Lever Action Rifle or custom pump-action shotguns insert only as many cartridges as needed to top off the magazine.
- **VR Engine Chambering Fix**: Dynamically reads live loaded ammo and inventory reserve to prevent premature reload cancellation and animation mismatches.
- **Motion Controller Reload Interruption**: Full integration with **FO4VRTools** (`OpenVRHookManagerAPI`):
  - **Weapon Hand Isolation**: Strictly isolates input to the primary weapon hand (`bLeftHandedMode` aware). Off-hand gestures, grip-squeezes, and thumbstick locomotion will never cancel a reload.
  - **Automatic Controller Detection**: Auto-detects and custom-tunes input masks for **Valve Index Knuckles**, **Meta Quest (1 / 2 / 3 / Pro)**, **HTC Vive Wands**, **PlayStation VR2**, and **Steam Frame**.
  - **Zero Frame Drops**: Quiet by default (`bVerboseLogging = 0`) to prevent VR headset stutter.
- **Pre-packaged Release**: Available in `Bullet Counted Reload System (BCR) - VR/BulletCountedReload-v2.05-VR.zip`.

---

### 2. Remove Ammo from Dropped Guns - VR (v1.2.6)
A complete port of **Remove Ammo from Dropped Guns** by *ScottyDoesKnow* to Fallout 4 VR.

- **Automated Ammo Extraction**: When an NPC dies and drops their weapon, the loaded chamber/magazine ammo is automatically extracted and placed into their corpse inventory.
- **Deadlock & Freeze Resolution**: Re-architected with direct native `AIProcess` queries (`0x0E803D0`, `0x0EC48C0`, `0x0EC4B90`) and lock-free inventory traversal, completely eliminating the Creation Engine `BSReadWriteLock` recursion deadlocks and CTDs present in the desktop version.
- **Zero Combat Overhead**: All in-combat logging is gated behind `Verbose` (level 4). At default `Log Level = 3`, death event execution completes in under 0.01 ms with **zero disk writes** during combat.
- **Crash-Proof Resolution**: Robust Structured Exception Handling (`__try` / `__except`) guarantees stability even with custom modded creatures and weapons.
- **Pre-packaged Release**: Available in `Remove Ammo from Dropped Guns - VR/AmmoRemover-v1.2.6-VR.zip`.

---

## Tools and Build Environment

This repository does not contain all supporting libraries, SDKs, and source dependencies required to build and maintain F4SEVR plugins:

- **`F4SE Source/F4SEVR/f4sevr_0_6_21/`**: The complete Fallout 4 VR Script Extender source code and pre-compiled static libraries (`f4se_common.lib`, `common_vc11.lib`).
- **`Address Library Source/`**: Complete database files (`fo4_database.csv`, `VR/version-1-2-72-0.csv`, `version-1-2-72-0.bin`) for reverse-engineering and symbol resolution.
- **`FO4VRTools/`**: Full source code for FO4VRTools by *lfrazer*, providing the OpenVR hook manager API for motion controller input intercepting.

---

## Directory Structure

```
├── Bullet Counted Reload System (BCR) - VR/     # BCR VR source code, project, & packages
│   ├── bin/Release/                             # Built binaries (BulletCountedReload.dll)
│   ├── package/                                 # Ready-to-use mod package layout
│   ├── src/                                     # BCR C++ source code & headers
│   ├── BulletCountedReload.vcxproj              # Visual Studio 2022 project
│   ├── BulletCountedReload-v2.05-VR.zip         # End-user release archive
│   └── README.md
├── Remove Ammo from Dropped Guns - VR/          # Ammo Remover VR source & packages
│   ├── bin/Release/                             # Built binaries (AmmoRemover.dll)
│   ├── include/                                 # Header files
│   ├── package/                                 # Ready-to-use mod package layout
│   ├── src/                                     # C++ source files
│   ├── AmmoRemover.sln                          # Visual Studio solution
│   ├── AmmoRemover.vcxproj                      # Visual Studio 2022 project
│   ├── AmmoRemover-v1.2.6-VR.zip                # End-user release archive
│   └── README.md
├── .gitignore
└── README.md
```

---

## Building from Source

### Prerequisites
- Visual Studio 2022 (Community edition or higher) with the **Desktop development with C++** workload.
- Windows 10 / 11 SDK (10.0.x).
- **`F4SE Source/F4SEVR/f4sevr_0_6_21/`**: The complete Fallout 4 VR Script Extender source code and pre-compiled static libraries (`f4se_common.lib`, `common_vc11.lib`).
- **`Address Library Source/`**: Complete database files (`fo4_database.csv`, `VR/version-1-2-72-0.csv`, `version-1-2-72-0.bin`) for reverse-engineering and symbol resolution.
- **`FO4VRTools/`**: Full source code for FO4VRTools by *lfrazer*, providing the OpenVR hook manager API for motion controller input intercepting.

### Steps
1. Clone this repository:
   ```bash
   git clone https://github.com/Samuel-Nankervis/F4SE-to-F4SEVR-Plugin-Ports.git
   ```
2. Open either project solution in Visual Studio 2022:
   - For BCR: `Bullet Counted Reload System (BCR) - VR/BulletCountedReload.vcxproj`
   - For Ammo Remover: `Remove Ammo from Dropped Guns - VR/AmmoRemover.sln`
3. Set the build configuration to **Release | x64**.
4. Build the solution. Pre-linked F4SEVR dependencies in `F4SE Source/` will link automatically.
5. Built binaries will be output to `bin/Release/`.

---

## Installation for Players

### Mod Organizer 2 / Vortex
Install the pre-built `.zip` archives directly via your mod manager:
1. `BulletCountedReload-v2.05-VR.zip`
2. `AmmoRemover-v1.2.6-VR.zip`

### Manual Installation
Extract each `.zip` archive into your Fallout 4 VR game folder:
`Fallout 4 VR/Data/`

---

## Credits & Acknowledgments

- **shavkacagarikia** – Original author of [Bullet Counted Reload System](https://www.nexusmods.com/fallout4/mods/41178).
- **ScottyDoesKnow** – Original author of [Remove Ammo from Dropped Guns](https://www.nexusmods.com/fallout4/mods/53018).
- **lfrazer** – Author of [FO4VRTools](https://github.com/lfrazer/FO4VRTools).
- **Ian Patterson, Stephen Abel, Paul Connelly (ianpatt, behippo, scruggsywuggsy99)** – F4SE and F4SEVR developers.
- **meh321** – Author of Address Library for F4SE.
