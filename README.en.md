# WallpaperManager

[English](README.en.md) | [简体中文](README.md)

A wallpaper manager for Windows written in pure C (Win32 API) — a modular refactor of the original `WallpaperManager.Bat` (824-line batch script), generated with the **DeepSeek v4 Pro** reasoning model.

> **Windows-only**. Tested on Windows 11 24H2 (running as Administrator).

- **Wide-char (UTF-16)**: full support for Chinese, Korean, Japanese, Thai, emoji, and all Unicode characters; no 8.3 short-name dependency.
- **Hidden execution**: compiled as a GUI-subsystem program (no console window); naturally hidden when invoked by the context menu / scheduled task — no mshta / PowerShell wrapper.
- **Zero external dependencies**: wallpaper application, task scheduling, and context menus all call Win32 APIs directly — no mshta / PowerShell, no temp files / XML files.

---

## Features

| Requirement | Implementation |
|---|---|
| Modular development | Nine modules: `main` / `common` / `log` / `config` / `filelist` / `wallpaper` / `dialog` / `registry` / `task` |
| No 8.3 short-name dependency | Wide-char full paths throughout (`FindFirstFileW`, etc.) |
| Modern dialogs | `IFileOpenDialog` (file/folder picker), `TaskDialogIndirect` (error/info), custom modal numeric input |
| Full logging | `WallpaperManager.log` (UTF-8; records every command, result, and error) |
| Code validity | `-Wall -Wextra` zero warnings + clang static analysis zero alerts |
| Hidden execution | GUI subsystem (`-mwindows`), no CLSID needed |
| No temp/XML files | In-memory `qsort` file sorting; task scheduling via COM |
| Crossfade transition | Old wallpaper fades out / new fades in on switch (GDI+ in-memory rendering + layered window, wallpaper layer only) |

Supported image types: `Jpg` `Jpeg` `Png` `Bmp` `Gif` `Tif` `Tiff` `Heic` `Heif` `Avif`.

---

## Build

Toolchain: **LLVM-MinGW UCRT** (clang, ≥ 18 recommended; this project verified on 22.1.8).

```bat
build.bat
```

Or the equivalent manual command:

```bat
llvm-windres app.rc app.res.o
clang -std=c11 -O2 -mwindows -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -Wall -Wextra ^
  main.c common.c log.c config.c filelist.c wallpaper.c dialog.c registry.c task.c app.res.o ^
  -o WallpaperManager.exe ^
  -lole32 -loleaut32 -lcomctl32 -lshell32 -ladvapi32 -luser32 -lgdi32 -lgdiplus -luuid -ltaskschd
```

The output `WallpaperManager.exe` is an `IMAGE_SUBSYSTEM_WINDOWS_GUI` program — no console window at runtime.

---

## Usage

### Install / Uninstall

```bat
WallpaperManager.exe Install      rem installs context menu (image/directory/desktop)
WallpaperManager.exe UnInstall    rem removes menu, scheduled task, and config
```

> `Install` writes the image context menu (HKLM) and the desktop menu; run as Administrator for a full install.

> Running `WallpaperManager.exe` with no arguments shows the prompt 「请查阅相关指令后以命令行的方式运行本程序。」 — use the command-line interface below.

### Context-menu commands

- **Image file** → `设为壁纸背景` (file mode, fixed wallpaper)
- **Directory** → `设为壁纸背景` (folder mode, auto-rotation)
- **Desktop right-click**:
  - `切换壁纸` → next / previous, set switch interval, set switch mode
  - `切换目录` → previous / next folder (multi-folder mode)
  - `壁纸管理器` → locate wallpaper, freeze / unfreeze, hide current wallpaper, set current mode

### Direct command line (path dispatch)

```bat
WallpaperManager.exe <directory path>   rem equivalent to Folder_Mode
WallpaperManager.exe <image path>       rem equivalent to File_Mode
WallpaperManager.exe Toggle_File Next|Last
WallpaperManager.exe Toggle_Folder Next|Last
WallpaperManager.exe Toggle_Mode N|D|S|J|Z
WallpaperManager.exe Toggle_Time
WallpaperManager.exe Freeze_Wallpaper
WallpaperManager.exe Hidden_Wallpaper
WallpaperManager.exe Locate_Wallpaper
```

