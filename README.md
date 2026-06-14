# SimultaneousInput

An SFSE plugin that lets you use **mouse and gamepad input at the same time** in
Starfield.

By default Starfield switches its entire input system between "mouse/keyboard"
and "gamepad" mode and ignores whichever device you used last. This breaks setups
where an analog stick (e.g. an azeron-style keypad or a controller) drives
movement while the **mouse** aims: the moment the stick moves, the game stops
accepting mouse look until you press a keyboard/mouse key again.

This plugin keeps both alive — walk and adjust speed with the analog stick **and**
look/aim with the mouse simultaneously.

## Compatibility

- **Starfield 1.16.244** (resolved through the Address Library at runtime, so it
  is not pinned to a single build, but only 1.16.244 has been tested for this
  release).
- Requires [SFSE](https://www.nexusmods.com/starfield/mods/106) and the
  [Address Library for SFSE Plugins](https://www.nexusmods.com/starfield/mods/3256).

## Installation

Install with a mod manager (Mod Organizer 2 / Vortex) or manually by copying
`SimultaneousInput.dll` to:

```
Starfield/Data/SFSE/Plugins/SimultaneousInput.dll
```

Then launch the game through SFSE.

## How it works

1. **LookHandler::ShouldHandleEvent** (vtable slot 1) is wrapped so a *Look* event
   from either the mouse or a thumbstick is accepted, instead of only the
   currently-active device's.
2. **BSPCGamepadDevice::Poll** is patched so moving the stick no longer latches
   the active input device to the gamepad. The game stays in mouse mode for look
   processing (mouse aiming keeps mouse sensitivity and orientation) while the
   stick still drives analog movement.

## Building

```sh
git clone --recurse-submodules https://github.com/Platin117/Starfield-SimultaneousInput
cd Starfield-SimultaneousInput
cmake --preset vs-windows
cmake --build build --config Release
```

Requires a C++23 MSVC toolchain and vcpkg (wired up via the CMake preset).

## Credits

- **Parapets / [Exit-9B](https://github.com/Exit-9B)** — original author of
  SimultaneousInput.
- **[FullStack0verfl0w](https://github.com/FullStack0verfl0w)** — fork this one is
  based on; kept the mod updated for an earlier Starfield version.
- **[gitlostinit](https://github.com/gitlostinit)** — reverse-engineering and
  Address Library IDs for current Starfield builds.
- **[Platin117](https://github.com/Platin117)** — fix and update for Starfield
  1.16.244, with [Claude Code](https://claude.com/claude-code).

## License

GPL-3.0 (see [COPYING](COPYING) and [EXCEPTIONS](EXCEPTIONS)), inherited from the
original project.
