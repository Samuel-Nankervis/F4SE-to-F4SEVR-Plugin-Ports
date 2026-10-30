# Remove Ammo from Dropped Guns - VR

A Fallout 4 VR F4SE plugin that automatically removes loaded ammunition from weapons dropped by deceased NPCs and transfers it directly into their corpse inventory.

In vanilla Fallout 4 VR, when enemies die and drop their guns, the loaded magazine ammo stays trapped inside the dropped physical gun on the ground. Players are forced to pick up heavy weapons and drop them from their Pip-Boy just to harvest the ammunition. This plugin completely eliminates that chore in VR!

---

## Features

- **Automated Ammo Extraction**: When an NPC dies, their equipped weapon's chamber/magazine ammo is automatically unloaded and transferred to their corpse.
- **VR Optimized**: Natively compiled for Fallout 4 VR 1.2.72 (F4SEVR 0.6.21+).
- **Zero Combat Hitching / Stutter-Free**: Direct lock-free memory traversal and stack-buffered logging prevent main-thread deadlocks and eliminate disk I/O in combat.
- **Safe Fallback Handling**: Uses native Creation Engine `AIProcess` functions to cleanly handle humanoids, creatures (dogs, radroaches, deathclaws), heavy weapons, and custom mod weapons.
- **Flexible Configuration**: Full control over weapon ammo percentage and corpse inventory ammo percentage via `AmmoRemover.ini`.

---

## Requirements

1. **Fallout 4 VR** (v1.2.72.0)
2. **F4SEVR** (v0.6.21 or newer)

---

## Installation

### Mod Organizer 2 / Vortex (Recommended)
1. Download `AmmoRemover-v1.2.6-VR.zip`.
2. Install the archive in your mod manager.
3. Enable the mod.

### Manual Installation
Extract the contents of the `.zip` archive into your Fallout 4 VR `Data` directory:
```
Fallout 4 VR/
└── Data/
    └── F4SE/
        └── Plugins/
            ├── AmmoRemover.dll
            ├── AmmoRemover.ini
            └── AmmoRemoverDefaults.ini
```

---

## Configuration

Settings can be customized in `Data/F4SE/Plugins/AmmoRemover.ini` (or `%USERPROFILE%/Documents/My Games/Fallout4VR/F4SE/AmmoRemover.ini`):

```ini
[Config]
; Percentage of loaded weapon ammo to transfer to corpse inventory upon death (0 - 1000)
; Default: 100 (100% of weapon magazine is unloaded and moved to corpse inventory)
; Set to 0 if you want dropped weapons to have 0 ammo without giving it to the corpse.
Weapon Ammo % = 100

; Percentage of reserve inventory ammo remaining on corpse upon death (0 - 1000)
; Default: 100 (100% of corpse inventory ammo is preserved)
; Set to 0 for a hardcore survival experience where NPCs drop no reserve ammo.
Inventory Ammo % = 100

; Logging Level (0 - 5)
; 0 = Fatal Error, 1 = Error, 2 = Warning, 3 = Message, 4 = Verbose, 5 = Debug
; Default: 3 (Logs startup status only; recommended for zero-stutter combat performance)
Log Level = 3
```

---

## Troubleshooting & Logs

Log files are written to:
`%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\AmmoRemover.log`

If you ever need to inspect detailed per-kill ammunition transfers, set `Log Level = 4` in `AmmoRemover.ini`.

---

## Credits & License

- Original mod by **ScottyDoesKnow** ([Nexus Mods / GitHub](https://github.com/ScottyDoesKnow/AmmoRemover))
- VR Port & Optimizations by **Antigravity / DeepMind**
- Licensed under the **MIT License**.
