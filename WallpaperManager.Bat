:: ++++++++++++++++++++++++++++++++++++++++++++++++++
::                  WallpaperManager                 
::                 By.Yaw @2026-08-10                
::                                                   
::                    Version: 1.0                   
:: ++++++++++++++++++++++++++++++++++++++++++++++++++
@echo off & title WallpaperManager & chcp 65001>nul & setlocal enabledelayedexpansion

set "TEM_Text=%~dpn0"
set "INI_Text=%~dpn0.ini"
set "XML_Text=%~dpn0.xml"
set "LOG_Text=%~dpn0.log"

set "Type_List=Jpg Jpeg Png Bmp Gif Tif Tiff Heic Heif Avif"

set "DesktopBackground_Reg_Path=HKEY_CURRENT_USER\Control Panel\Desktop"
set "LockScreenBackground_Reg_Path=HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PersonalizationCSP"

set "Folder_WallaperManager_Reg_Path=HKEY_CLASSES_ROOT\Directory\shell\WallpaperManager"
set "DesktopBackground_ToggleWallpaper_Reg_Path=HKEY_CLASSES_ROOT\DesktopBackground\Shell\ToggleWallpaper"
set "DesktopBackground_WallpaperManager_Reg_Path=HKEY_CLASSES_ROOT\DesktopBackground\Shell\WallpaperManager"

chcp 936>nul
set "Select_File_Powershell=powershell -c "Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.Application]::EnableVisualStyles(); $dlg = New-Object System.Windows.Forms.OpenFileDialog; $dlg.ShowHelp = $true; $dlg.Multiselect = $false; $result = $dlg.ShowDialog(); if ($result -eq 'OK') { Write-Output $dlg.FileName }""
set "Select_Folder_Powershell=powershell -c "Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.Application]::EnableVisualStyles(); $dlg = New-Object System.Windows.Forms.FolderBrowserDialog; $result = $dlg.ShowDialog(); if ($result -eq 'OK') { Write-Output $dlg.SelectedPath }""
set "Toggle_Time_Powershell=powershell -c "[System.Reflection.Assembly]::LoadWithPartialName^('Microsoft.VisualBasic'^) ^| Out-Null; $num = [Microsoft.VisualBasic.Interaction]::InputBox^('请输入新的时间^(分钟^) [1-999]：', '设置切换时间', '1'^); if^($num -match '^\d+$' -and [int]$num -ge 1 -and [int]$num -le 999^) { Write-Output $num } else { Write-Output 'Cancel' }""

set /a "TEM_Count=0"
for /f "delims=" %%a in ('tasklist /v /fo csv /nh /fi "IMAGENAME eq cmd.exe"') do (
	set "TEM_String=%%a" & if "!TEM_String!" neq "!TEM_String:WallpaperManager=!" (set /a "TEM_Count+=1")
)
if !TEM_Count! gtr 1 (call :Output_Message "[运行进程错误] : 重复进程冲突" & exit)
chcp 65001>nul

reg add "!LockScreenBackground_Reg_Path!" /v "LogonImageStatus" /t REG_DWORD /d "1" /f >nul 2>nul
reg add "!LockScreenBackground_Reg_Path!" /v "LockScreenImageStatus" /t REG_DWORD /d "1" /f >nul 2>nul

for /f "tokens=2 delims= " %%a in ("!Date!") do (set "Today=%%a" & set "Today=!Today:/=-!")
for /f "tokens=2 delims= " %%a in ('whoami /user') do (set "User_SID=%%a")
for /f "tokens=2 delims= " %%a in ('fsutil 8dot3name query') do (
	if "%%a" neq "0" (fsutil 8dot3name set 0 >nul)
)

REM --------------------------------------------------

:Parameters
echo [!Today! !Time!] : Parameters {%*}>>"!LOG_Text!"

if "%~2" neq "" (set "Parameters=%~2")
if /i "%~1" equ "Install" (call :Install)
if /i "%~1" equ "UnInstall" (call :UnInstall)

if /i "%~1" equ "File_Mode" (call :File_Mode)
if /i "%~1" equ "Folder_Mode" (call :Folder_Mode)

if /i "%~1" equ "Toggle_Time" (call :Toggle_Time)
if /i "%~1" equ "Toggle_Mode" (call :Toggle_Mode)
if /i "%~1" equ "Toggle_File" (call :Toggle_File)
if /i "%~1" equ "Toggle_Folder" (call :Toggle_Folder)

if /i "%~1" equ "Locate_Wallpaper" (call :Locate_Wallpaper)
if /i "%~1" equ "Freeze_Wallpaper" (call :Freeze_Wallpaper)
if /i "%~1" equ "Hidden_Wallpaper" (call :Hidden_Wallpaper)

if exist "%~1" (
	cd /d "%~1" >nul >nul 2>nul && (
		set "Folder_Path=%~1" && call :Folder_Mode
	) || (
		set "File_Path=%~1" && call :File_Mode
	)
)

del /f /q "!TEM_Text!*.TEM" >nul 2>nul
exit

REM --------------------------------------------------

:Install
reg delete "!Folder_WallaperManager_Reg_Path!" /f >nul 2>nul
reg delete "!DesktopBackground_ToggleWallpaper_Reg_Path!" /f >nul 2>nul
reg delete "!DesktopBackground_WallpaperManager_Reg_Path!" /f >nul 2>nul
chcp 936>nul
for %%a in (!Type_List!) do (
	call :Install_File_Menu "%%a"
	reg delete "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%%a\Shell\setdesktopwallpaper" /f >nul 2>nul
)
reg add "!Folder_WallaperManager_Reg_Path!" /v Icon /t REG_SZ /d "imageres.dll,146" /f >nul 2>nul
reg add "!Folder_WallaperManager_Reg_Path!" /v MUIVerb /t REG_SZ /d "设置为桌面壁纸(B)" /f >nul 2>nul
REM reg add "!Folder_WallaperManager_Reg_Path!\Command" /t REG_SZ /d "%~f0 \"%%1\"" /f >nul 2>nul
reg add "!Folder_WallaperManager_Reg_Path!\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 %%1\"\", 0, False:window.close\"^)" /f >nul 2>nul
call :Install_Mouse_Menu "Wa Wb We Wf"
exit /b 0

