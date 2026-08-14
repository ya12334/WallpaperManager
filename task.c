/* task.c — 定时任务模块实现（Task Scheduler 2.0 COM 接口）
 *
 * 全程 C 接口（lpVtbl），BSTR 用 SysAllocString 构造、VARIANT 用 VariantInit 初始化，
 * 均为 oleaut32 提供的标准 COM 原语，无任何外部进程或临时文件。
 */
#include "task.h"
#include "log.h"

#include <taskschd.h>
#include <oleauto.h>

#define TASK_NAME_W L"WallpaperManager"

/* ---------- 连接本地任务计划程序服务 ---------- */
static ITaskService *connect_service(void) {
    ITaskService *svc = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_ITaskService, (void **)&svc);
    if (FAILED(hr) || !svc) {
        LOGE(L"Failed to create Task Scheduler instance (0x%08X)", (unsigned)hr);
        return NULL;
    }

    VARIANT server, user, domain, password;
    VariantInit(&server); VariantInit(&user);
    VariantInit(&domain); VariantInit(&password);
    hr = svc->lpVtbl->Connect(svc, server, user, domain, password);
    if (FAILED(hr)) {
        LOGE(L"Failed to connect to Task Scheduler (0x%08X)", (unsigned)hr);
        svc->lpVtbl->Release(svc);
        return NULL;
    }
    return svc;
}

/* ---------- 获取根目录文件夹 ---------- */
static ITaskFolder *get_root_folder(ITaskService *svc) {
    ITaskFolder *root = NULL;
    BSTR path = SysAllocString(L"\\");
    HRESULT hr = svc->lpVtbl->GetFolder(svc, path, &root);
    SysFreeString(path);
    if (FAILED(hr)) return NULL;
    return root;
}

/* ---------- 查询任务对象 ---------- */
static IRegisteredTask *get_task(ITaskFolder *root) {
    IRegisteredTask *task = NULL;
    BSTR name = SysAllocString(TASK_NAME_W);
    HRESULT hr = root->lpVtbl->GetTask(root, name, &task);
    SysFreeString(name);
    if (FAILED(hr) || !task) return NULL;
    return task;
}

