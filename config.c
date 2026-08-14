/* config.c — 配置模块实现（方案 A） */
#include "config.h"

#define CFG_FILENAME L"WallpaperManager.cfg"

wchar_t *config_get_path(void) {
    wchar_t *dir = path_get_exe_dir();
    wchar_t *p = path_join(dir, CFG_FILENAME);
    heap_free(dir);
    return p;
}

void config_init(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->mode = wcs_dup(CFG_MODE_SINGLE);
    cfg->toggle_interval = 10;
    cfg->toggle_mode = wcs_dup(L"N+");
    cfg->current_id = 1;
    cfg->file_index = 0;
    cfg->folder_index = 0;
    cfg->current_file = wcs_dup(L"");
    cfg->current_folder = wcs_dup(L"");
    cfg->root_path = wcs_dup(L"");
    cfg->folders = NULL;
    cfg->folder_count = 0;
    cfg->hidden = NULL;
    cfg->hidden_count = 0;
}

void config_free(AppConfig *cfg) {
    if (!cfg) return;
    heap_free(cfg->mode);
    heap_free(cfg->toggle_mode);
    heap_free(cfg->current_file);
    heap_free(cfg->current_folder);
    heap_free(cfg->root_path);
    for (int i = 0; i < cfg->folder_count; i++) heap_free(cfg->folders[i]);
    heap_free(cfg->folders);
    for (int i = 0; i < cfg->hidden_count; i++) heap_free(cfg->hidden[i]);
    heap_free(cfg->hidden);
    memset(cfg, 0, sizeof(*cfg));
}

void config_clear_folders(AppConfig *cfg) {
    for (int i = 0; i < cfg->folder_count; i++) heap_free(cfg->folders[i]);
    heap_free(cfg->folders);
    cfg->folders = NULL;
    cfg->folder_count = 0;
}

void config_add_folder(AppConfig *cfg, const wchar_t *path) {
    cfg->folders = xrealloc(cfg->folders, (cfg->folder_count + 1) * sizeof(wchar_t *));
    cfg->folders[cfg->folder_count++] = wcs_dup(path);
}

int config_is_hidden(const AppConfig *cfg, const wchar_t *filename) {
    for (int i = 0; i < cfg->hidden_count; i++)
        if (wcs_ieq(cfg->hidden[i], filename)) return 1;
    return 0;
}

void config_add_hidden(AppConfig *cfg, const wchar_t *filename) {
    if (config_is_hidden(cfg, filename)) return;
    cfg->hidden = xrealloc(cfg->hidden, (cfg->hidden_count + 1) * sizeof(wchar_t *));
    cfg->hidden[cfg->hidden_count++] = wcs_dup(filename);
}

/* ---- 简易 UTF-8 行修剪 ---- */
static char *trim_ascii(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
    *end = '\0';
    return s;
}

/* 去掉可能包裹的值引号 */
static char *strip_quotes(char *s) {
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        s[n - 1] = '\0';
        return s + 1;
    }
    return s;
}

