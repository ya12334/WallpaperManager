/* config.h — 配置模块（方案 A：简洁分段文本格式）
 *
 * 配置文件 WallpaperManager.cfg 为 UTF-8 文本：
 *   [State]    键值对（Current_ID / Current_Mode / File_Index / Folder_Index /
 *              Toggle_Time / Toggle_Mode / Current_File / Current_Folder / Root_Path）
 *   [Folders]  每行一个目录完整路径（仅单目录/多目录模式写入）
 *   [Hidden]   每行一个被隐藏（跳过）的文件名（仅存在隐藏时写入）
 *
 * 文件列表不持久化，运行时用 FindFirstFile + qsort 重算。
 */
#pragma once
#include "common.h"

#define CFG_MODE_FILE     L"File Mode"
#define CFG_MODE_SINGLE   L"Single Folder Mode"
#define CFG_MODE_MORE     L"More Folder Mode"

typedef struct {
    wchar_t *mode;           /* File Mode / Single Folder Mode / More Folder Mode */
    int      toggle_interval;/* 切换间隔（分钟） */
    wchar_t *toggle_mode;    /* 首字符 N/D/S/J/Z + 方向 +/- */
    int      current_id;     /* 当前目录编号（1 基，More Folder 模式） */
    int      file_index;     /* 当前文件在排序序列中的下标（0 基） */
    int      folder_index;   /* 当前目录在 folders 列表中的下标（0 基） */
    wchar_t *current_file;   /* 当前壁纸文件名 */
    wchar_t *current_folder; /* 当前目录名 */
    wchar_t *root_path;      /* 根路径 */

    wchar_t **folders;       /* 目录列表（完整路径） */
    int       folder_count;
    wchar_t **hidden;        /* 隐藏文件名列表 */
    int       hidden_count;
} AppConfig;

void     config_init(AppConfig *cfg);
void     config_free(AppConfig *cfg);
int      config_load(AppConfig *cfg);   /* 0 成功；-1 文件不存在（保持默认） */
int      config_save(const AppConfig *cfg);
wchar_t *config_get_path(void);

int  config_is_hidden(const AppConfig *cfg, const wchar_t *filename);
void config_add_hidden(AppConfig *cfg, const wchar_t *filename);

/* 目录列表操作（More Folder 模式） */
void config_clear_folders(AppConfig *cfg);
void config_add_folder(AppConfig *cfg, const wchar_t *path);
