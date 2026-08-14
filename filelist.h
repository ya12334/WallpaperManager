/* filelist.h — 文件扫描与排序模块
 * 使用 FindFirstFile/FindNextFile 扫描目录，内存 qsort 排序，不产生任何临时文件。
 */
#pragma once
#include "common.h"
#include "config.h"

#define SORT_NAME   0 /* 名称升序 */
#define SORT_DATE   1 /* 日期升序（旧→新） */
#define SORT_SIZE   2 /* 大小升序（小→大） */
#define SORT_RANDOM 3 /* 伪随机（确定性洗牌） */

typedef struct {
    wchar_t   *name;   /* 仅文件名 */
    wchar_t   *path;   /* 完整路径 */
    ULONGLONG  size;
    FILETIME   mtime;
    int        hidden;
} FileEntry;

typedef struct {
    FileEntry *items;
    int        count;
    int        cap;
} FileList;

/* 判断文件是否为支持的图片类型（Jpg/Jpeg/Png/Bmp/Gif/Tif/Tiff/Heic/Heif/Avif） */
int is_image_ext(const wchar_t *filename);

/* 扫描 dir 顶层图片文件（非递归），过滤隐藏文件，返回文件数量（0 表示无） */
int fl_scan(FileList *list, const wchar_t *dir, const AppConfig *cfg);

/* 原地排序；rng_seed 仅用于 SORT_RANDOM（由目录路径派生，保证顺序稳定） */
void fl_sort(FileList *list, int sort_mode, unsigned long long rng_seed);

void fl_free(FileList *list);

/* 递归判断目录（含子目录）内是否存在至少一张非空图片文件 */
int dir_contains_image(const wchar_t *dir);

/* ---------- 目录列表（More Folder 模式） ---------- */
typedef struct {
    wchar_t **paths;   /* 含图片目录的完整路径（DFS，每层子目录按名称排序） */
    int       count;
    int       cap;
} FolderList;

/* 递归收集 dir 及其所有子目录中“顶层直接含图片”的目录，返回目录数量 */
int fl_scan_image_folders(FolderList *folders, const wchar_t *dir);

void fl_free_folders(FolderList *folders);
