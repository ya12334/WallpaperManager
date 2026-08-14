/* registry.c — 右键菜单注册实现 */
#include "registry.h"
#include "log.h"

#define REGBUF 1024

#define REG_DIR_MENU       L"Directory\\shell\\WallpaperManager"
#define REG_DESKTOP_TOGGLE L"DesktopBackground\\Shell\\ToggleWallpaper"
#define REG_DESKTOP_MGR    L"DesktopBackground\\Shell\\WallpaperManager"

static const wchar_t *g_exts[] = {
    L"Jpg", L"Jpeg", L"Png", L"Bmp", L"Gif",
    L"Tif", L"Tiff", L"Heic", L"Heif", L"Avif"
};
#define G_EXT_COUNT (sizeof(g_exts) / sizeof(g_exts[0]))

/* ---------- 注册表原语 ---------- */
static LONG reg_write_sz(HKEY root, const wchar_t *sub, const wchar_t *name, const wchar_t *val) {
    HKEY key;
    LONG rc = RegCreateKeyExW(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)val,
                        (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc;
}

static LONG reg_write_expand_sz(HKEY root, const wchar_t *sub, const wchar_t *name, const wchar_t *val) {
    HKEY key;
    LONG rc = RegCreateKeyExW(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(key, name, 0, REG_EXPAND_SZ, (const BYTE *)val,
                        (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc;
}

static LONG reg_write_dword(HKEY root, const wchar_t *sub, const wchar_t *name, DWORD v) {
    HKEY key;
    LONG rc = RegCreateKeyExW(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&v, sizeof(v));
    RegCloseKey(key);
    return rc;
}

static void reg_delete_tree(HKEY root, const wchar_t *sub) {
    RegDeleteTreeW(root, sub);
}

/* 在 base 下写入键值（base 为 HKCR 相对路径） */
static void set_sz(const wchar_t *base, const wchar_t *child, const wchar_t *name, const wchar_t *val) {
    wchar_t path[REGBUF];
    StringCchPrintfW(path, ARRAYSIZE(path), L"%s\\%s", base, child);
    reg_write_sz(HKEY_CLASSES_ROOT, path, name, val);
}
static void set_dword(const wchar_t *base, const wchar_t *child, const wchar_t *name, DWORD v) {
    wchar_t path[REGBUF];
    StringCchPrintfW(path, ARRAYSIZE(path), L"%s\\%s", base, child);
    reg_write_dword(HKEY_CLASSES_ROOT, path, name, v);
}
static void set_cmd(const wchar_t *base, const wchar_t *child, const wchar_t *exe,
                    const wchar_t *verb, const wchar_t *param) {
    wchar_t cmd[REGBUF];
    if (param) StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" %s %s", exe, verb, param);
    else       StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" %s", exe, verb);
    wchar_t path[REGBUF];
    StringCchPrintfW(path, ARRAYSIZE(path), L"%s\\%s\\Command", base, child);
    reg_write_sz(HKEY_CLASSES_ROOT, path, NULL, cmd);
}

static int has_token(const wchar_t *code, const wchar_t *tok) {
    const wchar_t *p = code;
    size_t tl = wcslen(tok);
    while (p && *p) {
        while (*p == L' ') p++;
        if (wcsncmp(p, tok, tl) == 0 && (p[tl] == L' ' || p[tl] == L'\0')) return 1;
        p = wcschr(p, L' ');
    }
    return 0;
}

static const wchar_t *mode_label(wchar_t c) {
    switch (c) {
        case L'N': return L"名称";
        case L'D': return L"日期";
        case L'S': return L"体积";
        case L'J': return L"假随机";
        case L'Z': return L"真随机";
    }
    return L"";
}

/* 构造切换模式菜单项标签：名称 + 排序方向(+/-) + 当前标记。
 * 仅当前模式附加方向符号（+ 升序 / - 降序）；真随机(Z)无方向。 */
static void mode_label_full(const AppConfig *cfg, wchar_t c, wchar_t *out, size_t n) {
    const wchar_t *name = mode_label(c);
    int cur = (cfg->toggle_mode && cfg->toggle_mode[0] == c);
    wchar_t dir = 0;
    if (cur && c != L'Z')
        dir = (cfg->toggle_mode[1] == L'-') ? L'-' : L'+';
    if (dir)
        StringCchPrintfW(out, n, L"%s%c%s", name, dir, cur ? L" (当前)" : L"");
    else
        StringCchPrintfW(out, n, L"%s%s", name, cur ? L" (当前)" : L"");
}

/* ---------- 安装 ---------- */
int registry_install(void) {
    wchar_t *exe = path_get_exe_path();

    /* 清理旧菜单 */
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DIR_MENU);
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_TOGGLE);
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_MGR);

    /* 图片类型右键菜单（HKLM，需管理员） */
    for (size_t i = 0; i < G_EXT_COUNT; i++) {
        wchar_t base[REGBUF];
        StringCchPrintfW(base, ARRAYSIZE(base),
            L"SOFTWARE\\Classes\\SystemFileAssociations\\.%s\\Shell\\WallpaperManager", g_exts[i]);
        wchar_t cmd[REGBUF];
        StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" \"%%1\"", exe);
        reg_write_sz(HKEY_LOCAL_MACHINE, base, L"Icon", L"imageres.dll,146");
        reg_write_sz(HKEY_LOCAL_MACHINE, base, L"MUIVerb", L"设置为桌面壁纸");

        wchar_t cmdpath[REGBUF];
        StringCchPrintfW(cmdpath, ARRAYSIZE(cmdpath), L"%s\\Command", base);
        reg_write_sz(HKEY_LOCAL_MACHINE, cmdpath, NULL, cmd);

        /* 删除系统默认"设为桌面壁纸" */
        wchar_t dflt[REGBUF];
        StringCchPrintfW(dflt, ARRAYSIZE(dflt),
            L"SOFTWARE\\Classes\\SystemFileAssociations\\.%s\\Shell\\setdesktopwallpaper", g_exts[i]);
        reg_delete_tree(HKEY_LOCAL_MACHINE, dflt);
    }

    /* 目录右键菜单 */
    {
        wchar_t cmd[REGBUF];
        StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" \"%%1\"", exe);
        reg_write_sz(HKEY_CLASSES_ROOT, REG_DIR_MENU, L"Icon", L"imageres.dll,146");
        reg_write_sz(HKEY_CLASSES_ROOT, REG_DIR_MENU, L"MUIVerb", L"设置为桌面壁纸");
        wchar_t cmdpath[REGBUF];
        StringCchPrintfW(cmdpath, ARRAYSIZE(cmdpath), L"%s\\Command", REG_DIR_MENU);
        reg_write_sz(HKEY_CLASSES_ROOT, cmdpath, NULL, cmd);
    }

    /* 桌面右键菜单（默认集：Wa Wb We Wf；尚未设置模式，故不显示“(当前)”标记） */
    AppConfig tmp;
    config_init(&tmp);
    heap_free(tmp.mode); tmp.mode = wcs_dup(L"");
    registry_install_desktop_menu(L"Wa Wb We Wf", &tmp, L"冻结当前壁纸");
    config_free(&tmp);

    heap_free(exe);
    LOGI(L"Context menu installed");
    return 0;
}