:Install_File_Menu
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\WallpaperManager" /v Icon /t REG_SZ /d "imageres.dll,146" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\WallpaperManager" /v MUIVerb /t REG_SZ /d "设置为桌面壁纸(B)" /f >nul 2>nul
REM reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\WallpaperManager\Command" /t REG_SZ /d "%~f0 \"%%1\"" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\WallpaperManager\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 %%1\"\", 0, False:window.close\"^)" /f >nul 2>nul
exit /b

REM --------------------------------------------------

:UnInstall
del /f /q "!INI_Text!" >nul 2>nul
del /f /q "!XML_Text!" >nul 2>nul
del /f /q "!LOG_Text!" >nul 2>nul
call :Automated_Tasks "Delete"
reg delete "!Folder_WallaperManager_Reg_Path!" /f >nul 2>nul
reg delete "!DesktopBackground_ToggleWallpaper_Reg_Path!" /f >nul 2>nul
reg delete "!DesktopBackground_WallpaperManager_Reg_Path!" /f >nul 2>nul
for %%a in (!Type_List!) do (
	call :UnInstall_File_Menu "%%a"
	reg delete "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%%a\Shell\WallpaperManager" /f >nul 2>nul
)
exit /b 0

:UnInstall_File_Menu
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\setdesktopwallpaper" /ve /t REG_EXPAND_SZ /d "@%%SystemRoot%%\system32\stobject.dll,-417" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\setdesktopwallpaper" /v MultiSelectModel /t REG_SZ /d "Player" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\setdesktopwallpaper" /v NeverDefault /t REG_SZ /d "" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\setdesktopwallpaper\Command" /ve /t REG_EXPAND_SZ /d "%%SystemRoot%%\Explorer.exe" /f >nul 2>nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\SystemFileAssociations\.%~1\Shell\setdesktopwallpaper\Command" /v DelegateExecute /t REG_SZ /d "{ff609cc7-d34d-4049-a1aa-2293517ffcc6}" /f >nul 2>nul
exit /b

REM --------------------------------------------------

:File_Mode
chcp 936>nul
if not defined File_Path (for /f "delims=" %%a in ('!Select_File_Powershell!') do (set "File_Path=%%~a"))

if defined File_Path (
	call :Get_Status "!File_Path!" && (
		for /f "delims=" %%a in ("!File_Path!") do (
			set "File_Suffix=" & set "File_Path=%%~sa"
			for %%b in (!Type_List!) do (if /i ".%%b" equ "%%~xa" (set "File_Suffix=%%~xa"))
			if defined File_Suffix (
				set "Current_Mode=File Mode"
				set "Current_File_Name=%%~nxa"
				set "Current_Menu_Mode=Wa Wb We Wf"
				set "Current_Root_Path=%%~dpa" & set "Current_Root_Path=!Current_Root_Path:~,-1!"
				for /f "delims=" %%b in ("!Current_Root_Path!") do (
					set "Current_Folder_Name=%%~nxb"
					set "Current_Root_Path=%%~dpb" & set "Current_Root_Path=!Current_Root_Path:~,-1!"
				)
			) else (call :Output_Message "[文件模式错误] : 不支持该文件类型" & exit /b 1)
		)
		
		call :Setting_Mouse_Menu "File"
		call :Install_Mouse_Menu "!Current_Menu_Mode!"
		call :Edit_Config "Create" "Mb Mg Mh Mi"
		call :Automated_Tasks "Change" "Disable"
		call :Applay_Wallpaper "!File_Path!"

	) || (call :Output_Message "[文件模式错误] : 文件数据异常" & exit /b 1)
) else (call :Output_Message "[文件模式错误] : 未选择文件" & exit /b 1)
chcp 65001>nul
exit /b 0

REM --------------------------------------------------

:Folder_Mode
chcp 936>nul
set "Current_Freeze_Name=定格当前壁纸"
if not defined Folder_Path (for /f "delims=" %%a in ('!Select_Folder_Powershell!') do (set "Folder_Path=%%~a"))

if defined Folder_Path (
	call :Get_Status "!Folder_Path!" && (
		for /f "delims=" %%a in ("!Folder_Path!") do (
			set /a "All_Folder_Count=1"
			set /a "Current_ID=1"
			set /a "Current_File_Index=0"
			set /a "Current_Folder_Index=0"
			set /a "Current_Toggle_Time=10"
			set "Current_J_Code=0"
			set "Current_NDS_Code=0"
			set "Current_Toggle_Mode=N+"
			set "Current_Folder_Name=%%~nxa"
			set "Current_Mode=Single Folder Mode"
			set "Current_Menu_Mode=Ta Tc Wa Wb Wc Wd We Wf"
			set "Current_Config_Mode=Ma Mb Mc Md Me Mf Mg Mh Mi Mj Mk D0"
			set "Current_Root_Path=%%~dpa" & set "Current_Root_Path=!Current_Root_Path:~,-1!"
		)

		call :Get_File_Info "!Current_Folder_Name!" "!Folder_Path!"
		call :Get_Config_Info "Current_Toggle_Time" "Current_Toggle_Mode"

		for /f "delims=" %%a in ("!Current_Toggle_Mode!") do (
			for /f "delims=" %%b in ("!Folder_Path!\!Current_%%a!") do (set "File_Path=%%~sb")
			set "Current_File_Name=!Current_%%a!"
		)

		call :Setting_Mouse_Menu "Folder" "!Current_Toggle_Mode!"
		call :Install_Mouse_Menu "!Current_Menu_Mode!"
		call :Edit_Config "Create" "!Current_Config_Mode!"
		call :Automated_Tasks "Create"
		call :Applay_Wallpaper "!File_Path!"

	) || (call :Output_Message "[目录模式错误] : 未找到目录文件" & exit /b 1)
) else (call :Output_Message "[目录模式错误] : 未选择目录" & exit /b 1)
chcp 65001>nul
exit /b 0

