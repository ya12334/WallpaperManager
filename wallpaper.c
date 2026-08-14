/* wallpaper.c — 壁纸应用实现 */
#include "wallpaper.h"
#include "log.h"

#define REG_DESKTOP     L"Control Panel\\Desktop"
#define REG_LOCKSCREEN  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PersonalizationCSP"

#define SPI_SETDESKWALLPAPER 0x0014
#define SPIF_SENDWININICHANGE 0x0002

static int reg_set_sz(HKEY root, const wchar_t *sub, const wchar_t *name,
                      const wchar_t *value) {
    HKEY key;
    LONG rc = RegCreateKeyExW(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return -1;
    rc = RegSetValueExW(key, name, 0, REG_SZ,
                        (const BYTE *)value,
                        (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return (rc == ERROR_SUCCESS) ? 0 : -1;
}

static int reg_set_dword(HKEY root, const wchar_t *sub, const wchar_t *name, DWORD v) {
    HKEY key;
    LONG rc = RegCreateKeyExW(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return -1;
    rc = RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&v, sizeof(v));
    RegCloseKey(key);
    return (rc == ERROR_SUCCESS) ? 0 : -1;
}

void wallpaper_enable_lock_screen(void) {
    /* 非管理员无 HKLM 写入权限，直接跳过，避免每次切换产生无效写入与日志噪音 */
    if (!is_admin()) return;
    if (reg_set_dword(HKEY_LOCAL_MACHINE, REG_LOCKSCREEN, L"LogonImageStatus", 1) != 0)
        LOGW(L"Failed to write lock screen state (LogonImageStatus)");
    if (reg_set_dword(HKEY_LOCAL_MACHINE, REG_LOCKSCREEN, L"LockScreenImageStatus", 1) != 0)
        LOGW(L"Failed to write lock screen state (LockScreenImageStatus)");
}

/* ---------- GDI+ flat API 手动声明（C 环境；官方头文件为 C++ 命名空间语法） ---------- */
typedef INT GpStatus;
typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void   *DebugEventCallback;
    BOOL    SuppressBackgroundThread;
    BOOL    SuppressExternalCodecs;
} GdiplusStartupInput;
typedef struct GdiplusStartupOutput {
    void *NotificationHook;
    void *NotificationUnhook;
} GdiplusStartupOutput;
typedef void *GpGraphics;
typedef void *GpImage;
typedef void *GpBitmap;

#define GDIP_OK          0
#define GDIP_UNIT_PIXEL  2

GpStatus WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input,
                               GdiplusStartupOutput *output);
void WINAPI GdiplusShutdown(ULONG_PTR token);
GpStatus WINAPI GdipCreateBitmapFromFile(const WCHAR *filename, GpBitmap **bitmap);
GpStatus WINAPI GdipGetImageWidth(GpImage *image, UINT *width);
GpStatus WINAPI GdipGetImageHeight(GpImage *image, UINT *height);
GpStatus WINAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);
GpStatus WINAPI GdipDeleteGraphics(GpGraphics *graphics);
GpStatus WINAPI GdipDisposeImage(GpImage *image);
GpStatus WINAPI GdipDrawImageRectRectI(GpGraphics *graphics, GpImage *image,
    INT dstx, INT dsty, INT dstwidth, INT dstheight,
    INT srcx, INT srcy, INT srcwidth, INT srcheight,
    INT unit, const void *imageAttributes, void *callback, void *callbackData);

static int wallpaper_apply(const wchar_t *path); /* 前向声明（实际应用壁纸） */

/* ---------- 壁纸切换过渡动画（交叉淡化，仅作用于壁纸层） ---------- */
static ULONG_PTR g_gdip_token = 0;
static int       g_gdip_ready = 0;

static int gdip_ensure(void) {
    if (g_gdip_ready) return 1;
    GdiplusStartupInput in;
    in.GdiplusVersion = 1;
    in.DebugEventCallback = NULL;
    in.SuppressBackgroundThread = FALSE;
    in.SuppressExternalCodecs = FALSE;
    GdiplusStartupOutput out;
    out.NotificationHook = NULL;
    out.NotificationUnhook = NULL;
    if (GdiplusStartup(&g_gdip_token, &in, &out) != GDIP_OK) return 0;
    g_gdip_ready = 1;
    return 1;
}

