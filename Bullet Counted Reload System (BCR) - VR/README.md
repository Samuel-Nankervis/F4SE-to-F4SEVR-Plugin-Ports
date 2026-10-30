# Bullet Counted Reload System (BCR) - Fallout 4 VR Port

A native F4SEVR port of **Bullet Counted Reload System (BCR)** by *shavkacagarikia* for **Fallout 4 VR (Runtime 1.2.72.0 / F4SEVR 0.6.21)**.

BCR replaces whole-magazine reload animations with iterative, bullet-by-bullet reload animations for lever-action rifles, shotguns, revolvers, and pump-action weapons. Only the exact number of rounds missing from the chamber/magazine are loaded, and the reload can be interrupted at any time.

---

## Features

- **Bullet-by-Bullet Reloading**: Firing 1 shot loads 1 round; firing all rounds performs a full sequential reload.
- **VR Motion Controller Interruption**:
  - Pulling the weapon hand **Trigger** (Fire) or pressing the **Reload button** (A button on modern VR controllers) cleanly interrupts the reload animation.
  - Automatically finishes chambering the active round and returns directly to the ready-to-fire stance.
- **Automatic Controller Detection**:
  - Automatically identifies **Valve Index (Knuckles)**, **Meta / Oculus Touch (Quest 1/2/3/Pro, Rift S)**, **PlayStation VR2 Sense**, **Steam Frame**, **HTC Vive Wands**, and **Windows Mixed Reality (WMR)** without requiring manual keybinding adjustments.
  - Off-hand controller inputs (locomotion, jumping, Pip-Boy, favorites, off-hand grip) are strictly isolated and will never accidentally interrupt a reload.
  - Natural hand grip and touchpad contact on the weapon hand are protected against accidental reload cancellation.
- **Left-Handed Mode Support**:
  - Automatically respects Fallout 4 VR's `bLeftHandedMode` setting from `fallout4prefs.ini`, or can be explicitly forced in `BulletCountedReload.ini`.
- **Live Memory Resolution**:
  - Directly queries live loaded and reserve ammo counts from the engine's `middleProcess`, bypassing VR-specific event dispatch omissions.
- **Zero Performance Overhead**:
  - Quiet logging by default; no disk I/O frame drops during gameplay.

---

## Requirements

1. **Fallout 4 VR** (Steam runtime version `1.2.72.0`)
2. **F4SEVR** version `0.6.21` (or compatible)
3. **Fallout 4 VR Tools** (`FO4VRTools.dll`) - Recommended for OpenVR motion controller input hooks.

---

## Installation

### Via Mod Organizer 2 or Vortex
1. Download `BulletCountedReload-v2.05-VR.zip`.
2. Install the archive through Mod Organizer 2 or Vortex as a standard mod.
3. Ensure the mod is enabled in your mod list.

### Manual Installation
Extract the archive into your Fallout 4 VR `Data` directory so that:
```text
Data/
├── BCR/
│   └── WhatsThis.txt
└── F4SE/
    └── Plugins/
        ├── BulletCountedReload.dll
        └── BulletCountedReload.ini
```

---

## Configuration (`BulletCountedReload.ini`)

An optional configuration file is provided at `Data/F4SE/Plugins/BulletCountedReload.ini`:

```ini
[VR]
; Enable VR reload interruption (1 = enabled, 0 = disabled)
bEnableVRInterrupt = 1

; Weapon Hand:
; 0 = Auto-detect (automatically matches Fallout 4 VR left-handed mode setting)
; 1 = Force Right Hand as weapon hand
; 2 = Force Left Hand as weapon hand
iWeaponHand = 0

; Controller Type:
; 0 = Auto-detect (Valve Index, Oculus/Quest Touch, PSVR2, Steam Frame, Vive Wand, WMR)
; 1 = Valve Index (Knuckles)
; 2 = Meta / Oculus Touch (Quest 1/2/3/Pro, Rift S)
; 3 = HTC Vive Wand
; 4 = PlayStation VR2 Sense
; 5 = Steam Frame
; 6 = Windows Mixed Reality
iControllerType = 0

; Buttons on the WEAPON HAND that interrupt a reload (1 = enabled, 0 = disabled):
; NOTE: All off-hand buttons are ALWAYS ignored.
bInterruptOnTrigger = 1
bInterruptOnReloadButton = 1
bInterruptOnGrip = 0
bInterruptOnTouchpad = 0
bInterruptOnAltButton = 0

; Analog trigger sensitivity threshold (0.10 to 0.90, default 0.50)
fTriggerThreshold = 0.50

; Verbose debug logging in BulletCountedReload.log (1 = detailed animation logging, 0 = quiet/performance mode)
bVerboseLogging = 0
```

---

## Adding Weapons to BCR

To make a weapon use bullet counted reload:
1. Place a `.ini` file in `Data/BCR/` named after the mod plugin (e.g. `DLCCoast.esm.ini` for Far Harbor's Lever-Action Rifle).
2. Inside the INI, specify the weapon FormIDs under the `[Weapons]` section:
   ```ini
   [Weapons]
   weaponFormId_LeverActionRifle = 2c8b0
   ```
3. If the weapon has a bolt-charge / post-reload anim, append `(PostReloadAnim)` to the key:
   ```ini
   weaponFormId_ExampleGun(PostReloadAnim) = 12345
   ```

---

## Credits

- **shavkacagarikia**: Original Bullet Counted Reload System author.
- **FO4VR / F4SEVR Team**: F4SEVR and VR reverse-engineering foundation.
- **lfrazer**: FO4VRTools author.
- Ported to Fallout 4 VR with Advanced Agentic Coding by Google DeepMind.