REM --------------------------------------------------

:Toggle_Time
chcp 936>nul
set "Current_Freeze_Name=定格当前壁纸"
for /f "tokens=*" %%a in ('!Toggle_Time_Powershell!') do (if "%%a" neq "Cancel" (set "Current_Toggle_Time=%%a"))
if defined Current_Toggle_Time (

	call :Get_Config_Info "Current_Mode" "Current_Toggle_Mode"
	if /i "!Current_Mode!" equ "Single Folder Mode" (
		set "Current_Menu_Mode=Ta Tc Wa Wb Wc Wd We Wf"
	) else (
		set "Current_Menu_Mode=Ta Tb Tc Wa Wb Wc Wd We Wf"
	)

	call :Setting_Mouse_Menu "Folder" "!Current_Toggle_Mode!"
	call :Install_Mouse_Menu "!Current_Menu_Mode!"
	call :Edit_Config "Update" "Current_Toggle_Time"
	call :Automated_Tasks "Create"

) else (call :Output_Message "[切换时间错误] : 未输入新的时间" & exit /b 1)
chcp 65001>nul
exit /b 0

REM --------------------------------------------------

:Toggle_Mode
chcp 936>nul
set "Current_Freeze_Name=定格当前壁纸"
chcp 65001>nul

call :Get_Config_Info "Current_Mode" "Current_Toggle_Time" "Current_Toggle_Mode" "File_Count"
if "!Current_Toggle_Mode:~0,1!" equ "!Parameters!" (
	if "!Current_Toggle_Mode!" neq "!Current_Toggle_Mode:+=!" (
		set "Current_Toggle_Mode=!Current_Toggle_Mode:+=-!"
	) else (
		set "Current_Toggle_Mode=!Current_Toggle_Mode:-=+!"
	)
) else (
	set "Current_Toggle_Mode=!Parameters!!Current_Toggle_Mode:~1!"
)
if "!Current_Toggle_Mode!" equ "!Current_Toggle_Mode:Z=!" (
	if "!Current_Toggle_Mode:~1!" equ "" (
		set "Current_Toggle_Mode=!Current_Toggle_Mode!+"
	)
) else (
	set "Current_Toggle_Mode=!Current_Toggle_Mode:~,1!"
)
if /i "!Current_Mode!" equ "Single Folder Mode" (
	set "Current_Menu_Mode=Ta Tc Wa Wb Wc Wd We Wf"
) else (
	set "Current_Menu_Mode=Ta Tb Tc Wa Wb Wc Wd We Wf"
)
set "Parameters=Next" & set "Current_File_Index=-1"
call :Setting_Mouse_Menu "Folder" "!Current_Toggle_Mode!"
call :Install_Mouse_Menu "!Current_Menu_Mode!"
call :Edit_Config "Update" "Current_File_Index" "Current_Toggle_Mode"
call :Automated_Tasks "Create"
call :Toggle_File
exit /b 0

REM --------------------------------------------------

:Toggle_File
chcp 936>nul
set "Current_Freeze_Name=定格当前壁纸"
chcp 65001>nul

call :Get_Config_Info "Current_ID" "Current_Mode" "Current_File_Index" "Current_Folder_Index" "Current_Toggle_Time" "Current_Toggle_Mode" "ID" "File_Count" "Folder_Path"
for /f "delims=" %%a in ("!Current_ID!") do (
	if "!Current_Toggle_Mode!" neq "!Current_Toggle_Mode:+=!" (
		if /i "!Parameters!" equ "Next" (set /a "Current_File_Index+=1") else (set /a "Current_File_Index-=1")
	) else (
		if /i "!Parameters!" equ "Next" (set /a "Current_File_Index-=1") else (set /a "Current_File_Index+=1")
	)

	if !Current_File_Index! lss 0 (set /a "Current_File_Index=!File_Count_%%a!-1")
	if !Current_File_Index! geq !File_Count_%%a! (set /a "Current_File_Index=0")

	if "!Current_Toggle_Mode!" neq "!Current_Toggle_Mode:Z=!" (
		set /a "Current_File_Index=0"
		set /a "TEM_File_Mode=(!Random!)%%4+1" 2>nul
		set /a "TEM_File_Count=(!Random!)%%!File_Count_%%a!+1" 2>nul
		for %%b in (1:N 2:D 3:S 4:J) do (
			for /f "tokens=1,2 delims=:" %%c in ("%%b") do (
				if "!TEM_File_Mode!" equ "%%c" (set "TEM_File_Mode=%%d")
			)
		)
	) else (
		set "TEM_File_Mode=!Current_Toggle_Mode:~,1!"
		set /a "TEM_File_Count=!Current_File_Index!+1"
	)

	for /l %%b in (1,1,2) do (if "!TEM_File_Count:~2!" equ "" (set "TEM_File_Count=0!TEM_File_Count!"))
	call :Get_Config_Info "!TEM_File_Mode!!TEM_File_Count!"
	for /f "delims=" %%b in ("!TEM_File_Mode!!TEM_File_Count!_%%a") do (
		for /f "delims=" %%c in ("!Folder_Path_%%a!") do (
			for /f "delims=" %%d in ("%%c\!%%b!") do (
				set "Current_Folder_Name=%%~nxc"
				set "Current_File_Name=!%%b!"
				set "File_Path=%%~sd"
			)
		)
	)
)

