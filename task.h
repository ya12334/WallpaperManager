/* task.h — 定时任务模块（Task Scheduler 2.0 COM 接口）
 *
 * 使用 ITaskService 直接创建/管理计划任务，不依赖 schtasks.exe，也不产生 XML 临时文件。
 * 任务名固定为 "WallpaperManager"，动作固定为执行本程序 "Toggle_File Next"，
 * 每 interval 分钟重复一次，实现自动轮播。
 */
#pragma once
#include "common.h"

/* 创建/更新定时任务：每 interval_minutes 分钟执行一次切换。0 成功，-1 失败。 */
int task_create(int interval_minutes);

/* 删除定时任务。0 成功（含本就不存在），-1 失败。 */
int task_delete(void);

/* 任务是否存在：1 存在；0 不存在；-1 查询失败。 */
int task_exists(void);

/* 任务是否已启用：1 已启用；0 禁用或不存在；-1 查询失败。 */
int task_is_enabled(void);

/* 启用(1)/禁用(0)任务；若任务不存在且要求启用，则先按默认 10 分钟创建。0 成功，-1 失败。 */
int task_set_enabled(int enabled);
