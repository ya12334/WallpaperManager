/* main.c — 主流程与命令分发
 *
 * 编译为 GUI 子系统程序（无控制台窗口），被右键菜单直接调用即隐藏运行，
 * 无需 mshta/PowerShell 包装。命令行参数采用宽字符（UTF-16）解析。
 */
#include "common.h"
#include "log.h"
#include "config.h"
#include "filelist.h"
#include "wallpaper.h"
#include "dialog.h"
#include "registry.h"
#include "task.h"

#include <wctype.h>

/* ---------- 切换模式辅助 ---------- */
static int sort_from_mode(wchar_t c) {
    switch (c) {
        case L'D': return SORT_DATE;
        case L'S': return SORT_SIZE;
        case L'J': return SORT_RANDOM;
        case L'Z': return SORT_RANDOM;
        case L'N': default: return SORT_NAME;
    }
}

static int direction(const wchar_t *tm) {
    return (tm && tm[1] == L'-') ? -1 : 1;
}

static wchar_t *current_folder_path(const AppConfig *cfg) {
    if (cfg->folder_count == 0) return wcs_dup(cfg->root_path);
    int idx = 0;
    if (wcs_ieq(cfg->mode, CFG_MODE_MORE)) {
        idx = cfg->current_id - 1;
        if (idx < 0) idx = 0;
        if (idx >= cfg->folder_count) idx = cfg->folder_count - 1;
    }
    return wcs_dup(cfg->folders[idx]);
}

/* 根据当前模式返回桌面菜单 token 集 */
static const wchar_t *menu_code(const AppConfig *cfg) {
    return wcs_ieq(cfg->mode, CFG_MODE_SINGLE)
        ? L"Ta Tc Wa Wb Wc Wd We Wf"
        : L"Ta Tb Tc Wa Wb Wc Wd We Wf";
}

/* 冻结菜单标签：随定时任务启停状态动态切换 */
static const wchar_t *freeze_label(void) {
    return (task_is_enabled() == 1) ? L"冻结当前壁纸" : L"取消冻结壁纸";
}

/* ---------- 切换文件（核心） ---------- */
static int toggle_file(const wchar_t *param, AppConfig *cfg) {
    wchar_t *folder = current_folder_path(cfg);

    FileList list;
    int n = fl_scan(&list, folder, cfg);
    if (n == 0) {
        LOGE(L"No images in folder: %s", folder);
        dialog_error(NULL, L"切换壁纸", L"目录内没有可用的壁纸文件。");
        fl_free(&list);
        heap_free(folder);
        return 1;
    }

    int sort = sort_from_mode(cfg->toggle_mode[0]);
    unsigned long long seed = rng_seed_from_string(folder);
    fl_sort(&list, sort, seed);

    int idx;
    if (cfg->toggle_mode[0] == L'Z') {
        unsigned long long st = rng_seed_from_time();
        idx = (int)rng_range(&st, (unsigned long long)list.count);
    } else {
        int step = direction(cfg->toggle_mode);
        idx = wcs_ieq(param, L"Next") ? cfg->file_index + step : cfg->file_index - step;
        if (idx < 0) idx = list.count - 1;
        if (idx >= list.count) idx = 0;
    }

    const FileEntry *e = &list.items[idx];
    int ok = wallpaper_set(e->path);
    cfg->file_index = idx;
    heap_free(cfg->current_file);
    cfg->current_file = wcs_dup(e->name);
    heap_free(cfg->current_folder);
    cfg->current_folder = path_basename(folder);
    config_save(cfg);
    /* 切换文件不改变菜单内容（菜单仅依赖 mode/toggle_mode/interval/冻结态），
       故此处不重装右键菜单，避免每次轮播触发数十次注册表写。 */

    if (ok == 0) LOGI(L"Wallpaper changed: %s", e->path);
    else         LOGE(L"Failed to change wallpaper: %s", e->path);

    fl_free(&list);
    heap_free(folder);
    return ok;
}

/* ---------- 命令处理 ---------- */
static int cmd_install(void) {
    int rc = registry_install();
    if (rc == 0)
        dialog_info(NULL, L"安装", L"右键菜单安装成功。");
    else
        dialog_error(NULL, L"安装", L"安装失败。");
    return rc;
}

static int cmd_uninstall(void) {
    int rc = registry_uninstall();
    task_delete();
    wchar_t *path = config_get_path();
    DeleteFileW(path);
    heap_free(path);
    LOGI(L"Uninstall complete (config, menu and scheduled task removed)");
    if (rc == 0)
        dialog_info(NULL, L"卸载", L"右键菜单卸载成功。");
    else
        dialog_error(NULL, L"卸载", L"卸载失败。");
    return rc;
}

