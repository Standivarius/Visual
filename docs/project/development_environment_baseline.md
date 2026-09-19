# Visual Development Environment Baseline

**Machine:** MARIUS-DELL
**Date:** 2026-09-19
**Purpose:** Baseline for Spike 1 — Windows Graphics Capture -> D3D11 -> magnified output on a second monitor.

## Current readiness

**READY FOR SPIKE 1 BUILD WORK.**

The native Windows C++ toolchain is installed and verified with a configure/build/run smoke test that includes Windows Graphics Capture interop, C++/WinRT, D3D11 and DXGI.

The remaining environment item is **interactive desktop display-topology verification** before measuring the actual dual-monitor runtime behavior. The repository bridge runs in a background/disconnected desktop context, so its monitor enumeration is not authoritative for the logged-in desktop.

## Verified present

- Windows build: 26200.9445, x64. Registry reports Version 25H2 / Professional edition.
- CPU: Intel Core i5-10310U, 4 cores / 8 logical processors.
- GPU: Intel UHD Graphics.
- GPU driver: 31.0.101.2135, dated 2025-03-06.
- Visual Studio Build Tools 2022: 17.14.41 / installation version 17.14.37710.0.
- Visual Studio Build Tools instance is reported by `vswhere` as complete and launchable.
- MSVC toolset: 14.44.35207.
- MSVC x64 compiler: 19.44.35229.
- Windows SDK: 10.0.26100.0.
- Windows Graphics Capture interop header: `windows.graphics.capture.interop.h`.
- D3D11 header/library: `d3d11.h`, `d3d11.lib`.
- DXGI header/library: `dxgi1_2.h`, `dxgi.lib`.
- Windows application import library: `windowsapp.lib`.
- Visual Studio CMake: 3.31.6-msvc6.
- Visual Studio Ninja: 1.12.1.
- Git: 2.53.0.windows.1.
- Windows PowerShell: 5.1.
- winget: 1.29.290.
- Antigravity CLI: 1.2.7; authenticated Gemini headless delegation verified separately.

## Toolchain locations

- Build Tools: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- MSVC x64 compiler: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`
- Windows SDK root: `C:\Program Files (x86)\Windows Kits\10`
- CMake: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- Ninja: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`

## Verified SDK files

For Windows SDK `10.0.26100.0`:

- `Include\10.0.26100.0\um\d3d11.h`
- `Include\10.0.26100.0\shared\dxgi1_2.h`
- `Include\10.0.26100.0\um\windows.graphics.capture.interop.h`
- `Lib\10.0.26100.0\um\x64\d3d11.lib`
- `Lib\10.0.26100.0\um\x64\dxgi.lib`
- `Lib\10.0.26100.0\um\x64\windowsapp.lib`

## Build smoke test

A temporary CMake project under `artifacts/toolchain-install/smoke/` was configured with the `Visual Studio 17 2022` generator for x64.

The test translation unit includes:

- `<windows.h>`
- `<d3d11.h>`
- `<dxgi1_2.h>`
- `<windows.graphics.capture.interop.h>`
- `<winrt/base.h>`
- `<winrt/Windows.Graphics.Capture.h>`

and links:

- `d3d11`
- `dxgi`
- `windowsapp`

Result:

- CMake configuration succeeded.
- Release executable exists: `artifacts/toolchain-install/smoke/build/Release/visual_toolchain_smoke.exe`.
- Smoke executable ran successfully with exit code `0`.

## Display baseline status

Current bridge/background-session display diagnostics are **not trustworthy** for the interactive desktop. They previously reported a synthetic/disconnected `WinDisc` screen and only the integrated panel through PnP enumeration.

Before collecting Spike 1 performance results, the spike executable itself should enumerate and log the actual interactive desktop:

- physical monitor count and models where available;
- primary monitor;
- desktop geometry/topology;
- resolution;
- refresh rate;
- DPI/scaling;
- orientation;
- HDR state where available;
- adapter/output ownership.

This runtime logging is preferable to treating the background bridge's display enumeration as authoritative.

## Evidence artifacts

- `artifacts/2026-09-19__machine__spike1_environment_raw_v0.1.txt`
- `artifacts/2026-09-19__gemini-3.8-flash-high__spike1_environment_readiness_audit_v0.1.md`
- `artifacts/toolchain-install/verified_environment.txt`
- `artifacts/toolchain-install/smoke/`
- `artifacts/toolchain-install/smoke_run.txt`