if /i "!Current_Mode!" equ "Single Folder Mode" (
	set "Current_Menu_Mode=Ta Tc Wa Wb Wc Wd We Wf"
) else (
	set "Current_Menu_Mode=Ta Tb Tc Wa Wb Wc Wd We Wf"
)

call :Setting_Mouse_Menu "Folder" "!Current_Toggle_Mode!"
call :Install_Mouse_Menu "!Current_Menu_Mode!"
call :Edit_Config "Update" "Current_File_Index" "Current_File_Name" "Current_Folder_Name"

if "!Hidden_Code!" equ "0" (goto :Toggle_File)

call :Automated_Tasks "Create"
call :Applay_Wallpaper "!File_Path!"
exit /b 0

REM --------------------------------------------------

:Toggle_Folder
chcp 936>nul
set "Current_Freeze_Name=定格当前壁纸"
chcp 65001>nul

call :Get_Config_Info "Current_ID" "Current_File_Index" "Current_Folder_Index" "All_File_Count" "All_Folder_Count" "File_Count"
if /i "!Parameters!" equ "Next" (
	set /a "Current_ID+=1" & set /a "Current_Folder_Index+=1"
	if !Current_ID! gtr !All_Folder_Count! (set /a "Current_ID=1")
	if !Current_Folder_Index! geq !All_Folder_Count! (set /a "Current_Folder_Index=0")
) else (
	set /a "Current_ID-=1" & set /a "Current_Folder_Index-=1"
	if !Current_ID! leq 0 (set /a "Current_ID=!All_Folder_Count!")
	if !Current_Folder_Index! lss 0 (set /a "Current_Folder_Index=!All_Folder_Count!-1")
)

set /a "Current_File_Index=!All_File_Count!"
call :Edit_Config "Update" "Current_ID" "Current_File_Index" "Current_Folder_Index"
for /f "delims=" %%a in ("!Current_ID!") do (if "!File_Count_%%a!" equ "0" (goto :Toggle_Folder))
set "Parameters=Next"
call :Toggle_File
exit /b 0

REM --------------------------------------------------

:Locate_Wallpaper
chcp 936>nul
call :Get_Reg "!DesktopBackground_Reg_Path!" "WallPaper" "File_Path"
if not defined File_Path (
	call :Output_Message "[定位壁纸位置错误] : 未找到注册表信息"
) else (
	explorer /select,"!File_Path!"
)
chcp 65001>nul
exit /b 0

REM --------------------------------------------------

:Freeze_WallPaper
call :Get_Config_Info "Current_Mode" "Current_Toggle_Time" "Current_Toggle_Mode"
if /i "!Current_Mode!" equ "Single Folder Mode" (
	set "Current_Menu_Mode=Ta Tc Wa Wb Wc Wd We Wf"
) else (
	set "Current_Menu_Mode=Ta Tb Tc Wa Wb Wc Wd We Wf"
)

chcp 936>nul
set "Automated_Tasks_Code=Enable"
set "Current_Freeze_Name=定格当前壁纸"
set "Current_Toggle_Time=!Current_Toggle_Time!"
for /f "tokens=4 delims= " %%a in ('SchTasks /Query /TN "WallpaperManager" 2^>nul') do (
	set "Current_Toggle_Time=X"
	set "Automated_Tasks_Code=Disable"
	set "Current_Freeze_Name=取消当前定格"
)
chcp 65001>nul

call :Automated_Tasks "Change" "!Automated_Tasks_Code!"
call :Setting_Mouse_Menu "Folder" "!Current_Toggle_Mode!"
call :Install_Mouse_Menu "!Current_Menu_Mode!"
exit /b 0

REM --------------------------------------------------

:Hidden_WallPaper
set "Parameters=Next"
call :Get_Config_Info "Current_File_Name"
call :Edit_Config "Edit" "!Current_File_Name!"
call :Toggle_File
exit /b 0

REM --------------------------------------------------

:Get_Status
if exist "%~1" (
	for /f "delims=" %%a in ('dir /a-d /b /s "%~1" 2^>nul') do (
		if %%~za gtr 0 (exit /b 0)
	)
)
exit /b 1

REM --------------------------------------------------

:Get_Reg
for /f "tokens=3* delims= " %%a in ('reg query "%~1" /v "%~2"') do (
	if "%%b" neq "" (set "%~3=%%a %%b") else (set "%~3=%%a")
)
exit /b

REM --------------------------------------------------

:Get_File_Info
call :Get_File_List "%~1" "%~2"
call :Get_Folder_List "%~1" "%~2"
exit /b

:Get_Folder_List
for /f "delims=" %%a in ('dir /ad /b /s "%~2"') do (
	call :Get_Status "%%~a" && (
		if not defined All_File_Count (
			set /a "Current_ID+=1"
			set /a "Current_Folder_Index+=1"
			set "Folder_Path=%%~a"
			set "Current_Folder_Name=%%~nxa"
			set "Current_Root_Path=%%~dpa" & set "Current_Root_Path=!Current_Root_Path:~,-1!"
		)
		set /a "All_Folder_Count+=1"
		set "Current_Mode=More Folder Mode"
		set "Current_Menu_Mode=Ta Tb Tc Wa Wb Wc Wd We Wf"
		call :Get_File_List "%%~nxa" "%%~a"
	)
)
exit /b