static int cmd_file_mode(const wchar_t *path) {
    wchar_t *file = path ? wcs_dup(path) : dialog_pick_file(NULL);
    if (!file) { dialog_error(NULL, L"文件模式", L"未选择文件。"); return 1; }
    if (!path_is_file(file)) { dialog_error(NULL, L"文件模式", L"文件不存在或不可访问。"); heap_free(file); return 1; }
    if (!is_image_ext(file)) { dialog_error(NULL, L"文件模式", L"不支持的文件类型。"); heap_free(file); return 1; }

    AppConfig cfg;
    config_init(&cfg);
    heap_free(cfg.mode);        cfg.mode = wcs_dup(CFG_MODE_FILE);
    cfg.toggle_interval = 10;
    heap_free(cfg.toggle_mode); cfg.toggle_mode = wcs_dup(L"N+");
    cfg.current_id = 1; cfg.file_index = 0; cfg.folder_index = 0;
    heap_free(cfg.current_file); cfg.current_file = path_basename(file);
    wchar_t *dir = path_dirname(file);
    heap_free(cfg.root_path); cfg.root_path = wcs_dup(dir);
    heap_free(cfg.current_folder); cfg.current_folder = path_basename(dir);
    heap_free(dir);
    config_clear_folders(&cfg);

    if (wallpaper_set(file) != 0) {
        dialog_error(NULL, L"文件模式", L"应用壁纸失败。");
        config_free(&cfg);
        heap_free(file);
        return 1;
    }
    config_save(&cfg);
    task_set_enabled(FALSE); /* 文件模式为固定壁纸，停用自动轮播 */
    registry_install_desktop_menu(L"Wa Wb We Wf", &cfg, L"冻结当前壁纸");
    LOGI(L"File mode set: %s", file);

    config_free(&cfg);
    heap_free(file);
    return 0;
}

static int cmd_folder_mode(const wchar_t *path) {
    wchar_t *folder = path ? wcs_dup(path) : dialog_pick_folder(NULL);
    if (!folder) { dialog_error(NULL, L"目录模式", L"未选择目录。"); return 1; }
    if (!path_is_dir(folder)) { dialog_error(NULL, L"目录模式", L"目录不存在。"); heap_free(folder); return 1; }

    /* 递归收集含图片的目录（含子目录），据此判定单目录 / 多目录模式 */
    FolderList folders;
    int nf = fl_scan_image_folders(&folders, folder);
    if (nf == 0) {
        dialog_error(NULL, L"目录模式", L"该目录及其子目录内未找到图片文件。");
        heap_free(folder);
        return 1;
    }

    AppConfig cfg;
    config_init(&cfg);
    int is_more = (nf > 1);
    heap_free(cfg.mode);        cfg.mode = wcs_dup(is_more ? CFG_MODE_MORE : CFG_MODE_SINGLE);
    cfg.toggle_interval = 10;
    heap_free(cfg.toggle_mode); cfg.toggle_mode = wcs_dup(L"N+");
    cfg.current_id = 1; cfg.file_index = 0; cfg.folder_index = 0;
    wchar_t *dir = path_dirname(folder);
    heap_free(cfg.root_path); cfg.root_path = wcs_dup(dir);
    heap_free(dir);
    config_clear_folders(&cfg);
    for (int i = 0; i < nf; i++) config_add_folder(&cfg, folders.paths[i]);
    fl_free_folders(&folders);
    heap_free(cfg.current_folder); cfg.current_folder = path_basename(cfg.folders[0]);

    /* 设置第一目录的第一张图为壁纸 */
    FileList list;
    int n = fl_scan(&list, cfg.folders[0], &cfg);
    if (n == 0) {
        dialog_error(NULL, L"目录模式", L"目录内没有可用的壁纸文件。");
        fl_free(&list);
        config_free(&cfg);
        heap_free(folder);
        return 1;
    }
    fl_sort(&list, SORT_NAME, 0);
    wallpaper_set(list.items[0].path);
    heap_free(cfg.current_file);
    cfg.current_file = wcs_dup(list.items[0].name);
    config_save(&cfg);
    fl_free(&list);

    /* 定时任务：自动轮播 */
    if (task_create(cfg.toggle_interval) != 0)
        LOGW(L"Failed to create scheduled task (auto-rotation unavailable)");

    registry_install_desktop_menu(menu_code(&cfg), &cfg, freeze_label());
    LOGI(L"Folder mode set: %s (%d folders)", folder, nf);

    config_free(&cfg);
    heap_free(folder);
    return 0;
}

static int cmd_toggle_file(const wchar_t *param) {
    if (!param || (!wcs_ieq(param, L"Next") && !wcs_ieq(param, L"Last")))
        param = L"Next";

    AppConfig cfg;
    config_init(&cfg);
    if (config_load(&cfg) != 0) {
        dialog_error(NULL, L"切换壁纸", L"尚未设置壁纸目录。");
        config_free(&cfg);
        return 1;
    }
    if (wcs_ieq(cfg.mode, CFG_MODE_FILE)) {
        LOGI(L"File mode does not support switching");
        config_free(&cfg);
        return 1;
    }
    int rc = toggle_file(param, &cfg);
    config_free(&cfg);
    return rc;
}