/* ---------- 卸载 ---------- */
int registry_uninstall(void) {
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DIR_MENU);
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_TOGGLE);
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_MGR);

    for (size_t i = 0; i < G_EXT_COUNT; i++) {
        wchar_t base[REGBUF];
        StringCchPrintfW(base, ARRAYSIZE(base),
            L"SOFTWARE\\Classes\\SystemFileAssociations\\.%s\\Shell\\WallpaperManager", g_exts[i]);
        reg_delete_tree(HKEY_LOCAL_MACHINE, base);

        /* 恢复系统默认"设为桌面壁纸" */
        wchar_t dflt[REGBUF];
        StringCchPrintfW(dflt, ARRAYSIZE(dflt),
            L"SOFTWARE\\Classes\\SystemFileAssociations\\.%s\\Shell\\setdesktopwallpaper", g_exts[i]);
        reg_write_expand_sz(HKEY_LOCAL_MACHINE, dflt, NULL, L"@%SystemRoot%\\system32\\stobject.dll,-417");
        reg_write_sz(HKEY_LOCAL_MACHINE, dflt, L"MultiSelectModel", L"Player");
        reg_write_sz(HKEY_LOCAL_MACHINE, dflt, L"NeverDefault", L"");

        wchar_t cmdpath[REGBUF];
        StringCchPrintfW(cmdpath, ARRAYSIZE(cmdpath), L"%s\\Command", dflt);
        reg_write_expand_sz(HKEY_LOCAL_MACHINE, cmdpath, NULL, L"%SystemRoot%\\Explorer.exe");
        reg_write_sz(HKEY_LOCAL_MACHINE, cmdpath, L"DelegateExecute", L"{ff609cc7-d34d-4049-a1aa-2293517ffcc6}");
    }

    LOGI(L"Context menu uninstalled");
    return 0;
}

