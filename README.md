# WallpaperManager

一个纯 C（Win32 API）编写的 Windows 壁纸管理器，是原 `WallpaperManager.Bat`（824 行批处理）的模块化重构版本，由 **DeepSeek v4 Pro** 推理模型生成。

> **仅支持 Windows**，测试环境：Windows 11 24H2（运行用户 Administrator）。

- **宽字符（UTF-16）**：完整支持中文、韩文、日文、泰文及 emoji 等全部 Unicode 字符，不依赖 8.3 短名路径。
- **隐藏运行**：编译为 GUI 子系统程序（无控制台窗口），被右键菜单 / 定时任务调用时天然隐藏，无需 mshta / PowerShell 包装。
- **零外部依赖**：壁纸应用、任务计划、右键菜单全部直接调用 Win32 API，不使用 mshta / PowerShell，不产生临时文件 / XML 文件。

---

## 特性

| 需求 | 实现方式 |
|---|---|
| 模块化开发 | `main` / `common` / `log` / `config` / `filelist` / `wallpaper` / `dialog` / `registry` / `task` 九大模块 |
| 不依赖 8.3 短名 | 全程宽字符完整路径（`FindFirstFileW` 等） |
| 现代化窗口 | `IFileOpenDialog`（文件/目录选择）、`TaskDialogIndirect`（错误/信息）、自绘模态输入框（数字） |
| 完整日志 | `WallpaperManager.log`（UTF-8，记录每次命令、结果与错误） |
| 代码有效性 | `-Wall -Wextra` 零警告 + clang 静态分析零告警 |
| 隐藏运行 | GUI 子系统（`-mwindows`），无需 CLSID |
| 无临时/XML 文件 | 文件列表内存 `qsort` 排序，任务计划走 COM 接口 |
| 交叉淡化过渡 | 切换壁纸时旧壁纸淡出、新壁纸淡入（GDI+ 内存渲染 + 分层窗口，仅作用于壁纸层） |

支持的图片类型：`Jpg` `Jpeg` `Png` `Bmp` `Gif` `Tif` `Tiff` `Heic` `Heif` `Avif`。

---

## 编译

工具链：**LLVM-MinGW UCRT**（clang，建议 ≥ 18，本项目验证于 22.1.8）。

```bat
build.bat
```

或手动执行等价命令：

```bat
llvm-windres app.rc app.res.o
clang -std=c11 -O2 -mwindows -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -Wall -Wextra ^
  main.c common.c log.c config.c filelist.c wallpaper.c dialog.c registry.c task.c app.res.o ^
  -o WallpaperManager.exe ^
  -lole32 -loleaut32 -lcomctl32 -lshell32 -ladvapi32 -luser32 -lgdi32 -lgdiplus -luuid -ltaskschd
```

产物 `WallpaperManager.exe` 为 `IMAGE_SUBSYSTEM_WINDOWS_GUI` 程序，运行时不弹控制台。

---

## 使用

### 安装 / 卸载

```bat
WallpaperManager.exe Install      rem 安装右键菜单（图片/目录/桌面）
WallpaperManager.exe UnInstall    rem 移除菜单、定时任务与配置
```

> `Install` 写入图片右键菜单（HKLM）与桌面菜单，建议以管理员身份运行以完整安装。

> 不带任何参数直接运行 `WallpaperManager.exe` 会弹出提示「请查阅相关指令后以命令行的方式运行本程序。」，请按下方命令以命令行方式使用。

### 右键菜单命令

- **图片文件** → `设为壁纸背景`（文件模式，固定壁纸）
- **目录** → `设为壁纸背景`（目录模式，自动轮播）
- **桌面右键**：
  - `切换壁纸` → 上一张 / 下一张、设置切换时间、设置切换模式
  - `切换目录` → 上一目录 / 下一目录（多目录模式）
  - `壁纸管理器` → 定位壁纸位置、冻结/取消冻结、隐藏当前壁纸、设置当前模式

### 直接命令行（路径分发）

```bat
WallpaperManager.exe <目录路径>   rem 等价 Folder_Mode
WallpaperManager.exe <图片路径>   rem 等价 File_Mode
WallpaperManager.exe Toggle_File Next|Last
WallpaperManager.exe Toggle_Folder Next|Last
WallpaperManager.exe Toggle_Mode N|D|S|J|Z
WallpaperManager.exe Toggle_Time
WallpaperManager.exe Freeze_Wallpaper
WallpaperManager.exe Hidden_Wallpaper
WallpaperManager.exe Locate_Wallpaper
```

