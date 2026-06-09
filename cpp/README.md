# LAN Games Deployer C++ (Native)

This is a native Win32 C++ build focused on speed and old-PC compatibility.

## Build

```powershell
cd cpp
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Output:
- `cpp/build/Release/LANGamesDeployerCpp.exe`

## XP 32-bit target

For Windows XP 32-bit support, use an older VS toolset that supports XP:

```powershell
cmake -S . -B build_xp -G "Visual Studio 16 2019" -A Win32 -T v141_xp
cmake --build build_xp --config Release
```

Notes:
- Modern Windows SDK/toolsets may not run on XP.
- Prefer static runtime for minimal dependencies.

## Redistributables

- This project is configured for static runtime (`/MT`), so no VC++ redistributable is needed.
- For XP-era machines, you may still need legacy platform updates depending on OS/service pack state.

## Current feature status

- `1` Static runtime: done (`/MT` in CMake).
- `2` LAN service skeleton: done.
  - UDP broadcast discovery thread added.
  - HTTP share server added with `/manifest`.
- `3` Local game scanning/assets groundwork: partial.
  - Fast local scanning from `data/config.ini` `shared_root=...`
  - Local `PLAY` launch from discovered EXE.
  - Asset rendering and GET/OPEN shared-file panel are next.