:Get_File_List
set /a "Current_Folder_ID+=1"
set /a "Current_File_Count=0"
set /a "Current_Folder_Count=0"
REM 获取 名称/日期/体积 排序的文件列表
for %%a in (N:ON D:OD S:OS) do (
	set /a "TEM_Count=0"
	for /f "tokens=1,2 delims=:" %%b in ("%%a") do (
		for /f "delims=" %%d in ('dir /a-d /b /%%c "%~2" 2^>nul') do (
			REM 获取 名称/日期/体积 排序的首尾文件名称
			if "!Current_NDS_Code!" equ "0" (set "Current_%%b-=%%d")
			if "!Current_%%b+!" equ "" (set "Current_%%b+=%%d")

			set /a "TEM_Count+=1"
			if !TEM_Count! geq 0 (set "TEM_String=%%b00!TEM_Count!")
			if !TEM_Count! geq 10 (set "TEM_String=%%b0!TEM_Count!")
			if !TEM_Count! geq 100 (set "TEM_String=%%b!TEM_Count!")
			echo !TEM_String! : %%d
		)>>"!TEM_Text!-%~1-%%b.TEM"
	)
)

REM 获取 随机(假) 排序的文件列表,当前目录文件数量 和 所有文件数量 以及 名称排序的首个文件名
for /f "delims=" %%a in ('dir /a-d /b "%~2" 2^>nul') do (
	echo !Random!:%%a
	set /a "Current_File_Count+=1" & set /a "All_File_Count+=1"
)>>"!TEM_Text!-%~1-J.TEM.TEM"
set /a "TEM_Count=0"
sort "!TEM_Text!-%~1-J.TEM.TEM">"!TEM_Text!-%~1-J.TEM.TEM.TEM" 2>nul
for /f "tokens=1,2 delims=:" %%a in ('type "!TEM_Text!-%~1-J.TEM.TEM.TEM" 2^>nul') do (
	REM 获取 随机(假) 排序的首尾文件名称
	if "!Current_J_Code!" equ "0" (set "Current_J-=%%b")
	if "!Current_J+!" equ "" (set "Current_J+=%%b")

	set /a "TEM_Count+=1"
	if !TEM_Count! geq 0 (set "TEM_String=J00!TEM_Count!")
	if !TEM_Count! geq 10 (set "TEM_String=J0!TEM_Count!")
	if !TEM_Count! geq 100 (set "TEM_String=J!TEM_Count!")
	echo !TEM_String! : %%b
)>>"!TEM_Text!-%~1-J.TEM"
del /f /q "!TEM_Text!-%~1-J.TEM.TEM" >nul 2>nul & del /f /q "!TEM_Text!-%~1-J.TEM.TEM.TEM" >nul 2>nul
REM 将 名称/日期/体积/随机(假) 的文件列表 合并为一个,并记录文件夹顺序
for %%a in (D S J) do (
	for /f "tokens=1,2 delims=: " %%b in ('type "!TEM_Text!-%~1-N.TEM" 2^>nul') do (
		for /f "tokens=1,2 delims=: " %%d in ('type "!TEM_Text!-%~1-%%a.TEM" 2^>nul') do (
			if "%%~c" equ "%%~e" (echo %%b-%%d   : "%%~c")>>"!TEM_Text!-%~1.TEM"
		)
	)
	del /f /q "!TEM_Text!-%~1-%%a.TEM" >nul 2>nul
	move /y "!TEM_Text!-%~1.TEM" "!TEM_Text!-%~1-N.TEM" >nul >nul 2>nul
)

REM 整合其它数据,写入临时文件中
for /f "delims=" %%a in ('dir /ad /b "%~2"') do (set /a "Current_Folder_Count+=1")
(
	echo ID           = "!Current_Folder_ID!"
	echo File Count   = "!Current_File_Count!"
	echo Folder Count = "!Current_Folder_Count!"
	echo Folder Name  = "%~1"
	echo Folder Path  = "%~2"
	if exist "!TEM_Text!-%~1-N.TEM" (
		echo Folder List  {
		for /f "delims=" %%a in ('type "!TEM_Text!-%~1-N.TEM" 2^>nul') do (echo     %%a)
		echo }
	) else (echo Folder List  {})
)>"!TEM_Text!-%~1.TEM"

REM 获取真随机模式的文件,随机模式+随机数字
if not defined Current_Z (
	set /a "TEM_File_Mode=(!Random!)%%4+1" 2>nul
	set /a "TEM_File_Count=(!Random!)%%!Current_File_Count!+1" 2>nul
	for %%a in (1:N 2:D 3:S 4:J) do (
		for /f "tokens=1,2 delims=:" %%b in ("%%a") do (
			if "!TEM_File_Mode!" equ "%%b" (set "TEM_File_Mode=%%c")
		)
	)
	for /l %%a in (1,1,2) do (
		if "!TEM_File_Count:~2!" equ "" (set "TEM_File_Count=0!TEM_File_Count!")
	)
	for /f "delims=" %%a in ("!TEM_File_Mode!!TEM_File_Count!") do (
		for /f "tokens=1,2 delims=: " %%b in ('type "!TEM_Text!-%~1.TEM"') do (
			set "TEM_Line=%%b:%%c"
			if "!TEM_Line!" neq "!TEM_Line:%%a=!" (set "Current_Z=%%~c")
		)
	)
)

set "Current_J_Code=1"
set "Current_NDS_Code=1"
del /f /q "!TEM_Text!-%~1-N.TEM" >nul 2>nul
echo !TEM_Text!-%~1.TEM>>"!TEM_Text!-Sort.TEM"
exit /b

REM --------------------------------------------------

:Get_Config_Info
for %%a in (%*) do (
	set "TEM_String=%%~a" & set "TEM_String=!TEM_String:_= !"
	call :Get_Config_String "%%~a" "!TEM_String!"
)
exit /b

