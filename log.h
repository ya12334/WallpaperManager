/* log.h — 日志模块（完整记录模式）
 * 所有操作写入 WallpaperManager.log（UTF-8），带时间戳与级别。
 */
#pragma once
#include "common.h"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void log_init(void);
void log_close(void);
void log_write(LogLevel lvl, const wchar_t *fmt, ...);

#define LOGD(fmt, ...) log_write(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) log_write(LOG_INFO,  fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_write(LOG_WARN,  fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) log_write(LOG_ERROR, fmt, ##__VA_ARGS__)