---

## 切换模式

切换模式由首字母 + 方向构成（`toggle_mode`，如 `N+`、`D-`、`Z`）：

| 首字母 | 含义 |
|---|---|
| `N` | 名称排序 |
| `D` | 日期排序 |
| `S` | 大小排序 |
| `J` | 随机（固定顺序） |
| `Z` | 随机（每次不同） |

| 方向 | 含义 |
|---|---|
| `+` | 升序 / 向前 |
| `-` | 降序 / 向后 |

---

## 配置文件

`WallpaperManager.cfg`（UTF-8，位于 exe 同目录）：

```ini
[state]
mode = Single Folder Mode          ; Single Folder Mode / More Folder Mode / File Mode
toggle_interval = 10               ; 自动切换间隔（分钟，1-999）
toggle_mode = N+                   ; 切换模式
current_id = 1                     ; 当前目录序号（多目录模式）
file_index = 0                     ; 当前文件索引
folder_index = 0
current_file = xxx.jpg             ; 当前壁纸文件名
current_folder = xxx               ; 当前目录名
root_path = C:\...                 ; 根路径

[folders]
C:\...\folder1                     ; 多目录模式：每行一个含图片目录

[hidden]
xxx.jpg                           ; 每行一个已隐藏的文件名
```

---

## 目录结构

```
WallpaperManager/
├── main.c          # 入口（WinMain）与命令分发
├── common.c/.h     # 内存 / 字符串 / 路径 / UTF-8 / 随机数 / 进程锁
├── log.c/.h        # UTF-8 完整日志
├── config.c/.h     # 配置读写（方案 A）
├── filelist.c/.h   # 文件扫描、排序、目录递归收集
├── wallpaper.c/.h  # 壁纸应用（桌面 + 锁屏 + 交叉淡化过渡）
├── dialog.c/.h     # 现代对话框（选择 / 提示 / 数字输入）
├── registry.c/.h   # 右键菜单安装 / 卸载
├── task.c/.h       # 任务计划（Task Scheduler 2.0 COM）
├── app.manifest    # comctl32 v6 视觉样式 + TaskDialog
├── app.rc          # 资源脚本（内嵌 manifest）
├── build.bat       # 构建脚本
└── WallpaperManager.exe
```

---

## 技术要点

- **壁纸应用**：先写注册表（桌面 HKCU `Control Panel\Desktop\Wallpaper`、锁屏 HKLM `PersonalizationCSP`），再调用 `SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, path, SPIF_SENDWININICHANGE)` 立即生效（不写 win.ini）。
- **交叉淡化过渡**：切换壁纸时，旧壁纸经 GDI+ 渲染到分层覆盖窗口（置于壁纸层之上、图标与应用之下），应用新壁纸后淡出覆盖层，仅壁纸层受影响。渲染严格复刻系统「填充」定位（垂直 `dy=(H-dh)/3`、水平居中 `dx=(W-dw)/2`），避免过渡时壁纸上下跳动。
- **任务计划**：Task Scheduler 2.0 COM（`ITaskService`），无 XML、无 `schtasks`。任务 `WallpaperManager` 以 `InteractiveToken` / 最高权限，每 N 分钟执行 `WallpaperManager.exe Toggle_File Next`。
- **冻结壁纸**：`Freeze_Wallpaper` 在启用/停用定时任务之间切换，菜单标签随之在「冻结当前壁纸」/「取消冻结壁纸」之间动态变化。
- **右键菜单**：`HKCR\DesktopBackground\Shell\ToggleWallpaper`（切换壁纸）与 `WallpaperManager`（壁纸管理器）采用 `SubCommands` 级联；目录/图片菜单分别写入 `HKCR\Directory\shell\WallpaperManager` 与 `HKLM\SOFTWARE\Classes\SystemFileAssociations\.<ext>\Shell\WallpaperManager`。
- **效率优化**：切换文件不重装右键菜单（菜单内容仅依赖模式/切换模式/间隔/冻结态），定时轮播每次只做「读配置 → 扫目录 → 排序 → 设壁纸 → 存配置」，无注册表写开销。

---

## 注意事项

- 锁屏壁纸写入 HKLM，非管理员运行时该部分自动降级（记入日志警告），桌面壁纸不受影响。
- 若系统任务计划服务被禁用，自动轮播不可用（记入日志警告），手动切换仍正常。
- 卸载（`UnInstall`）会恢复系统默认的「设为桌面壁纸」菜单项。