:Get_Config_String
if exist "!INI_Text!" (
	set /a "TEM_Count=0"
	set /a "Hidden_Code=1"
	for /f "tokens=1* delims==:" %%a in ('type !INI_Text!') do (
		set "TEM_Line=%%a"
		if "!TEM_Line!" equ "!TEM_Line:Current ID=!" (
			if "!TEM_Line!" neq "!TEM_Line:ID=!" (
				set /a "TEM_Count+=1"
			)
		)
		if "!TEM_Line!" neq "!TEM_Line:%~2=!" (
			if "!TEM_Line!" neq "!TEM_Line:-H=!" (set /a "Hidden_Code=0")
			call :Get_Config_String_Formatting %1 %%b !TEM_Count!
		)
	)
)
exit /b

:Get_Config_String_Formatting
set "%~1=%~2" & set "%~1_%~3=%~2"
exit /b

REM --------------------------------------------------

:Edit_Config
set "Config_Code=%~2"
if /i "%~1" equ "Create" (
	(
		echo ##################################################
		if /i "!Config_Code!" neq "!Config_Code:Ma=!" (echo Current ID           = "!Current_ID!")
		if /i "!Config_Code!" neq "!Config_Code:Mb=!" (echo Current Mode         = "!Current_Mode!")
		if /i "!Config_Code!" neq "!Config_Code:Mc=!" (echo Current File Index   = "!Current_File_Index!")
		if /i "!Config_Code!" neq "!Config_Code:Md=!" (echo Current Folder Index = "!Current_Folder_Index!")
		if /i "!Config_Code!" neq "!Config_Code:Me=!" (echo Current Toggle Time  = "!Current_Toggle_Time!")
		if /i "!Config_Code!" neq "!Config_Code:Mf=!" (echo Current Toggle Mode  = "!Current_Toggle_Mode!")
		if /i "!Config_Code!" neq "!Config_Code:Mg=!" (echo Current File Name    = "!Current_File_Name!")
		if /i "!Config_Code!" neq "!Config_Code:Mh=!" (echo Current Folder Name  = "!Current_Folder_Name!")
		if /i "!Config_Code!" neq "!Config_Code:Mi=!" (echo Current Root Path    = "!Current_Root_Path!")
		if /i "!Config_Code!" neq "!Config_Code:Mj=!" (echo All File Count       = "!All_File_Count!")
		if /i "!Config_Code!" neq "!Config_Code:Mk=!" (echo All Folder Count     = "!All_Folder_Count!")

		if /i "!Config_Code!" neq "!Config_Code:D0=!" (
			for /f "delims=" %%a in ('type "!TEM_Text!-Sort.TEM"') do (
				echo --------------------------------------------------
				for /f "delims=" %%b in ('type "%%a"') do (echo %%b)
			)
		)
		echo ##################################################
	)>"!INI_Text!"
)
if /i "%~1" equ "Update" (
	cd.>"!TEM_Text!.TEM" & set "Config_Code=%*" & set Config_Code=!Config_Code:%1 =!
	for /f "tokens=1,2 delims==" %%a in ('type "!INI_Text!"') do (
		if "%%b" neq "" (set "TEM_Line=%%a=%%b") else (set "TEM_Line=%%a")
		for %%c in (!Config_Code!) do (
			set "TEM_String=%%~c" & set "TEM_String=!TEM_String:_= !"
			for /f "delims=" %%d in ("!TEM_String!") do (
				if "!TEM_Line!" neq "!TEM_Line:%%d=!" (set "TEM_Line=%%a= "!%%~c!"")
			)
		)
		echo !TEM_Line!>>"!TEM_Text!.TEM"
	)
	move /y "!TEM_Text!.TEM" "!INI_Text!" >nul 2>nul
)
if /i "%~1" equ "Edit" (
	cd.>"!TEM_Text!.TEM" & set "Config_Code=%~2"
	for /f "delims=" %%a in ('type "!INI_Text!"') do (
		for /f "tokens=1,2 delims=: " %%b in ("%%a") do (
			if /i "%~2" equ "%%~c" (
				echo     %%b-H : %%c
			) else (
				echo %%a
			)
		)
	)>>"!TEM_Text!.TEM"
	move /y "!TEM_Text!.TEM" "!INI_Text!" >nul 2>nul
)
exit /b

REM --------------------------------------------------