---

## Switch modes

A switch mode is an initial letter + direction (`toggle_mode`, e.g. `N+`, `D-`, `Z`):

| Letter | Meaning |
|---|---|
| `N` | Sort by name |
| `D` | Sort by date |
| `S` | Sort by size |
| `J` | Random (fixed order) |
| `Z` | Random (different each time) |

| Direction | Meaning |
|---|---|
| `+` | Ascending / forward |
| `-` | Descending / backward |

---

## Configuration file

`WallpaperManager.cfg` (UTF-8, in the same directory as the exe):

```ini
[state]
mode = Single Folder Mode          ; Single Folder Mode / More Folder Mode / File Mode
toggle_interval = 10               ; auto-switch interval (minutes, 1-999)
toggle_mode = N+                   ; switch mode
current_id = 1                     ; current folder index (multi-folder mode)
file_index = 0                     ; current file index
folder_index = 0
current_file = xxx.jpg             ; current wallpaper filename
current_folder = xxx               ; current folder name
root_path = C:\...                 ; root path

[folders]
C:\...\folder1                     ; multi-folder mode: one image folder per line

[hidden]
xxx.jpg                           ; one hidden filename per line
```

---

## Directory structure

```
WallpaperManager/
├── main.c          # entry point (WinMain) and command dispatch
├── common.c/.h     # memory / string / path / UTF-8 / RNG / process lock
├── log.c/.h        # UTF-8 full logging
├── config.c/.h     # config read/write
├── filelist.c/.h   # file scan, sorting, recursive folder collection
├── wallpaper.c/.h  # wallpaper application (desktop + lock screen + crossfade)
├── dialog.c/.h     # modern dialogs (picker / prompt / numeric input)
├── registry.c/.h   # context-menu install / uninstall
├── task.c/.h       # task scheduling (Task Scheduler 2.0 COM)
├── app.manifest    # comctl32 v6 visual styles + TaskDialog
├── app.rc          # resource script (embedded manifest)
├── build.bat       # build script
└── WallpaperManager.exe
```

---

## Technical notes

- **Wallpaper application**: writes the registry first (desktop HKCU `Control Panel\Desktop\Wallpaper`, lock screen HKLM `PersonalizationCSP`), then calls `SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, path, SPIF_SENDWININICHANGE)` to apply immediately (no win.ini write).
- **Crossfade transition**: on switch, the old wallpaper is rendered via GDI+ to a layered overlay window (placed above the wallpaper layer, below icons/apps); the new wallpaper is applied, then the overlay fades out — only the wallpaper layer is affected. Rendering faithfully replicates the system "Fill" positioning (vertical `dy=(H-dh)/3`, horizontally centered `dx=(W-dw)/2`) to avoid vertical jitter during the transition.
- **Task scheduling**: Task Scheduler 2.0 COM (`ITaskService`), no XML, no `schtasks`. The `WallpaperManager` task runs `WallpaperManager.exe Toggle_File Next` every N minutes with `InteractiveToken` / highest privileges.
- **Freeze wallpaper**: `Freeze_Wallpaper` toggles the scheduled task on/off; the menu label switches between 「冻结当前壁纸」/「取消冻结壁纸」.
- **Context menu**: `HKCR\DesktopBackground\Shell\ToggleWallpaper` and `WallpaperManager` use cascading `SubCommands`; directory/image menus are written to `HKCR\Directory\shell\WallpaperManager` and `HKLM\SOFTWARE\Classes\SystemFileAssociations\.<ext>\Shell\WallpaperManager` respectively.
- **Efficiency**: switching files does not reinstall the context menu (menu content only depends on mode/switch-mode/interval/freeze state); scheduled rotation does only "read config → scan folder → sort → set wallpaper → save config" with no registry-write overhead.

---

## Notes

- The lock-screen wallpaper is written to HKLM; when not running as Administrator this part degrades automatically (logged as a warning), and the desktop wallpaper is unaffected.
- If the system Task Scheduler service is disabled, auto-rotation is unavailable (logged as a warning); manual switching still works.
- Uninstall (`UnInstall`) restores the system default "Set as desktop background" menu item.
