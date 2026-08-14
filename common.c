/* common.c — 公共工具实现 */
#include "common.h"

/* ---------- 内存 ---------- */
void *heap_alloc(size_t sz, const char *file, int line) {
    void *p = malloc(sz ? sz : 1);
    if (!p) {
        char dbg[512];
        StringCchPrintfA(dbg, ARRAYSIZE(dbg),
            "WallpaperManager 内存分配失败: %zu 字节 @ %s:%d\n", sz, file, line);
        OutputDebugStringA(dbg);
        MessageBoxW(NULL, L"内存分配失败，程序即将退出。", L"WallpaperManager",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
    return p;
}

void *heap_realloc(void *p, size_t sz, const char *file, int line) {
    void *q = realloc(p, sz ? sz : 1);
    if (!q) {
        char dbg[512];
        StringCchPrintfA(dbg, ARRAYSIZE(dbg),
            "WallpaperManager 内存重分配失败: %zu 字节 @ %s:%d\n", sz, file, line);
        OutputDebugStringA(dbg);
        MessageBoxW(NULL, L"内存分配失败，程序即将退出。", L"WallpaperManager",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
    return q;
}

void heap_free(void *p) { if (p) free(p); }

/* ---------- COM / 通用控件 ---------- */
int com_init(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    return (hr == S_OK || hr == S_FALSE) ? 0 : -1;
}

void com_uninit(void) { CoUninitialize(); }

void ui_init(void) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
}

/* ---------- 字符串工具 ---------- */
wchar_t *wcs_dup(const wchar_t *s) {
    if (!s) return wcs_dup(L"");
    size_t n = wcslen(s);
    wchar_t *r = xmalloc((n + 1) * sizeof(wchar_t));
    memcpy(r, s, (n + 1) * sizeof(wchar_t));
    return r;
}

wchar_t *wcs_dupn(const wchar_t *s, size_t n) {
    wchar_t *r = xmalloc((n + 1) * sizeof(wchar_t));
    memcpy(r, s, n * sizeof(wchar_t));
    r[n] = 0;
    return r;
}

wchar_t *wcs_concat2(const wchar_t *a, const wchar_t *b) {
    size_t la = wcslen(a), lb = wcslen(b);
    wchar_t *r = xmalloc((la + lb + 1) * sizeof(wchar_t));
    memcpy(r, a, la * sizeof(wchar_t));
    memcpy(r + la, b, lb * sizeof(wchar_t));
    r[la + lb] = 0;
    return r;
}

wchar_t *wcs_concat3(const wchar_t *a, const wchar_t *b, const wchar_t *c) {
    size_t la = wcslen(a), lb = wcslen(b), lc = wcslen(c);
    wchar_t *r = xmalloc((la + lb + lc + 1) * sizeof(wchar_t));
    memcpy(r, a, la * sizeof(wchar_t));
    memcpy(r + la, b, lb * sizeof(wchar_t));
    memcpy(r + la + lb, c, lc * sizeof(wchar_t));
    r[la + lb + lc] = 0;
    return r;
}

int wcs_ieq(const wchar_t *a, const wchar_t *b) {
    return _wcsicmp(a, b) == 0;
}

int wcs_startswith_ci(const wchar_t *s, const wchar_t *pfx) {
    return _wcsnicmp(s, pfx, wcslen(pfx)) == 0;
}

int wcs_endswith_ci(const wchar_t *s, const wchar_t *sfx) {
    size_t ls = wcslen(s), lp = wcslen(sfx);
    if (lp > ls) return 0;
    return _wcsicmp(s + ls - lp, sfx) == 0;
}

void wcs_trim(wchar_t *s) {
    wchar_t *start = s;
    while (*start == L' ' || *start == L'\t') start++;
    if (start != s) memmove(s, start, (wcslen(start) + 1) * sizeof(wchar_t));
    size_t len = wcslen(s);
    while (len > 0 && (s[len - 1] == L' ' || s[len - 1] == L'\t' ||
                       s[len - 1] == L'\r' || s[len - 1] == L'\n'))
        s[--len] = 0;
}

int wcs_to_int(const wchar_t *s, int *out) {
    return swscanf(s, L"%d", out) == 1;
}

/* ---------- 动态宽字符串 ---------- */
void wstr_init(WStr *ws) {
    ws->cap = 256;
    ws->len = 0;
    ws->data = xmalloc(ws->cap * sizeof(wchar_t));
    ws->data[0] = 0;
}

static void wstr_ensure(WStr *ws, size_t extra) {
    if (ws->len + extra + 1 > ws->cap) {
        while (ws->len + extra + 1 > ws->cap) ws->cap *= 2;
        ws->data = xrealloc(ws->data, ws->cap * sizeof(wchar_t));
    }
}

void wstr_append(WStr *ws, const wchar_t *s) {
    size_t n = wcslen(s);
    wstr_ensure(ws, n);
    memcpy(ws->data + ws->len, s, (n + 1) * sizeof(wchar_t));
    ws->len += n;
}

void wstr_appendf(WStr *ws, const wchar_t *fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    StringCchVPrintfW(buf, ARRAYSIZE(buf), fmt, ap);
    va_end(ap);
    wstr_append(ws, buf);
}

wchar_t *wstr_detach(WStr *ws) {
    wchar_t *r = ws->data;
    ws->data = NULL;
    ws->len = ws->cap = 0;
    return r;
}

void wstr_free(WStr *ws) {
    if (ws->data) heap_free(ws->data);
    ws->data = NULL;
    ws->len = ws->cap = 0;
}

/* ---------- 路径工具 ---------- */
int path_is_dir(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int path_is_file(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

wchar_t *path_join(const wchar_t *dir, const wchar_t *name) {
    size_t dl = wcslen(dir);
    if (dl > 0 && (dir[dl - 1] == L'\\' || dir[dl - 1] == L'/'))
        return wcs_concat2(dir, name);
    return wcs_concat3(dir, L"\\", name);
}

wchar_t *path_dirname(const wchar_t *p) {
    wchar_t *d = wcs_dup(p);
    size_t len = wcslen(d);
    while (len > 0 && (d[len - 1] == L'\\' || d[len - 1] == L'/')) d[--len] = 0;
    wchar_t *slash = wcsrchr(d, L'\\');
    wchar_t *slash2 = wcsrchr(d, L'/');
    if (slash2 && (!slash || slash2 > slash)) slash = slash2;
    if (slash) *slash = 0;
    else d[0] = 0;
    return d;
}

wchar_t *path_basename(const wchar_t *p) {
    const wchar_t *b = wcsrchr(p, L'\\');
    const wchar_t *s = wcsrchr(p, L'/');
    if (s && (!b || s > b)) b = s;
    return wcs_dup(b ? b + 1 : p);
}

wchar_t *path_get_exe_path(void) {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(NULL, buf, ARRAYSIZE(buf));
    if (n == 0 || n >= ARRAYSIZE(buf)) return wcs_dup(L"");
    return wcs_dup(buf);
}

wchar_t *path_get_exe_dir(void) {
    wchar_t *p = path_get_exe_path();
    wchar_t *slash = wcsrchr(p, L'\\');
    if (slash) *slash = 0;
    return p;
}

/* ---------- UTF-8 转换 ---------- */
char *wide_to_utf8(const wchar_t *w) {
    if (!w) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *u = xmalloc(n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, u, n, NULL, NULL);
    return u;
}

wchar_t *utf8_to_wide(const char *u) {
    if (!u) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = xmalloc(n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, u, -1, w, n);
    return w;
}

/* ---------- 文件读写（UTF-8 文本） ---------- */
char *read_text_file_utf8(const wchar_t *path, size_t *out_len) {
    HANDLE h = CreateFileW(path, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz;
    GetFileSizeEx(h, &sz);
    size_t n = (size_t)sz.QuadPart;
    char *buf = xmalloc(n + 1);
    DWORD read = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)n, &read, NULL);
    CloseHandle(h);
    if (!ok) { heap_free(buf); return NULL; }
    buf[read] = 0;
    if (out_len) *out_len = read;
    return buf;
}

int write_text_file_utf8(const wchar_t *path, const char *data, size_t len) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, (DWORD)len, &written, NULL);
    CloseHandle(h);
    return (ok && written == len) ? 0 : -1;
}

/* ---------- 随机数 ---------- */
unsigned long long rng_seed_from_time(void) {
    unsigned long long s = (unsigned long long)GetTickCount64();
    s ^= ((unsigned long long)GetCurrentProcessId()) << 32;
    s ^= (unsigned long long)(uintptr_t)&s;
    return s ? s : 0x9E3779B97F4A7C15ULL;
}

unsigned long long rng_seed_from_string(const wchar_t *s) {
    unsigned long long h = 1469598103934665603ULL; /* FNV-1a 64-bit */
    if (s) {
        const unsigned char *b = (const unsigned char *)s;
        size_t n = wcslen(s) * sizeof(wchar_t);
        for (size_t i = 0; i < n; i++) {
            h ^= b[i];
            h *= 1099511628211ULL;
        }
    }
    return h ? h : 1;
}

unsigned long long rng_next(unsigned long long *state) {
    /* splitmix64 */
    unsigned long long z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

unsigned long long rng_range(unsigned long long *state, unsigned long long n) {
    if (n == 0) return 0;
    return rng_next(state) % n;
}

/* ---------- 进程互斥锁 ---------- */
HANDLE acquire_process_lock(const wchar_t *name) {
    HANDLE h = CreateMutexW(NULL, FALSE, name);
    if (!h) return NULL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return NULL;
    }
    return h;
}

/* ---------- 管理员权限检测 ---------- */
int is_admin(void) {
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admin = NULL;
    BOOL member = FALSE;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin)) {
        CheckTokenMembership(NULL, admin, &member);
        FreeSid(admin);
    }
    return member ? 1 : 0;
}
