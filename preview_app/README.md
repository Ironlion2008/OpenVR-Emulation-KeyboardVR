# SteamVR Mirror Preview

This is a small standalone Windows desktop app that connects to SteamVR as an OpenVR client and mirrors a single compositor eye into a normal Win32 window.

The previewed eye is hardcoded at the top of `SteamVRMirrorPreview.cpp` with `kPreviewEye`, which currently uses the left eye.

## Build

From the repository root:

```
cmake -S preview_app -B preview_app/build -G "Visual Studio 18 2026" -A x64
cmake --build preview_app/build --config Release --target steamvr_mirror_preview
```

## Run

Start SteamVR, then run:

```
.\preview_app\build\Release\steamvr_mirror_preview.exe
```
