/* registry.h — 右键菜单注册模块
 * 图片文件右键、目录右键、桌面右键（切换壁纸 / 壁纸管理器）三级级联菜单。
 * 命令直接指向本程序（GUI 子系统，无控制台窗口），不再使用 mshta/PowerShell。
 */
#pragma once
#include "common.h"
#include "config.h"

int  registry_install(void);   /* 注册全部右键菜单，0 成功 */
int  registry_uninstall(void); /* 卸载菜单并恢复系统默认"设为桌面壁纸" */
void registry_install_desktop_menu(const wchar_t *menu_code, const AppConfig *cfg,
                                   const wchar_t *freeze_name);