/* ---------- 动态桌面右键菜单 ---------- */
void registry_install_desktop_menu(const wchar_t *menu_code, const AppConfig *cfg,
                                   const wchar_t *freeze_name) {
    wchar_t *exe = path_get_exe_path();

    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_TOGGLE);
    reg_delete_tree(HKEY_CLASSES_ROOT, REG_DESKTOP_MGR);

    /* Ta：切换壁纸 根菜单 */
    if (has_token(menu_code, L"Ta")) {
        set_sz(REG_DESKTOP_TOGGLE, L"", L"Icon", L"imageres.dll,146");
        set_sz(REG_DESKTOP_TOGGLE, L"", L"MUIVerb", L"切换壁纸");
        set_sz(REG_DESKTOP_TOGGLE, L"", L"Position", L"bottom");
        set_sz(REG_DESKTOP_TOGGLE, L"", L"SubCommands", L"");
    }
    /* Tb：切换目录 */
    if (has_token(menu_code, L"Tb")) {
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[1].Toggle_Last_Folder", L"MUIVerb", L"切换到上一目录");
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[1].Toggle_Last_Folder", exe, L"Toggle_Folder", L"Last");
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[2].Toggle_Next_Folder", L"MUIVerb", L"切换到下一目录");
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[2].Toggle_Next_Folder", exe, L"Toggle_Folder", L"Next");
        set_dword(REG_DESKTOP_TOGGLE, L"Shell\\[3].Line", L"CommandFlags", 8);
    }
    /* Tc：切换文件 + 设置切换时间/模式 */
    if (has_token(menu_code, L"Tc")) {
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[4].Toggle_Last_File", L"MUIVerb", L"切换到上一壁纸");
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[4].Toggle_Last_File", exe, L"Toggle_File", L"Last");
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[5].Toggle_Next_File", L"MUIVerb", L"切换到下一壁纸");
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[5].Toggle_Next_File", exe, L"Toggle_File", L"Next");
        set_dword(REG_DESKTOP_TOGGLE, L"Shell\\[6].Line", L"CommandFlags", 8);

        wchar_t time_label[128];
        StringCchPrintfW(time_label, ARRAYSIZE(time_label), L"设置切换时间 (%d)", cfg->toggle_interval);
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[7].Setting_Toggle_Time", L"MUIVerb", time_label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[7].Setting_Toggle_Time", exe, L"Toggle_Time", NULL);

        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode", L"MUIVerb", L"设置切换模式");
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode", L"Position", L"bottom");
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode", L"SubCommands", L"");

        wchar_t label[128];
        mode_label_full(cfg, L'N', label, ARRAYSIZE(label));
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[1].Name_Order", L"MUIVerb", label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[1].Name_Order", exe, L"Toggle_Mode", L"N");

        mode_label_full(cfg, L'D', label, ARRAYSIZE(label));
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[2].Date_Order", L"MUIVerb", label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[2].Date_Order", exe, L"Toggle_Mode", L"D");

        mode_label_full(cfg, L'S', label, ARRAYSIZE(label));
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[3].Size_Order", L"MUIVerb", label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[3].Size_Order", exe, L"Toggle_Mode", L"S");

        mode_label_full(cfg, L'J', label, ARRAYSIZE(label));
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[4].Random_False_Order", L"MUIVerb", label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[4].Random_False_Order", exe, L"Toggle_Mode", L"J");

        mode_label_full(cfg, L'Z', label, ARRAYSIZE(label));
        set_sz(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[5].Random_True_Order", L"MUIVerb", label);
        set_cmd(REG_DESKTOP_TOGGLE, L"Shell\\[8].Setting_Toggle_Mode\\Shell\\[5].Random_True_Order", exe, L"Toggle_Mode", L"Z");
    }

    /* Wa：壁纸管理器 根菜单 */
    if (has_token(menu_code, L"Wa")) {
        set_sz(REG_DESKTOP_MGR, L"", L"Icon", L"imageres.dll,146");
        set_sz(REG_DESKTOP_MGR, L"", L"MUIVerb", L"壁纸管理器");
        set_sz(REG_DESKTOP_MGR, L"", L"Position", L"bottom");
        set_sz(REG_DESKTOP_MGR, L"", L"SubCommands", L"");
    }
    if (has_token(menu_code, L"Wb")) {
        set_sz(REG_DESKTOP_MGR, L"Shell\\[1].Locate_Wallpaper", L"MUIVerb", L"定位壁纸位置");
        set_cmd(REG_DESKTOP_MGR, L"Shell\\[1].Locate_Wallpaper", exe, L"Locate_Wallpaper", NULL);
    }
    if (has_token(menu_code, L"Wc")) {
        set_sz(REG_DESKTOP_MGR, L"Shell\\[2].Freeze_Wallpaper", L"MUIVerb", freeze_name);
        set_cmd(REG_DESKTOP_MGR, L"Shell\\[2].Freeze_Wallpaper", exe, L"Freeze_Wallpaper", NULL);
    }
    if (has_token(menu_code, L"Wd")) {
        set_sz(REG_DESKTOP_MGR, L"Shell\\[4].Hidden_Wallpaper", L"MUIVerb", L"隐藏当前壁纸");
        set_cmd(REG_DESKTOP_MGR, L"Shell\\[4].Hidden_Wallpaper", exe, L"Hidden_Wallpaper", NULL);
    }
    if (has_token(menu_code, L"We")) {
        set_dword(REG_DESKTOP_MGR, L"Shell\\[6].Line", L"CommandFlags", 8);
    }
    if (has_token(menu_code, L"Wf")) {
        set_sz(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode", L"MUIVerb", L"设置当前模式");
        set_sz(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode", L"Position", L"bottom");
        set_sz(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode", L"SubCommands", L"");

        wchar_t flabel[128], dlabel[128];
        StringCchPrintfW(flabel, ARRAYSIZE(flabel), L"文件模式%s",
            wcs_ieq(cfg->mode, CFG_MODE_FILE) ? L" (当前)" : L"");
        StringCchPrintfW(dlabel, ARRAYSIZE(dlabel), L"目录模式%s",
            (wcs_ieq(cfg->mode, CFG_MODE_SINGLE) || wcs_ieq(cfg->mode, CFG_MODE_MORE)) ? L" (当前)" : L"");

        set_sz(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode\\Shell\\[1].File_Mode", L"MUIVerb", flabel);
        set_cmd(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode\\Shell\\[1].File_Mode", exe, L"File_Mode", NULL);
        set_sz(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode\\Shell\\[2].Folder_Mode", L"MUIVerb", dlabel);
        set_cmd(REG_DESKTOP_MGR, L"Shell\\[7].Setting_Current_Mode\\Shell\\[2].Folder_Mode", exe, L"Folder_Mode", NULL);
    }

    heap_free(exe);
}
