/* dialog.c — 现代对话框实现 */
#include "dialog.h"

/* ---------- 文件/目录选择（IFileOpenDialog） ---------- */
static wchar_t *pick_path(int folders) {
    IFileOpenDialog *dlg = NULL;
    wchar_t *result = NULL;

    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IFileOpenDialog, (void **)&dlg);
    if (FAILED(hr) || !dlg) return NULL;

    FILEOPENDIALOGOPTIONS opts = 0;
    dlg->lpVtbl->GetOptions(dlg, &opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST;
    if (folders) opts |= FOS_PICKFOLDERS;
    else         opts |= FOS_FILEMUSTEXIST;
    dlg->lpVtbl->SetOptions(dlg, opts);
    dlg->lpVtbl->SetTitle(dlg, folders ? L"选择目录" : L"选择壁纸图片");

    hr = dlg->lpVtbl->Show(dlg, NULL);
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        if (SUCCEEDED(dlg->lpVtbl->GetResult(dlg, &item))) {
            PWSTR p = NULL;
            if (SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &p)) && p) {
                result = wcs_dup(p);
                CoTaskMemFree(p);
            }
            item->lpVtbl->Release(item);
        }
    }
    dlg->lpVtbl->Release(dlg);
    return result;
}

wchar_t *dialog_pick_file(HWND owner) { (void)owner; return pick_path(0); }
wchar_t *dialog_pick_folder(HWND owner) { (void)owner; return pick_path(1); }

/* ---------- 错误/信息提示（TaskDialogIndirect） ---------- */
static void task_dialog(HWND owner, const wchar_t *title, const wchar_t *text, PCWSTR icon) {
    TASKDIALOGCONFIG tdc;
    memset(&tdc, 0, sizeof(tdc));
    tdc.cbSize = sizeof(tdc);
    tdc.hwndParent = owner;
    tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = L"WallpaperManager";
    tdc.pszMainIcon = icon;
    tdc.pszMainInstruction = title;
    tdc.pszContent = text;

    TASKDIALOG_BUTTON btn;
    btn.nButtonID = IDOK;
    btn.pszButtonText = L"确定";
    tdc.pButtons = &btn;
    tdc.cButtons = 1;
    tdc.nDefaultButton = IDOK;

    int n = 0;
    if (FAILED(TaskDialogIndirect(&tdc, &n, NULL, NULL))) {
        UINT u = (icon == TD_ERROR_ICON) ? MB_ICONERROR
               : (icon == TD_WARNING_ICON) ? MB_ICONWARNING : MB_ICONINFORMATION;
        MessageBoxW(owner, text, title, MB_OK | u);
    }
}

void dialog_error(HWND owner, const wchar_t *title, const wchar_t *text) {
    task_dialog(owner, title, text, TD_ERROR_ICON);
}
void dialog_info(HWND owner, const wchar_t *title, const wchar_t *text) {
    task_dialog(owner, title, text, TD_INFORMATION_ICON);
}

/* ---------- 数字输入（自绘模态对话框） ---------- */
#define INPUT_CLASS L"WallpaperManagerInputDlg"

typedef struct {
    wchar_t prompt[512];
    int min, max, def;
    HWND edit;
    int value;
    BOOL ok;
} InputState;

static HFONT message_font(void) {
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    return CreateFontIndirectW(&ncm.lfMessageFont);
}

static LRESULT CALLBACK input_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    InputState *st = (InputState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
            st = (InputState *)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

            HINSTANCE hi = GetModuleHandleW(NULL);
            HFONT font = message_font();

            HWND prompt = CreateWindowExW(0, L"STATIC", st->prompt,
                WS_CHILD | WS_VISIBLE, 24, 24, 432, 44, hwnd, NULL, hi, NULL);
            SendMessageW(prompt, WM_SETFONT, (WPARAM)font, TRUE);

            st->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                24, 84, 432, 32, hwnd, NULL, hi, NULL);
            SendMessageW(st->edit, WM_SETFONT, (WPARAM)font, TRUE);
            wchar_t defbuf[32];
            StringCchPrintfW(defbuf, ARRAYSIZE(defbuf), L"%d", st->def);
            SetWindowTextW(st->edit, defbuf);
            SendMessageW(st->edit, EM_SETSEL, 0, -1);

            HWND okb = CreateWindowExW(0, L"BUTTON", L"确定",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                150, 140, 100, 32, hwnd, (HMENU)IDOK, hi, NULL);
            HWND cancelb = CreateWindowExW(0, L"BUTTON", L"取消",
                WS_CHILD | WS_VISIBLE, 270, 140, 100, 32, hwnd, (HMENU)IDCANCEL, hi, NULL);
            SendMessageW(okb, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(cancelb, WM_SETFONT, (WPARAM)font, TRUE);

            SetFocus(st->edit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                wchar_t buf[32];
                GetWindowTextW(st->edit, buf, ARRAYSIZE(buf));
                int v = 0;
                if (wcs_to_int(buf, &v) && v >= st->min && v <= st->max) {
                    st->value = v;
                    st->ok = TRUE;
                    DestroyWindow(hwnd);
                    PostQuitMessage(0);
                } else {
                    dialog_error(hwnd, L"输入无效",
                        L"请输入有效范围内的整数。");
                }
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                st->ok = FALSE;
                DestroyWindow(hwnd);
                PostQuitMessage(0);
                return 0;
            }
            break;
        case WM_CLOSE:
            st->ok = FALSE;
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int dialog_input_number(HWND owner, const wchar_t *title, const wchar_t *prompt,
                        int min, int max, int def, int *out) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = input_wndproc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = INPUT_CLASS;
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    InputState st;
    memset(&st, 0, sizeof(st));
    StringCchCopyW(st.prompt, ARRAYSIZE(st.prompt), prompt);
    st.min = min;
    st.max = max;
    st.def = def;
    st.value = def;
    st.ok = FALSE;

    int w = 480, h = 220;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, INPUT_CLASS, title,
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE, sx, sy, w, h,
        owner, NULL, GetModuleHandleW(NULL), &st);
    if (!hwnd) return 0;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (IsDialogMessageW(hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (out) *out = st.value;
    return st.ok;
}