:Automated_Tasks
if /i "%~1" equ "Delete" (schtasks /%~1 /tn "WallpaperManager" /f >nul 2>nul)
if /i "%~1" equ "Change" (schtasks /%~1 /tn "WallpaperManager" /%~2 >nul 2>nul)
if /i "%~1" equ "Create" (
    set "Now=!Today!T!Time:~0,-3!"
    (
        echo ^<^?xml version="1.0" encoding="UTF-16"^?^>
        echo ^<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"^>
        echo   ^<RegistrationInfo^>
        echo     ^<Date^>!Now!^</Date^>
        echo     ^<Author^>!COMPUTERNAME!\!USERNAME!^</Author^>
        echo     ^<URI^>\WallPaperManager^</URI^>
        echo   ^</RegistrationInfo^>
        echo   ^<Triggers^>
        echo     ^<TimeTrigger^>
        echo       ^<Repetition^>
        echo         ^<Interval^>PT!CURRENT_TOGGLE_TIME!M^</Interval^>
        echo         ^<StopAtDurationEnd^>false^</StopAtDurationEnd^>
        echo       ^</Repetition^>
        echo       ^<StartBoundary^>!Now!^</StartBoundary^>
        echo       ^<Enabled^>true^</Enabled^>
        echo     ^</TimeTrigger^>
        echo   ^</Triggers^>
        echo   ^<Principals^>
        echo     ^<Principal id="Author"^>
        echo       ^<UserId^>!USER_SID!^</UserId^>
        echo       ^<LogonType^>InteractiveToken^</LogonType^>
        echo       ^<RunLevel^>HighestAvailable^</RunLevel^>
        echo     ^</Principal^>
        echo   ^</Principals^>
        echo   ^<Settings^>
        echo     ^<MultipleInstancesPolicy^>IgnoreNew^</MultipleInstancesPolicy^>
        echo     ^<DisallowStartIfOnBatteries^>false^</DisallowStartIfOnBatteries^>
        echo     ^<StopIfGoingOnBatteries^>false^</StopIfGoingOnBatteries^>
        echo     ^<AllowHardTerminate^>false^</AllowHardTerminate^>
        echo     ^<StartWhenAvailable^>true^</StartWhenAvailable^>
        echo     ^<RunOnlyIfNetworkAvailable^>false^</RunOnlyIfNetworkAvailable^>
        echo     ^<IdleSettings^>
        echo       ^<StopOnIdleEnd^>true^</StopOnIdleEnd^>
        echo       ^<RestartOnIdle^>false^</RestartOnIdle^>
        echo     ^</IdleSettings^>
        echo     ^<AllowStartOnDemand^>true^</AllowStartOnDemand^>
        echo     ^<Enabled^>true^</Enabled^>
        echo     ^<Hidden^>false^</Hidden^>
        echo     ^<RunOnlyIfIdle^>false^</RunOnlyIfIdle^>
        echo     ^<DisallowStartOnRemoteAppSession^>false^</DisallowStartOnRemoteAppSession^>
        echo     ^<UseUnifiedSchedulingEngine^>true^</UseUnifiedSchedulingEngine^>
        echo     ^<WakeToRun^>false^</WakeToRun^>
        echo     ^<ExecutionTimeLimit^>PT0S^</ExecutionTimeLimit^>
        echo     ^<Priority^>7^</Priority^>
        echo     ^<RestartOnFailure^>
        echo       ^<Interval^>PT1M^</Interval^>
        echo       ^<Count^>999^</Count^>
        echo     ^</RestartOnFailure^>
        echo   ^</Settings^>
        echo   ^<Actions Context="Author"^>
        echo     ^<Exec^>
        echo       ^<Command^>mshta^</Command^>
        echo       ^<Arguments^>vbscript:Execute^("CreateObject(""WScript.Shell"").Run ""conhost cmd /c Powershell -C %~f0 Toggle_File Next"", 0, False:window.close"^)^</Arguments^>
        echo     ^</Exec^>
        echo   ^</Actions^>
        echo ^</Task^>
    )>"!XML_Text!"
    schtasks /delete /tn "WallpaperManager" /f >nul 2>nul
    schtasks /create /tn "WallpaperManager" /xml "!XML_Text!" /f >nul 2>nul
	del /f /q "!XML_Text!" >nul 2>nul
)
exit /b

REM --------------------------------------------------

:Setting_Mouse_Menu
chcp 936>nul
for %%a in (File Folder) do (
	if /i "%%a" equ "%~1" (set "%%a_Menu_Symbol= (当前)") else (set "%%a_Menu_Symbol=")
)

set "TEM_String=%~2"
for %%a in (N D S J Z) do (
	if "!TEM_String!" neq "!TEM_String:%%a=!" (set "%%a_Menu_Symbol=!TEM_String:~1! (当前)") else (set "%%a_Menu_Symbol=!TEM_String:~1!")
	if "!%%a_Menu_Symbol!" equ "" (set "%%a_Menu_Symbol=+")
)
if "!Z_Menu_Symbol!" neq "!Z_Menu_Symbol:+=!" (set "Z_Menu_Symbol=!Z_Menu_Symbol:+=!") else (set "Z_Menu_Symbol=!Z_Menu_Symbol:-=!")
chcp 65001>nul
exit /b

REM --------------------------------------------------

:Install_Mouse_Menu
chcp 936>nul
set "Menu_Code=%~1"
reg delete "!DesktopBackground_ToggleWallpaper_Reg_Path!" /f >nul 2>nul
reg delete "!DesktopBackground_WallpaperManager_Reg_Path!" /f >nul 2>nul
if "!Menu_Code!" neq "!Menu_Code:Ta=!" (
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!" /v Icon /t REG_SZ /d "imageres.dll,146" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!" /v MUIVerb /t REG_SZ /d "切换壁纸" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!" /v Position /t REG_SZ /d "bottom" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!" /v SubCommands /t REG_SZ /d "" /f >nul 2>nul
)