/* 找到承载桌面图标的 WorkerW（其下方即壁纸层），用于确定覆盖层的 Z 序位置 */
static BOOL CALLBACK find_workerw_enum(HWND hwnd, LPARAM lp) {
    if (FindWindowExW(hwnd, NULL, L"SHELLDLL_DefView", NULL)) {
        *(HWND *)lp = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND find_workerw(void) {
    HWND progman = FindWindowW(L"Progman", NULL);
    if (progman)
        SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, NULL);
    HWND w = NULL;
    EnumWindows(find_workerw_enum, (LPARAM)&w);
    return w;
}

/* 读取壁纸显示样式（WallpaperStyle / TileWallpaper） */
static void read_wallpaper_style(int *style, int *tile) {
    *style = 10; *tile = 0; /* 默认：填充 */
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_DESKTOP, 0, KEY_QUERY_VALUE, &k)
        != ERROR_SUCCESS)
        return;
    wchar_t buf[64]; DWORD sz = sizeof(buf); DWORD ty = 0;
    if (RegQueryValueExW(k, L"WallpaperStyle", NULL, &ty, (BYTE *)buf, &sz) == ERROR_SUCCESS
        && ty == REG_SZ)
        *style = (int)wcstol(buf, NULL, 10);
    sz = sizeof(buf);
    if (RegQueryValueExW(k, L"TileWallpaper", NULL, &ty, (BYTE *)buf, &sz) == ERROR_SUCCESS
        && ty == REG_SZ)
        *tile = (int)wcstol(buf, NULL, 10);
    RegCloseKey(k);
}

/* 覆盖层窗口与渲染缓冲区 */
static HWND    g_fade_hwnd = NULL;
static HDC     g_fade_dc = NULL;
static HBITMAP g_fade_bmp = NULL;
static HBITMAP g_fade_old = NULL;

