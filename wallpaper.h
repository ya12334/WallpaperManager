/* wallpaper.h — 壁纸应用模块
 * 直接调用 user32 的 SystemParametersInfo 与注册表 API，不使用 PowerShell/mshta。
 */
#pragma once
#include "common.h"

/* 设置桌面 + 锁屏壁纸，返回 0 成功、-1 失败（桌面壁纸设置失败时） */
int wallpaper_set(const wchar_t *path);

/* 开启锁屏图片状态标志（LogonImageStatus / LockScreenImageStatus = 1），尽力而为 */
void wallpaper_enable_lock_screen(void);

/* 读取当前桌面壁纸路径（HKCU\Control Panel\Desktop\Wallpaper），返回新分配字符串 */
wchar_t *wallpaper_get_current(void);
