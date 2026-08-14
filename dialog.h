/* dialog.h — 现代对话框模块
 * 文件/目录选择：IFileOpenDialog（Vista+ 现代样式）
 * 错误/信息提示：TaskDialogIndirect（失败回退 MessageBox）
 * 数字输入：自绘模态对话框（编辑框 ES_NUMBER + 系统字体）
 */
#pragma once
#include "common.h"

wchar_t *dialog_pick_file(HWND owner);   /* 选择文件，返回新分配路径或 NULL */
wchar_t *dialog_pick_folder(HWND owner); /* 选择目录，返回新分配路径或 NULL */
void  dialog_error(HWND owner, const wchar_t *title, const wchar_t *text);
void  dialog_info(HWND owner, const wchar_t *title, const wchar_t *text);
int   dialog_input_number(HWND owner, const wchar_t *title, const wchar_t *prompt,
                          int min, int max, int def, int *out);
