# DOS 7.1 Native Overlay Launcher for Windows

A lightweight, fullscreen DOS overlay launcher for Windows written in modern **C++20 (WinAPI)** with seamless **otvdm (WineVDM)** integration for running 16-bit DOS applications.

## Features
- **All-in-One Standalone Binary:** Embeds the splash screen, virtual drive archive, and 16-bit compatibility layer directly into a single `.exe` via `RCDATA` resources.
- **Smart Auto-Installer:** Automatically detects the Windows UI language (`EN` / `RU`) and offers to unpack and register `otvdm` if it is not installed on the host machine.
- **Authentic Retro Experience:** Displays a fullscreen boot splash (`LOGO.SYS`), renders console text using the classic raster `Terminal` (IBM VGA) font, and uses native `Segoe UI` dialogs.
- **Virtual `A:` Drive:** Extracts the environment into a hidden temporary directory, mounts it as a virtual `A:\` drive, and cleans up everything automatically upon `EXIT`.

## Building from Source (Visual Studio)
> **Note:** This repository contains only the open-source C++ launcher code. You must provide your own legal DOS binaries (such as **FreeDOS**) and `otvdm` build before compiling.

1. Place the following resource files into the project directory:
   - `msdos.ico` — Application icon
   - `black_bg.bmp` — Fullscreen background bitmap
   - `msd.zip` — Archive containing your DOS environment (e.g., FreeDOS utilities, `LOGO.SYS`)
   - `otvdm.zip` — Archive containing the open-source `otvdm` (WineVDM) binaries
2. Open the `.sln` solution in **Visual Studio 2022**.
3. Set the configuration to **Release**, C++ Language Standard to **ISO C++20 (`/std:c++20`)**, and Runtime Library to **Multi-threaded (`/MT`)**.
4. Build the solution (`Ctrl + Shift + B`).

## License
This project is licensed under the **MIT License**.