/* ---------- 创建/更新定时任务 ---------- */
int task_create(int interval_minutes) {
    if (interval_minutes < 1) interval_minutes = 1;

    ITaskService *svc = connect_service();
    if (!svc) return -1;

    ITaskDefinition *def = NULL;
    HRESULT hr = svc->lpVtbl->NewTask(svc, 0, &def);
    if (FAILED(hr) || !def) {
        svc->lpVtbl->Release(svc);
        return -1;
    }

    /* 主体：交互令牌 + 最高可用权限（锁屏壁纸需管理员） */
    IPrincipal *principal = NULL;
    if (SUCCEEDED(def->lpVtbl->get_Principal(def, &principal)) && principal) {
        principal->lpVtbl->put_LogonType(principal, TASK_LOGON_INTERACTIVE_TOKEN);
        principal->lpVtbl->put_RunLevel(principal, TASK_RUNLEVEL_HIGHEST);
        principal->lpVtbl->Release(principal);
    }

    /* 触发器：时间触发器 + 每 interval 分钟重复一次 */
    ITriggerCollection *triggers = NULL;
    if (SUCCEEDED(def->lpVtbl->get_Triggers(def, &triggers)) && triggers) {
        ITrigger *trigger = NULL;
        if (SUCCEEDED(triggers->lpVtbl->Create(triggers, TASK_TRIGGER_TIME, &trigger))
            && trigger) {
            IRepetitionPattern *rep = NULL;
            if (SUCCEEDED(trigger->lpVtbl->get_Repetition(trigger, &rep)) && rep) {
                wchar_t iso[32];
                StringCchPrintfW(iso, ARRAYSIZE(iso), L"PT%dM", interval_minutes);
                BSTR b = SysAllocString(iso);
                rep->lpVtbl->put_Interval(rep, b);
                SysFreeString(b);
                rep->lpVtbl->put_StopAtDurationEnd(rep, VARIANT_FALSE);
                rep->lpVtbl->Release(rep);
            }

            /* 起始时间：当前本地时间 */
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t sb[32];
            StringCchPrintfW(sb, ARRAYSIZE(sb), L"%04u-%02u-%02uT%02u:%02u:%02u",
                             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            BSTR b2 = SysAllocString(sb);
            trigger->lpVtbl->put_StartBoundary(trigger, b2);
            SysFreeString(b2);
            trigger->lpVtbl->put_Enabled(trigger, VARIANT_TRUE);
            trigger->lpVtbl->Release(trigger);
        }
        triggers->lpVtbl->Release(triggers);
    }

    /* 动作：执行本程序 "Toggle_File Next" */
    IActionCollection *actions = NULL;
    if (SUCCEEDED(def->lpVtbl->get_Actions(def, &actions)) && actions) {
        IAction *action = NULL;
        if (SUCCEEDED(actions->lpVtbl->Create(actions, TASK_ACTION_EXEC, &action))
            && action) {
            IExecAction *exec = NULL;
            if (SUCCEEDED(action->lpVtbl->QueryInterface(action, &IID_IExecAction,
                                                        (void **)&exec)) && exec) {
                wchar_t *exe = path_get_exe_path();
                BSTR bpath = SysAllocString(exe);
                BSTR barg = SysAllocString(L"Toggle_File Next");
                exec->lpVtbl->put_Path(exec, bpath);
                exec->lpVtbl->put_Arguments(exec, barg);
                SysFreeString(bpath);
                SysFreeString(barg);
                heap_free(exe);
                exec->lpVtbl->Release(exec);
            }
            action->lpVtbl->Release(action);
        }
        actions->lpVtbl->Release(actions);
    }

    /* 运行设置 */
    ITaskSettings *settings = NULL;
    if (SUCCEEDED(def->lpVtbl->get_Settings(def, &settings)) && settings) {
        settings->lpVtbl->put_MultipleInstances(settings, TASK_INSTANCES_IGNORE_NEW);
        settings->lpVtbl->put_DisallowStartIfOnBatteries(settings, VARIANT_FALSE);
        settings->lpVtbl->put_StopIfGoingOnBatteries(settings, VARIANT_FALSE);
        settings->lpVtbl->put_AllowHardTerminate(settings, VARIANT_FALSE);
        settings->lpVtbl->put_StartWhenAvailable(settings, VARIANT_TRUE);
        settings->lpVtbl->put_Enabled(settings, VARIANT_TRUE);
        settings->lpVtbl->Release(settings);
    }

    /* 注册（覆盖式更新） */
    ITaskFolder *root = get_root_folder(svc);
    if (root) {
        VARIANT user, password, sddl;
        VariantInit(&user); VariantInit(&password); VariantInit(&sddl);
        IRegisteredTask *reg = NULL;
        hr = root->lpVtbl->RegisterTaskDefinition(
            root, TASK_NAME_W, def, TASK_CREATE_OR_UPDATE,
            user, password, TASK_LOGON_INTERACTIVE_TOKEN, sddl, &reg);
        if (reg) reg->lpVtbl->Release(reg);
        root->lpVtbl->Release(root);
    } else {
        hr = E_FAIL;
    }

    def->lpVtbl->Release(def);
    svc->lpVtbl->Release(svc);

    if (FAILED(hr)) {
        LOGE(L"Failed to create/update scheduled task (0x%08X)", (unsigned)hr);
        return -1;
    }
    LOGI(L"Scheduled task created/updated: switch every %d minutes", interval_minutes);
    return 0;
}

/* ---------- 删除定时任务 ---------- */
int task_delete(void) {
    ITaskService *svc = connect_service();
    if (!svc) return -1;
    ITaskFolder *root = get_root_folder(svc);
    if (!root) { svc->lpVtbl->Release(svc); return -1; }

    BSTR name = SysAllocString(TASK_NAME_W);
    HRESULT hr = root->lpVtbl->DeleteTask(root, name, 0);
    SysFreeString(name);
    root->lpVtbl->Release(root);
    svc->lpVtbl->Release(svc);

    if (FAILED(hr) && hr != (HRESULT)0x80070002L) { /* HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) */
        LOGE(L"Failed to delete scheduled task (0x%08X)", (unsigned)hr);
        return -1;
    }
    LOGI(L"Scheduled task deleted");
    return 0;
}

/* ---------- 任务是否存在 ---------- */
int task_exists(void) {
    ITaskService *svc = connect_service();
    if (!svc) return -1;
    ITaskFolder *root = get_root_folder(svc);
    if (!root) { svc->lpVtbl->Release(svc); return -1; }

    IRegisteredTask *task = get_task(root);
    if (task) task->lpVtbl->Release(task);
    root->lpVtbl->Release(root);
    svc->lpVtbl->Release(svc);
    return task ? 1 : 0;
}

/* ---------- 任务是否启用 ---------- */
int task_is_enabled(void) {
    ITaskService *svc = connect_service();
    if (!svc) return -1;
    ITaskFolder *root = get_root_folder(svc);
    if (!root) { svc->lpVtbl->Release(svc); return -1; }

    IRegisteredTask *task = get_task(root);
    int result = 0;
    if (task) {
        VARIANT_BOOL enabled = VARIANT_FALSE;
        task->lpVtbl->get_Enabled(task, &enabled);
        result = (enabled == VARIANT_TRUE) ? 1 : 0;
        task->lpVtbl->Release(task);
    }
    root->lpVtbl->Release(root);
    svc->lpVtbl->Release(svc);
    return result;
}

/* ---------- 启用/禁用任务 ---------- */
int task_set_enabled(int enabled) {
    ITaskService *svc = connect_service();
    if (!svc) return -1;
    ITaskFolder *root = get_root_folder(svc);
    if (!root) { svc->lpVtbl->Release(svc); return -1; }

    IRegisteredTask *task = get_task(root);
    int rc = 0;
    if (!task) {
        /* 任务不存在：要求启用时先创建 */
        svc->lpVtbl->Release(svc);
        root->lpVtbl->Release(root);
        if (enabled) return task_create(10);
        return 0; /* 禁用不存在的任务视为成功 */
    }

    HRESULT hr = task->lpVtbl->put_Enabled(task, enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(hr)) {
        LOGE(L"Failed to enable/disable scheduled task (0x%08X)", (unsigned)hr);
        rc = -1;
    }
    task->lpVtbl->Release(task);
    root->lpVtbl->Release(root);
    svc->lpVtbl->Release(svc);
    return rc;
}
