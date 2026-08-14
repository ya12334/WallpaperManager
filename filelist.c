/* filelist.c — 文件扫描与排序实现 */
#include "filelist.h"

static const wchar_t *g_exts[] = {
    L"jpg", L"jpeg", L"png", L"bmp", L"gif",
    L"tif", L"tiff", L"heic", L"heif", L"avif"
};
#define G_EXT_COUNT (sizeof(g_exts) / sizeof(g_exts[0]))

int is_image_ext(const wchar_t *filename) {
    const wchar_t *dot = wcsrchr(filename, L'.');
    if (!dot) return 0;
    dot++;
    for (size_t i = 0; i < G_EXT_COUNT; i++)
        if (wcs_ieq(dot, g_exts[i])) return 1;
    return 0;
}

int fl_scan(FileList *list, const wchar_t *dir, const AppConfig *cfg) {
    list->items = NULL;
    list->count = 0;
    list->cap = 0;

    wchar_t *pattern = path_join(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    heap_free(pattern);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_image_ext(fd.cFileName)) continue;
        if (fd.nFileSizeHigh == 0 && fd.nFileSizeLow == 0) continue; /* 空文件 */
        if (cfg && config_is_hidden(cfg, fd.cFileName)) continue;

        if (list->count >= list->cap) {
            list->cap = list->cap ? list->cap * 2 : 64;
            list->items = xrealloc(list->items, (size_t)list->cap * sizeof(FileEntry));
        }
        FileEntry *e = &list->items[list->count++];
        e->name = wcs_dup(fd.cFileName);
        e->path = path_join(dir, fd.cFileName);
        e->size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        e->mtime = fd.ftLastWriteTime;
        e->hidden = 0;
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return list->count;
}

static int cmp_name(const void *a, const void *b) {
    const FileEntry *x = a, *y = b;
    return _wcsicmp(x->name, y->name);
}

static int cmp_date(const void *a, const void *b) {
    const FileEntry *x = a, *y = b;
    return CompareFileTime(&x->mtime, &y->mtime);
}

static int cmp_size(const void *a, const void *b) {
    const FileEntry *x = a, *y = b;
    if (x->size < y->size) return -1;
    if (x->size > y->size) return 1;
    return _wcsicmp(x->name, y->name);
}

void fl_sort(FileList *list, int sort_mode, unsigned long long rng_seed) {
    if (list->count <= 1) return;
    switch (sort_mode) {
        case SORT_DATE:
            qsort(list->items, list->count, sizeof(FileEntry), cmp_date);
            break;
        case SORT_SIZE:
            qsort(list->items, list->count, sizeof(FileEntry), cmp_size);
            break;
        case SORT_RANDOM: {
            unsigned long long st = rng_seed;
            for (int i = list->count - 1; i > 0; i--) {
                int j = (int)rng_range(&st, (unsigned long long)(i + 1));
                FileEntry tmp = list->items[i];
                list->items[i] = list->items[j];
                list->items[j] = tmp;
            }
            break;
        }
        case SORT_NAME:
        default:
            qsort(list->items, list->count, sizeof(FileEntry), cmp_name);
            break;
    }
}

void fl_free(FileList *list) {
    for (int i = 0; i < list->count; i++) {
        heap_free(list->items[i].name);
        heap_free(list->items[i].path);
    }
    heap_free(list->items);
    list->items = NULL;
    list->count = list->cap = 0;
}

int dir_contains_image(const wchar_t *dir) {
    wchar_t *pattern = path_join(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    heap_free(pattern);
    if (h == INVALID_HANDLE_VALUE) return 0;

    int found = 0;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wchar_t *sub = path_join(dir, fd.cFileName);
            int r = dir_contains_image(sub);
            heap_free(sub);
            if (r) { found = 1; break; }
        } else {
            if (is_image_ext(fd.cFileName) && (fd.nFileSizeHigh != 0 || fd.nFileSizeLow != 0)) {
                found = 1;
                break;
            }
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return found;
}

/* ---------- 目录列表（More Folder 模式） ---------- */

/* 判断 dir 顶层是否直接含至少一张非空图片文件（不递归） */
static int dir_has_image_top(const wchar_t *dir) {
    wchar_t *pattern = path_join(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    heap_free(pattern);
    if (h == INVALID_HANDLE_VALUE) return 0;

    int found = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (is_image_ext(fd.cFileName) && (fd.nFileSizeHigh != 0 || fd.nFileSizeLow != 0)) {
            found = 1;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

static void folder_list_add(FolderList *f, const wchar_t *path) {
    if (f->count >= f->cap) {
        f->cap = f->cap ? f->cap * 2 : 8;
        f->paths = xrealloc(f->paths, (size_t)f->cap * sizeof(wchar_t *));
    }
    f->paths[f->count++] = wcs_dup(path);
}

static int cmp_wcs_ptr(const void *a, const void *b) {
    const wchar_t *x = *(const wchar_t *const *)a;
    const wchar_t *y = *(const wchar_t *const *)b;
    return _wcsicmp(x, y);
}

/* 递归：收集 dir（若直接含图片）及其所有子目录中直接含图片的目录 */
static void scan_folders_rec(FolderList *f, const wchar_t *dir) {
    if (dir_has_image_top(dir)) folder_list_add(f, dir);

    wchar_t *pattern = path_join(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    heap_free(pattern);
    if (h == INVALID_HANDLE_VALUE) return;

    /* 先收集并排序子目录名，保证顺序确定 */
    wchar_t **subs = NULL;
    int n = 0, cap = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; /* 跳过链接防循环 */
        if (n >= cap) { cap = cap ? cap * 2 : 16; subs = xrealloc(subs, (size_t)cap * sizeof(wchar_t *)); }
        subs[n++] = wcs_dup(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    qsort(subs, n, sizeof(wchar_t *), cmp_wcs_ptr);
    for (int i = 0; i < n; i++) {
        wchar_t *sub = path_join(dir, subs[i]);
        scan_folders_rec(f, sub);
        heap_free(sub);
        heap_free(subs[i]);
    }
    heap_free(subs);
}

int fl_scan_image_folders(FolderList *folders, const wchar_t *dir) {
    folders->paths = NULL;
    folders->count = 0;
    folders->cap = 0;
    scan_folders_rec(folders, dir);
    return folders->count;
}

void fl_free_folders(FolderList *folders) {
    for (int i = 0; i < folders->count; i++) heap_free(folders->paths[i]);
    heap_free(folders->paths);
    folders->paths = NULL;
    folders->count = folders->cap = 0;
}
