@echo off
setlocal
cd /d "%~dp0"

rem 定位工具链（优先 PATH，否则使用 winget 安装的 LLVM-MinGW）
set "CLANG="
for %%d in (clang.exe) do set "CLANG=%%~$PATH:d"
if not defined CLANG set "CLANG=C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin\clang.exe"

set "WINDRES="
for %%d in (llvm-windres.exe) do set "WINDRES=%%~$PATH:d"
if not defined WINDRES set "WINDRES=C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin\llvm-windres.exe"

echo [1/3] 编译资源 (manifest)...
"%WINDRES%" app.rc app.res.o || goto :fail

echo [2/3] 编译并链接...
"%CLANG%" -std=c11 -O2 -mwindows -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -Wall -Wextra ^
  main.c common.c log.c config.c filelist.c wallpaper.c dialog.c registry.c task.c app.res.o ^
  -o WallpaperManager.exe ^
  -lole32 -loleaut32 -lcomctl32 -lshell32 -ladvapi32 -luser32 -lgdi32 -lgdiplus -luuid -ltaskschd || goto :fail

echo [3/3] 清理...
del /q app.res.o 2>nul

echo 编译成功: WallpaperManager.exe
exit /b 0

:fail
echo 编译失败
exit /b 1