static LRESULT CALLBACK fade_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_fade_dc) {
            int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            BitBlt(dc, 0, 0, sw, sh, g_fade_dc, 0, 0, SRCCOPY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}

static void fade_set_alpha(BYTE a) {
    if (g_fade_hwnd)
        SetLayeredWindowAttributes(g_fade_hwnd, 0, a, LWA_ALPHA);
}

/* 把旧壁纸按当前样式渲染到内存 DC；失败返回 -1 */
static int render_old_wallpaper(HDC dc, int sw, int sh, GpImage *img,
                                int iw, int ih, int style, int tile) {
    RECT r = { 0, 0, sw, sh };
    FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    GpGraphics *g = NULL;
    if (GdipCreateFromHDC(dc, &g) != GDIP_OK || !g) return -1;

    GpStatus st = GDIP_OK;
    if (tile == 1) {
        for (int y = 0; y < sh && st == GDIP_OK; y += ih)
            for (int x = 0; x < sw && st == GDIP_OK; x += iw)
                st = GdipDrawImageRectRectI(g, img, x, y, iw, ih, 0, 0, iw, ih,
                                            GDIP_UNIT_PIXEL, NULL, NULL, NULL);
    } else {
        int dx = 0, dy = 0, dw = sw, dh = sh, sx = 0, sy = 0, sww = iw, shh = ih;
        if (style == 0) {                 /* 居中 */
            int w = iw < sw ? iw : sw;
            int h = ih < sh ? ih : sh;
            sx = (iw - w) / 2; sy = (ih - h) / 2; sww = w; shh = h;
            dx = (sw - w) / 2; dy = (sh - h) / 2; dw = w; dh = h;
        } else if (style == 2) {          /* 拉伸：全图 → 全屏（默认值已满足） */
        } else if (style == 6) {          /* 适应 */
            double s = ((double)sw / iw < (double)sh / ih) ? (double)sw / iw : (double)sh / ih;
            dw = (int)(iw * s); dh = (int)(ih * s);
            dx = (sw - dw) / 2; dy = (sh - dh) / 2;
        } else {                          /* 10 填充 / 22 跨屏：覆盖 */
            double s = ((double)sw / iw > (double)sh / ih) ? (double)sw / iw : (double)sh / ih;
            dw = (int)(iw * s); dh = (int)(ih * s);
            /* Windows 的“填充”垂直方向并非居中：顶部裁 1/3、底部裁 2/3（dy=(H-dh)/3），
               水平方向仍居中（dx=(W-dw)/2）。若垂直误用居中(/2)，覆盖层会与真实壁纸
               错位 (dh-sh)/6，交叉淡化时产生上下抽搐。 */
            dx = (sw - dw) / 2; dy = (sh - dh) / 3;
        }
        st = GdipDrawImageRectRectI(g, img, dx, dy, dw, dh, sx, sy, sww, shh,
                                    GDIP_UNIT_PIXEL, NULL, NULL, NULL);
    }
    GdipDeleteGraphics(g);
    return (st == GDIP_OK) ? 0 : -1;
}

/* 交叉淡化：旧壁纸渲染到覆盖层 → 应用新壁纸 → 覆盖层淡出露出新壁纸。
 * 覆盖层置于壁纸层之上、桌面图标与应用窗口之下（仅壁纸层受过渡影响）。
 * 任何一步失败均回退为直接应用，不影响壁纸切换功能。 */
static int wallpaper_crossfade(const wchar_t *old_path, const wchar_t *new_path) {
    if (!gdip_ensure()) return wallpaper_apply(new_path);

    GpBitmap *img = NULL;
    if (GdipCreateBitmapFromFile(old_path, &img) != GDIP_OK || !img)
        return wallpaper_apply(new_path);
    UINT iw = 0, ih = 0;
    GdipGetImageWidth((GpImage *)img, &iw);
    GdipGetImageHeight((GpImage *)img, &ih);
    if (iw == 0 || ih == 0) {
        GdipDisposeImage((GpImage *)img);
        return wallpaper_apply(new_path);
    }

    HWND workerw = find_workerw();
    if (!workerw) {
        GdipDisposeImage((GpImage *)img);
        return wallpaper_apply(new_path);
    }

    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (sw <= 0 || sh <= 0) {
        GdipDisposeImage((GpImage *)img);
        return wallpaper_apply(new_path);
    }

    int style, tile;
    read_wallpaper_style(&style, &tile);

    /* 注册覆盖层窗口类 */
    static int registered = 0;
    if (!registered) {
        WNDCLASSEXW wc;
        wc.cbSize        = sizeof(wc);
        wc.style         = 0;
        wc.lpfnWndProc   = fade_wndproc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = GetModuleHandleW(NULL);
        wc.hIcon         = NULL;
        wc.hCursor       = NULL;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = L"WallpaperManagerFade";
        wc.hIconSm       = NULL;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            GdipDisposeImage((GpImage *)img);
            return wallpaper_apply(new_path);
        }
        registered = 1;
    }

    /* 渲染旧壁纸到内存 DC */
    HDC screendc = GetDC(NULL);
    g_fade_dc = CreateCompatibleDC(screendc);
    g_fade_bmp = CreateCompatibleBitmap(screendc, sw, sh);
    ReleaseDC(NULL, screendc);
    if (!g_fade_dc || !g_fade_bmp) {
        if (g_fade_bmp) DeleteObject(g_fade_bmp);
        if (g_fade_dc) DeleteDC(g_fade_dc);
        g_fade_dc = NULL; g_fade_bmp = NULL; g_fade_old = NULL;
        GdipDisposeImage((GpImage *)img);
        return wallpaper_apply(new_path);
    }
    g_fade_old = (HBITMAP)SelectObject(g_fade_dc, g_fade_bmp);
    if (render_old_wallpaper(g_fade_dc, sw, sh, (GpImage *)img, (int)iw, (int)ih,
                             style, tile) != 0) {
        SelectObject(g_fade_dc, g_fade_old);
        DeleteObject(g_fade_bmp);
        DeleteDC(g_fade_dc);
        g_fade_dc = NULL; g_fade_bmp = NULL; g_fade_old = NULL;
        GdipDisposeImage((GpImage *)img);
        return wallpaper_apply(new_path);
    }

    /* 创建覆盖层窗口，插入到壁纸层与图标层之间 */
    g_fade_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"WallpaperManagerFade", L"", WS_POPUP,
        GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
        sw, sh, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (g_fade_hwnd) {
        /* 置于 WorkerW（图标宿主）之后：位于壁纸之上、图标与应用之下 */
        SetWindowPos(g_fade_hwnd, workerw, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetLayeredWindowAttributes(g_fade_hwnd, 0, 255, LWA_ALPHA);
        ShowWindow(g_fade_hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(g_fade_hwnd);
    }

    /* 应用新壁纸（被覆盖层遮挡，视觉无跳变） */
    int rc = wallpaper_apply(new_path);

    /* 淡出覆盖层，交叉淡化到新壁纸 */
    if (g_fade_hwnd) {
        for (int a = 255; a >= 0; a -= 12) { fade_set_alpha((BYTE)a); Sleep(12); }
        DestroyWindow(g_fade_hwnd);
        g_fade_hwnd = NULL;
    }

    /* 清理 */
    if (g_fade_old) SelectObject(g_fade_dc, g_fade_old);
    if (g_fade_bmp) DeleteObject(g_fade_bmp);
    if (g_fade_dc) DeleteDC(g_fade_dc);
    g_fade_dc = NULL; g_fade_bmp = NULL; g_fade_old = NULL;
    GdipDisposeImage((GpImage *)img);

    return rc;
}

/* 实际应用壁纸（注册表 + SystemParametersInfo） */
static int wallpaper_apply(const wchar_t *path) {
    int rc = 0;

    /* 桌面壁纸：HKCU\Control Panel\Desktop\Wallpaper */
    if (reg_set_sz(HKEY_CURRENT_USER, REG_DESKTOP, L"Wallpaper", path) != 0) {
        LOGE(L"Failed to write desktop wallpaper registry: %s", path);
        rc = -1;
    }

    /* 锁屏壁纸（HKLM，需管理员权限；非管理员跳过，避免失败写入与系统抖动） */
    if (is_admin()) {
        if (reg_set_sz(HKEY_LOCAL_MACHINE, REG_LOCKSCREEN, L"LogonImagePath", path) != 0)
            LOGW(L"Failed to write lock screen wallpaper (LogonImagePath)");
        if (reg_set_sz(HKEY_LOCAL_MACHINE, REG_LOCKSCREEN, L"LockScreenImagePath", path) != 0)
            LOGW(L"Failed to write lock screen wallpaper (LockScreenImagePath)");
    }

    /* 立即应用：直接调用 user32；去掉 SPIF_UPDATEINIFILE，避免每次切换写入 win.ini */
    BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path,
                                    SPIF_SENDWININICHANGE);
    if (!ok) {
        LOGE(L"Failed to apply wallpaper (SystemParametersInfo): %s", path);
        rc = -1;
    }

    return rc;
}

/* 设置壁纸（带交叉淡化过渡动画；动画失败时直接应用，不影响功能） */
int wallpaper_set(const wchar_t *path) {
    wchar_t *old = wallpaper_get_current();
    int do_anim = (old && *old && !wcs_ieq(old, path));
    int rc;
    if (do_anim)
        rc = wallpaper_crossfade(old, path);
    else
        rc = wallpaper_apply(path);
    heap_free(old);
    return rc;
}

wchar_t *wallpaper_get_current(void) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_DESKTOP, 0, KEY_QUERY_VALUE, &key)
        != ERROR_SUCCESS)
        return NULL;

    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, L"Wallpaper", NULL, &type, NULL, &size) != ERROR_SUCCESS
        || type != REG_SZ || size < 2) {
        RegCloseKey(key);
        return NULL;
    }

    wchar_t *buf = xmalloc(size);
    LONG rc = RegQueryValueExW(key, L"Wallpaper", NULL, &type, (BYTE *)buf, &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) {
        heap_free(buf);
        return NULL;
    }
    return buf;
}