static int cmd_toggle_mode(const wchar_t *param) {
    if (!param || wcslen(param) < 1) return 1;
    wchar_t c = (wchar_t)towupper(param[0]);
    if (c != L'N' && c != L'D' && c != L'S' && c != L'J' && c != L'Z') return 1;

    AppConfig cfg;
    config_init(&cfg);
    if (config_load(&cfg) != 0) {
        dialog_error(NULL, L"设置切换模式", L"尚未设置壁纸目录。");
        config_free(&cfg);
        return 1;
    }
    if (wcs_ieq(cfg.mode, CFG_MODE_FILE)) { config_free(&cfg); return 1; }

    wchar_t old = cfg.toggle_mode ? cfg.toggle_mode[0] : L'N';
    wchar_t olddir = (cfg.toggle_mode && cfg.toggle_mode[1]) ? cfg.toggle_mode[1] : L'+';
    wchar_t newmode[4];

    if (old == c) {
        wchar_t nd = (olddir == L'-') ? L'+' : L'-';
        StringCchPrintfW(newmode, ARRAYSIZE(newmode), L"%c%c", c, nd);
    } else {
        StringCchPrintfW(newmode, ARRAYSIZE(newmode), L"%c%c", c, olddir);
    }
    if (c == L'Z') {
        StringCchPrintfW(newmode, ARRAYSIZE(newmode), L"Z");
    }

    heap_free(cfg.toggle_mode);
    cfg.toggle_mode = wcs_dup(newmode);
    cfg.file_index = -1;
    config_save(&cfg);
    registry_install_desktop_menu(menu_code(&cfg), &cfg, freeze_label()); /* 刷新“(当前)”标记 */

    int rc = toggle_file(L"Next", &cfg);
    config_free(&cfg);
    return rc;
}

static int cmd_toggle_time(void) {
    AppConfig cfg;
    config_init(&cfg);
    if (config_load(&cfg) != 0) {
        dialog_error(NULL, L"设置切换时间", L"尚未设置壁纸目录。");
        config_free(&cfg);
        return 1;
    }

    int val = cfg.toggle_interval;
    if (dialog_input_number(NULL, L"设置切换时间",
                            L"请输入新的切换时间（分钟）[1-999]：",
                            1, 999, cfg.toggle_interval, &val)) {
        cfg.toggle_interval = val;
        config_save(&cfg);
        if (task_create(val) != 0)
            LOGW(L"Failed to update scheduled task");
        registry_install_desktop_menu(menu_code(&cfg), &cfg, freeze_label());
        LOGI(L"Toggle interval updated: %d minutes", val);
    } else {
        LOGI(L"Toggle time setting cancelled");
    }
    config_free(&cfg);
    return 0;
}