if "!Menu_Code!" neq "!Menu_Code:Tb=!" (
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[1].Toggle_Last_Folder" /v MUIVerb /t REG_SZ /d "切至上一目录" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[1].Toggle_Last_Folder\Command" /t REG_SZ /d "%~f0 Toggle_Folder Last" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[1].Toggle_Last_Folder\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Folder Last\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[2].Toggle_Next_Folder" /v MUIVerb /t REG_SZ /d "切至下一目录" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[2].Toggle_Next_Folder\Command" /t REG_SZ /d "%~f0 Toggle_Folder Next" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[2].Toggle_Next_Folder\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Folder Next\"\", 0, False:window.close\"^)" /f >nul 2>nul

	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[3].Line" /v CommandFlags /t REG_DWORD /d "8" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:Tc=!" (
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[4].Toggle_Last_File" /v MUIVerb /t REG_SZ /d "切至上一壁纸" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[4].Toggle_Last_File\Command" /t REG_SZ /d "%~f0 Toggle_File Last" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[4].Toggle_Last_File\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_File Last\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[5].Toggle_Next_File" /v MUIVerb /t REG_SZ /d "切至下一壁纸" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[5].Toggle_Next_File\Command" /t REG_SZ /d "%~f0 Toggle_File Next" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[5].Toggle_Next_File\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_File Next\"\", 0, False:window.close\"^)" /f >nul 2>nul

	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[6].Line" /v CommandFlags /t REG_DWORD /d "8" /f >nul 2>nul

	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[7].Setting_Toggle_Time" /v MUIVerb /t REG_SZ /d "设置切换时间 (!Current_Toggle_Time!)" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[7].Setting_Toggle_Time\Command" /t REG_SZ /d "%~f0 Toggle_Time" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[7].Setting_Toggle_Time\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Time\"\", 0, False:window.close\"^)" /f >nul 2>nul

	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode" /v MUIVerb /t REG_SZ /d "设置切换模式" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode" /v Position /t REG_SZ /d "bottom" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode" /v SubCommands /t REG_SZ /d "" /f >nul 2>nul

	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[1].Name_Order" /v MUIVerb /t REG_SZ /d "名称!N_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[1].Name_Order\Command" /t REG_SZ /d "%~f0 Toggle_Mode N" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[1].Name_Order\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Mode N\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[2].Date_Order" /v MUIVerb /t REG_SZ /d "日期!D_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[2].Date_Order\Command" /t REG_SZ /d "%~f0 Toggle_Mode D" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[2].Date_Order\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Mode D\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[3].Size_Order" /v MUIVerb /t REG_SZ /d "体积!S_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[3].Size_Order\Command" /t REG_SZ /d "%~f0 Toggle_Mode S" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[3].Size_Order\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Mode S\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[4].Random_False_Order" /v MUIVerb /t REG_SZ /d "假随机!J_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[4].Random_False_Order\Command" /t REG_SZ /d "%~f0 Toggle_Mode J" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[4].Random_False_Order\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Mode J\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[5].Random_True_Order" /v MUIVerb /t REG_SZ /d "真随机!Z_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[5].Random_True_Order\Command" /t REG_SZ /d "%~f0 Toggle_Mode Z" /f >nul 2>nul
	reg add "!DesktopBackground_ToggleWallpaper_Reg_Path!\Shell\[8].Setting_Toggle_Mode\Shell\[5].Random_True_Order\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Toggle_Mode Z\"\", 0, False:window.close\"^)" /f >nul 2>nul
)

if "!Menu_Code!" neq "!Menu_Code:Wa=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!" /v Icon /t REG_SZ /d "imageres.dll,146" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!" /v MUIVerb /t REG_SZ /d "壁纸管理" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!" /v Position /t REG_SZ /d "bottom" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!" /v SubCommands /t REG_SZ /d "" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:Wb=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[1].Locate_Wallpaper" /v MUIVerb /t REG_SZ /d "定位壁纸位置" /f >nul 2>nul
	REM reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[1].Locate_Wallpaper\Command" /t REG_SZ /d "%~f0 Locate_Wallpaper" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[1].Locate_Wallpaper\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Locate_Wallpaper\"\", 0, False:window.close\"^)" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:Wc=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[2].Freeze_Wallpaper" /v MUIVerb /t REG_SZ /d "!Current_Freeze_Name!" /f >nul 2>nul
	REM reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[2].Freeze_Wallpaper\Command" /t REG_SZ /d "%~f0 Freeze_Wallpaper" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[2].Freeze_Wallpaper\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Freeze_Wallpaper\"\", 0, False:window.close\"^)" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:Wd=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[4].Hidden_Wallpaper" /v MUIVerb /t REG_SZ /d "隐藏当前壁纸" /f >nul 2>nul
	REM reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[4].Hidden_Wallpaper\Command" /t REG_SZ /d "%~f0 Hidden_Wallpaper" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[4].Hidden_Wallpaper\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Hidden_Wallpaper\"\", 0, False:window.close\"^)" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:We=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[6].Line" /v CommandFlags /t REG_DWORD /d "8" /f >nul 2>nul
)
if "!Menu_Code!" neq "!Menu_Code:Wf=!" (
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode" /v MUIVerb /t REG_SZ /d "设置当前模式" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode" /v Position /t REG_SZ /d "bottom" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode" /v SubCommands /t REG_SZ /d "" /f >nul 2>nul

	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[1].File_Mode" /v MUIVerb /t REG_SZ /d "文件模式!File_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[1].File_Mode\Command" /t REG_SZ /d "%~f0 File_Mode" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[1].File_Mode\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 File_Mode\"\", 0, False:window.close\"^)" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[2].Folder_Mode" /v MUIVerb /t REG_SZ /d "目录模式!Folder_Menu_Symbol!" /f >nul 2>nul
	REM reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[2].Folder_Mode\Command" /t REG_SZ /d "%~f0 Folder_Mode" /f >nul 2>nul
	reg add "!DesktopBackground_WallpaperManager_Reg_Path!\Shell\[7].Setting_Current_Mode\Shell\[2].Folder_Mode\Command" /t REG_SZ /d "mshta vbscript:Execute^(\"CreateObject^(\"\"WScript.Shell\"\"^).Run \"\"conhost cmd /c Powershell -C %~f0 Folder_Mode\"\", 0, False:window.close\"^)" /f >nul 2>nul
)
chcp 65001>nul
exit /b

REM --------------------------------------------------

:Applay_Wallpaper
reg add "!DesktopBackground_Reg_Path!" /v "Wallpaper" /t REG_SZ /d "%~1" /f >nul 2>nul
reg add "!LockScreenBackground_Reg_Path!" /v "LogonImagePath" /t REG_SZ /d "%~1" /f >nul 2>nul
reg add "!LockScreenBackground_Reg_Path!" /v "LockScreenImagePath" /t REG_SZ /d "%~1" /f >nul 2>nul

Powershell -C "Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class Wallpaper{[DllImport(\"user32.dll\")]public static extern int SystemParametersInfo(int uAction,int uParam,string lpvParam,int fuWinIni);}';[void][Wallpaper]::SystemParametersInfo(0x14,0,(Get-ItemPropertyValue 'HKCU:\Control Panel\Desktop' -Name Wallpaper),3)"
exit /b

REM --------------------------------------------------

:Output_Message
msg * "%~1" & echo 1y | choice /t 10 /d y>nul & exit /b

REM --------------------------------------------------