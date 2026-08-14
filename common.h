/* common.h — 公共头文件
 * 集中管理：包含、内存、字符串、路径、UTF-8 转换、随机数、进程锁等基础工具。
 * 全程宽字符（UTF-16），支持中文/韩文/日文/泰文等所有 Unicode 字符。
 */
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7+ */
#endif

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---------- 内存 ---------- */
void *heap_alloc(size_t sz, const char *file, int line);
void *heap_realloc(void *p, size_t sz, const char *file, int line);
void  heap_free(void *p);
#define xmalloc(sz)   heap_alloc((sz), __FILE__, __LINE__)
#define xrealloc(p,sz) heap_realloc((p), (sz), __FILE__, __LINE__)

/* ---------- COM / 通用控件初始化 ---------- */
int  com_init(void);
void com_uninit(void);
void ui_init(void);

/* ---------- 字符串工具 ---------- */
wchar_t *wcs_dup(const wchar_t *s);
wchar_t *wcs_dupn(const wchar_t *s, size_t n);
wchar_t *wcs_concat2(const wchar_t *a, const wchar_t *b);
wchar_t *wcs_concat3(const wchar_t *a, const wchar_t *b, const wchar_t *c);
int   wcs_ieq(const wchar_t *a, const wchar_t *b);
int   wcs_startswith_ci(const wchar_t *s, const wchar_t *pfx);
int   wcs_endswith_ci(const wchar_t *s, const wchar_t *sfx);
void  wcs_trim(wchar_t *s);
int   wcs_to_int(const wchar_t *s, int *out);

/* ---------- 动态宽字符串 ---------- */
typedef struct {
    wchar_t *data;
    size_t   len;
    size_t   cap;
} WStr;
void     wstr_init(WStr *ws);
void     wstr_append(WStr *ws, const wchar_t *s);
void     wstr_appendf(WStr *ws, const wchar_t *fmt, ...);
wchar_t *wstr_detach(WStr *ws);
void     wstr_free(WStr *ws);

/* ---------- 路径工具 ---------- */
int       path_is_dir(const wchar_t *p);
int       path_is_file(const wchar_t *p);
wchar_t  *path_join(const wchar_t *dir, const wchar_t *name);
wchar_t  *path_dirname(const wchar_t *p);
wchar_t  *path_basename(const wchar_t *p);
wchar_t  *path_get_exe_dir(void);
wchar_t  *path_get_exe_path(void);

/* ---------- UTF-8 转换 ---------- */
char    *wide_to_utf8(const wchar_t *w);
wchar_t *utf8_to_wide(const char *u);

/* ---------- 文件读写（UTF-8 文本） ---------- */
char *read_text_file_utf8(const wchar_t *path, size_t *out_len);
int   write_text_file_utf8(const wchar_t *path, const char *data, size_t len);

/* ---------- 随机数 ---------- */
unsigned long long rng_seed_from_string(const wchar_t *s);
unsigned long long rng_seed_from_time(void);
unsigned long long rng_next(unsigned long long *state);
unsigned long long rng_range(unsigned long long *state, unsigned long long n);

/* ---------- 进程互斥锁（防止重复实例） ---------- */
HANDLE acquire_process_lock(const wchar_t *name);

/* ---------- 管理员权限检测 ---------- */
int is_admin(void);