int config_load(AppConfig *cfg) {
    wchar_t *path = config_get_path();
    size_t len = 0;
    char *data = read_text_file_utf8(path, &len);
    heap_free(path);
    if (!data) return -1; /* 文件不存在，使用默认值 */

    config_free(cfg);
    config_init(cfg);

    int section = 0; /* 0 无 / 1 state / 2 folders / 3 hidden */
    char *p = data;
    while (*p) {
        char *line = p;
        char *nl = strchr(p, '\n');
        if (nl) { *nl = '\0'; p = nl + 1; }
        else { p += strlen(p); }

        line = trim_ascii(line);
        if (*line == '\0' || *line == '#' || *line == ';') continue;

        if (line[0] == '[') {
            if (strcmp(line, "[State]") == 0) section = 1;
            else if (strcmp(line, "[Folders]") == 0) section = 2;
            else if (strcmp(line, "[Hidden]") == 0) section = 3;
            else section = 0;
            continue;
        }

        if (section == 2) {
            wchar_t *w = utf8_to_wide(line);
            config_add_folder(cfg, w);
            heap_free(w);
        } else if (section == 3) {
            wchar_t *w = utf8_to_wide(line);
            config_add_hidden(cfg, w);
            heap_free(w);
        } else if (section == 1) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = trim_ascii(line);
            char *val = strip_quotes(trim_ascii(eq + 1));
            wchar_t *wval = utf8_to_wide(val);

            if (strcmp(key, "Current_Mode") == 0) { heap_free(cfg->mode); cfg->mode = wval; }
            else if (strcmp(key, "Toggle_Time") == 0) { cfg->toggle_interval = atoi(val); heap_free(wval); }
            else if (strcmp(key, "Toggle_Mode") == 0) { heap_free(cfg->toggle_mode); cfg->toggle_mode = wval; }
            else if (strcmp(key, "Current_ID") == 0) { cfg->current_id = atoi(val); heap_free(wval); }
            else if (strcmp(key, "File_Index") == 0) { cfg->file_index = atoi(val); heap_free(wval); }
            else if (strcmp(key, "Folder_Index") == 0) { cfg->folder_index = atoi(val); heap_free(wval); }
            else if (strcmp(key, "Current_File") == 0) { heap_free(cfg->current_file); cfg->current_file = wval; }
            else if (strcmp(key, "Current_Folder") == 0) { heap_free(cfg->current_folder); cfg->current_folder = wval; }
            else if (strcmp(key, "Root_Path") == 0) { heap_free(cfg->root_path); cfg->root_path = wval; }
            else heap_free(wval);
        }
    }

    /* 容错：手动编辑配置导致的越界值 */
    if (cfg->toggle_interval < 1) cfg->toggle_interval = 1;
    if (cfg->toggle_interval > 999) cfg->toggle_interval = 999;

    heap_free(data);
    return 0;
}

int config_save(const AppConfig *cfg) {
    WStr ws;
    wstr_init(&ws);
    wstr_appendf(&ws, L"[State]\r\n");
    wstr_appendf(&ws, L"Current_ID = %d\r\n", cfg->current_id);
    wstr_appendf(&ws, L"Current_Mode = %s\r\n", cfg->mode);
    wstr_appendf(&ws, L"File_Index = %d\r\n", cfg->file_index);
    wstr_appendf(&ws, L"Folder_Index = %d\r\n", cfg->folder_index);
    wstr_appendf(&ws, L"Toggle_Time = %d\r\n", cfg->toggle_interval);
    wstr_appendf(&ws, L"Toggle_Mode = %s\r\n", cfg->toggle_mode);
    wstr_appendf(&ws, L"Current_File = %s\r\n", cfg->current_file);
    wstr_appendf(&ws, L"Current_Folder = %s\r\n", cfg->current_folder);
    wstr_appendf(&ws, L"Root_Path = %s\r\n", cfg->root_path);

    /* 仅目录模式（单/多）持久化目录列表 */
    if (wcs_ieq(cfg->mode, CFG_MODE_SINGLE) || wcs_ieq(cfg->mode, CFG_MODE_MORE)) {
        wstr_appendf(&ws, L"\r\n[Folders]\r\n");
        for (int i = 0; i < cfg->folder_count; i++)
            wstr_appendf(&ws, L"%s\r\n", cfg->folders[i]);
    }

    /* 仅当存在被隐藏的壁纸时写入 */
    if (cfg->hidden_count > 0) {
        wstr_appendf(&ws, L"\r\n[Hidden]\r\n");
        for (int i = 0; i < cfg->hidden_count; i++)
            wstr_appendf(&ws, L"%s\r\n", cfg->hidden[i]);
    }

    wchar_t *content = wstr_detach(&ws);
    char *u8 = wide_to_utf8(content);
    heap_free(content);

    wchar_t *path = config_get_path();
    int rc = u8 ? write_text_file_utf8(path, u8, strlen(u8)) : -1;
    heap_free(u8);
    heap_free(path);
    return rc;
}