static int cmd_locate(void) {
    wchar_t *cur = wallpaper_get_current();
    if (!cur || !*cur) {
        dialog_error(NULL, L"定位壁纸", L"未找到壁纸信息。");
        heap_free(cur);
        return 1;
    }
    wchar_t args[32768];
    StringCchPrintfW(args, ARRAYSIZE(args), L"/select,\"%s\"", cur);
    ShellExecuteW(NULL, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
    LOGI(L"Located wallpaper: %s", cur);
    heap_free(cur);
    return 0;
}

static int cmd_hidden(void) {
    AppConfig cfg;
    config_init(&cfg);
    if (config_load(&cfg) != 0) {
        dialog_error(NULL, L"隐藏壁纸", L"尚未设置壁纸目录。");
        config_free(&cfg);
        return 1;
    }
    if (cfg.current_file && *cfg.current_file) {
        config_add_hidden(&cfg, cfg.current_file);
        config_save(&cfg);
        LOGI(L"Wallpaper hidden: %s", cfg.current_file);
    }
    int rc = toggle_file(L"Next", &cfg);
    config_free(&cfg);
    return rc;
}

static int cmd_freeze(void) {
    AppConfig cfg;
    config_init(&cfg);
    config_load(&cfg); /* 忽略失败，使用默认值 */

    int enabled = task_is_enabled();
    if (enabled == 1) {
        task_set_enabled(FALSE);
        LOGI(L"Wallpaper frozen (auto-rotation paused)");
    } else {
        if (task_exists() == 0) task_create(cfg.toggle_interval);
        else                    task_set_enabled(TRUE);
        LOGI(L"Wallpaper unfrozen (auto-rotation resumed)");
    }

    registry_install_desktop_menu(menu_code(&cfg), &cfg, freeze_label());
    config_free(&cfg);
    return 0;
}

static int cmd_toggle_folder(const wchar_t *param) {
    AppConfig cfg;
    config_init(&cfg);
    if (config_load(&cfg) != 0) {
        dialog_error(NULL, L"切换目录", L"尚未设置壁纸目录。");
        config_free(&cfg);
        return 1;
    }
    if (!wcs_ieq(cfg.mode, CFG_MODE_MORE) || cfg.folder_count < 2) {
        config_free(&cfg);
        return 1;
    }

    int dir = wcs_ieq(param, L"Last") ? -1 : 1;
    int n = cfg.folder_count;
    int id = ((cfg.current_id - 1 + dir) % n + n) % n;
    cfg.current_id = id + 1;
    cfg.folder_index = id;

    wchar_t *folder = current_folder_path(&cfg);
    FileList list;
    int cnt = fl_scan(&list, folder, &cfg);
    if (cnt == 0) {
        LOGE(L"No images in folder: %s", folder);
        dialog_error(NULL, L"切换目录", L"目标目录内没有可用的壁纸文件。");
        fl_free(&list);
        heap_free(folder);
        config_free(&cfg);
        return 1;
    }
    int sort = sort_from_mode(cfg.toggle_mode[0]);
    unsigned long long seed = rng_seed_from_string(folder);
    fl_sort(&list, sort, seed);

    int idx;
    if (cfg.toggle_mode[0] == L'Z') {
        unsigned long long st = rng_seed_from_time();
        idx = (int)rng_range(&st, (unsigned long long)list.count);
    } else {
        idx = 0; /* 切目录后显示新目录第一张 */
    }

    const FileEntry *e = &list.items[idx];
    int ok = wallpaper_set(e->path);
    cfg.file_index = idx;
    heap_free(cfg.current_file);    cfg.current_file = wcs_dup(e->name);
    heap_free(cfg.current_folder);  cfg.current_folder = path_basename(folder);
    config_save(&cfg);
    if (ok == 0) LOGI(L"Switched to folder %d/%d: %s", cfg.current_id, n, folder);
    else         LOGE(L"Failed to switch folder: %s", folder);

    fl_free(&list);
    heap_free(folder);
    config_free(&cfg);
    return ok;
}

/* ---------- 分发 ---------- */
static int dispatch(const wchar_t *cmd, const wchar_t *param) {
    LOGI(L"Command: %s  Arg: %s", cmd, param ? param : L"(none)");

    if (wcs_ieq(cmd, L"Install"))         return cmd_install();
    if (wcs_ieq(cmd, L"UnInstall"))       return cmd_uninstall();
    if (wcs_ieq(cmd, L"File_Mode"))       return cmd_file_mode(param);
    if (wcs_ieq(cmd, L"Folder_Mode"))     return cmd_folder_mode(param);
    if (wcs_ieq(cmd, L"Toggle_Time"))     return cmd_toggle_time();
    if (wcs_ieq(cmd, L"Toggle_Mode"))     return cmd_toggle_mode(param);
    if (wcs_ieq(cmd, L"Toggle_File"))     return cmd_toggle_file(param);
    if (wcs_ieq(cmd, L"Toggle_Folder"))   return cmd_toggle_folder(param);
    if (wcs_ieq(cmd, L"Locate_Wallpaper"))return cmd_locate();
    if (wcs_ieq(cmd, L"Freeze_Wallpaper"))return cmd_freeze();
    if (wcs_ieq(cmd, L"Hidden_Wallpaper"))return cmd_hidden();

    /* 直接传入路径：目录 → 目录模式；文件 → 文件模式 */
    if (path_is_dir(cmd))  return cmd_folder_mode(cmd);
    if (path_is_file(cmd)) return cmd_file_mode(cmd);

    LOGE(L"Unknown command: %s", cmd);
    return 1;
}

/* ---------- 入口 ---------- */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    log_init();
    LOGI(L"===== WallpaperManager started =====");

    ui_init();
    int com_ok = (com_init() == 0);
    wallpaper_enable_lock_screen();

    HANDLE lock = acquire_process_lock(L"Local\\WallpaperManager");
    if (!lock) {
        LOGW(L"Another instance is running; exiting");
        if (com_ok) com_uninit();
        log_close();
        return 0;
    }

    int rc = 0;
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        LOGI(L"No command-line arguments");
        dialog_info(NULL, L"WallpaperManager", L"请查阅相关指令后以命令行的方式运行本程序。");
    } else {
        const wchar_t *param = (argc >= 3) ? argv[2] : NULL;
        rc = dispatch(argv[1], param);
    }

    if (argv) LocalFree(argv);
    CloseHandle(lock);
    LOGI(L"===== WallpaperManager finished (rc=%d) =====", rc);
    if (com_ok) com_uninit();
    log_close();
    return rc;
}
