/* log.c — 日志模块实现 */
#include "log.h"
#include <stdarg.h>

static HANDLE g_log = INVALID_HANDLE_VALUE;

static const wchar_t *level_name(LogLevel lvl) {
    switch (lvl) {
        case LOG_DEBUG: return L"DEBUG";
        case LOG_INFO:  return L"INFO";
        case LOG_WARN:  return L"WARN";
        case LOG_ERROR: return L"ERROR";
    }
    return L"????";
}

void log_init(void) {
    wchar_t *dir = path_get_exe_dir();
    wchar_t *p = path_join(dir, L"WallpaperManager.log");
    g_log = CreateFileW(p, FILE_APPEND_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    heap_free(p);
    heap_free(dir);
}

void log_close(void) {
    if (g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }
}

void log_write(LogLevel lvl, const wchar_t *fmt, ...) {
    if (g_log == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t body[1600];
    va_list ap;
    va_start(ap, fmt);
    StringCchVPrintfW(body, ARRAYSIZE(body), fmt, ap);
    va_end(ap);

    wchar_t line[2048];
    StringCchPrintfW(line, ARRAYSIZE(line),
        L"[%04u-%02u-%02u %02u:%02u:%02u] [%s] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        level_name(lvl), body);

    char *u8 = wide_to_utf8(line);
    if (u8) {
        DWORD n = (DWORD)strlen(u8);
        DWORD written = 0;
        SetFilePointer(g_log, 0, NULL, FILE_END);
        WriteFile(g_log, u8, n, &written, NULL);
        heap_free(u8);
    }
}
