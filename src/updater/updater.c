/*
 * PhotoCraft Classic - PCUpdate.exe
 *
 *   PCUpdate.exe [--check] [--pid N]   interactive check (Help > Check for Updates)
 *   PCUpdate.exe --auto [--pid N]      silent startup check, at most once a day,
 *                                      UI only if an update is available
 *   PCUpdate.exe --from-file [manifest [msi]]
 *                                      install an update downloaded elsewhere
 *                                      (verified exactly like an online one)
 *   PCUpdate.exe --apply <msi> <pid> [<sha256> [<appdir>]]
 *                                      internal: runs from a temporary copy,
 *                                      closes the app, runs Windows Installer,
 *                                      restarts the app
 *
 * Design and threat model: docs/updater.md.
 */
#include <windows.h>
#include "pcrt.h"
#include "chrome.h"
#include "manifest.h"
#include "net.h"
#include "httpparse.h"
#include "pubkey.h"

#ifndef PC_VERSION
#define PC_VERSION "0.0.0"
#endif

#define APP_NAME        "PhotoCraft Classic"
#define UPD_TITLE       "PhotoCraft Classic Update"
#define REG_APP         "Software\\PhotoCraft Classic"
#define REG_UPD         "Software\\PhotoCraft Classic\\Update"
#define REG_IE          "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"
#define DEFAULT_URL     "https://github.com/knivesbatz/PhotoCraftr/releases/latest/download/update-manifest.txt"
#define APP_WNDCLASS    "PhotoCraftClassicFrame"
#define APP_CLOSE_MSG   "PhotoCraftClassic.UpdateClose"
#define APP_EXE         "PhotoCraftClassic.exe"
#define TEMP_SUBDIR     "PhotoCraftClassic-Update"
#define WND_CLASS       "PCUpdateWindow"
#define MUTEX_NAME      "PhotoCraftClassic.Updater"

#define WM_JOB_STATUS   (WM_APP + 1)
#define WM_JOB_DONE     (WM_APP + 2)

enum { ID_STATUS = 100, ID_DETAIL, ID_BAR, ID_NOTES, ID_STARTUP, ID_INSTALL, ID_LATER, ID_SKIP, ID_CANCEL_BTN };

/* ================================================================== */
/* small utilities                                                     */

static char g_tempdir[MAX_PATH];    /* %TEMP%\PhotoCraftClassic-Update\ */

static void path_join(char *out, size_t outsz, const char *dir, const char *name)
{
    size_t n;
    pc_strlcpy(out, dir, outsz);
    n = strlen(out);
    if (n && out[n - 1] != '\\') pc_strlcat(out, "\\", outsz);
    pc_strlcat(out, name, outsz);
}

static void path_dir(char *path)   /* strip the last component (keeps no trailing '\') */
{
    char *s = strrchr(path, '\\');
    if (s) *s = 0;
}

static int file_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != 0xFFFFFFFF && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void init_tempdir(void)
{
    char t[MAX_PATH];
    if (!GetTempPathA(sizeof t, t) || !t[0]) pc_strlcpy(t, "C:\\", sizeof t);
    path_join(g_tempdir, sizeof g_tempdir, t, TEMP_SUBDIR);
    CreateDirectoryA(g_tempdir, NULL);
    pc_strlcat(g_tempdir, "\\", sizeof g_tempdir);
}

static void logmsg(const char *fmt, ...)
{
    char line[600], path[MAX_PATH];
    SYSTEMTIME st;
    va_list ap;
    HANDLE f;
    DWORD w;
    int n;
    if (!g_tempdir[0]) return;
    GetLocalTime(&st);
    n = pc_snprintf(line, sizeof line, "%04u-%02u-%02u %02u:%02u:%02u [%lu] ", st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond, (unsigned long)GetCurrentProcessId());
    va_start(ap, fmt);
    pc_vsnprintf(line + n, sizeof line - (size_t)n - 2, fmt, ap);
    va_end(ap);
    pc_strlcat(line, "\r\n", sizeof line);
    path_join(path, sizeof path, g_tempdir, "update.log");
    f = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    if (GetFileSize(f, NULL) > 256 * 1024) { SetFilePointer(f, 0, NULL, FILE_BEGIN); SetEndOfFile(f); }
    SetFilePointer(f, 0, NULL, FILE_END);
    WriteFile(f, line, (DWORD)strlen(line), &w, NULL);
    CloseHandle(f);
}

static void fmt_size(char *out, size_t n, unsigned long bytes)
{
    if (bytes >= 1024UL * 1024UL) {
        unsigned long tenths = (bytes / 1024UL * 10UL) / 1024UL;
        pc_snprintf(out, n, "%lu.%lu MB", tenths / 10, tenths % 10);
    } else {
        pc_snprintf(out, n, "%lu KB", (bytes + 1023) / 1024);
    }
}

static unsigned long now_unix(void)
{
    SYSTEMTIME st;
    long d;
    GetSystemTime(&st);
    d = pc_days_from_civil(st.wYear, st.wMonth, st.wDay);
    if (d < 719528) return 0;
    return (unsigned long)(d - 719528) * 86400UL + st.wHour * 3600UL + st.wMinute * 60UL + st.wSecond;
}

/* ================================================================== */
/* registry                                                            */

static int reg_get_str(HKEY root, const char *sub, const char *name, char *out, DWORD outsz)
{
    HKEY k;
    DWORD type = 0, sz = outsz;
    LONG rc;
    if (!outsz) return 0;
    out[0] = 0;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    rc = RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz);
    RegCloseKey(k);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) { out[0] = 0; return 0; }
    out[sz < outsz ? sz : outsz - 1] = 0;
    return out[0] != 0;
}

static int reg_get_dword(HKEY root, const char *sub, const char *name, DWORD *out)
{
    HKEY k;
    DWORD type = 0, v = 0, sz = sizeof v;
    LONG rc;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    rc = RegQueryValueExA(k, name, NULL, &type, (BYTE *)&v, &sz);
    RegCloseKey(k);
    /* old Internet Explorer versions store ProxyEnable as 4-byte REG_BINARY */
    if (rc != ERROR_SUCCESS || sz != 4 || (type != REG_DWORD && type != REG_BINARY)) return 0;
    *out = v;
    return 1;
}

static int reg_get_bin(HKEY root, const char *sub, const char *name, void *out, DWORD want)
{
    HKEY k;
    DWORD type = 0, sz = want;
    LONG rc;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    rc = RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz);
    RegCloseKey(k);
    return rc == ERROR_SUCCESS && type == REG_BINARY && sz == want;
}

static int reg_set(HKEY root, const char *sub, const char *name, DWORD type, const void *data, DWORD sz)
{
    HKEY k;
    DWORD disp;
    LONG rc;
    if (RegCreateKeyExA(root, sub, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &k, &disp) != ERROR_SUCCESS) return 0;
    rc = RegSetValueExA(k, name, 0, type, (const BYTE *)data, sz);
    RegCloseKey(k);
    return rc == ERROR_SUCCESS;
}

static int reg_set_dword(const char *name, DWORD v) { return reg_set(HKEY_CURRENT_USER, REG_UPD, name, REG_DWORD, &v, sizeof v); }
static int reg_set_str(const char *name, const char *v) { return reg_set(HKEY_CURRENT_USER, REG_UPD, name, REG_SZ, v, (DWORD)strlen(v) + 1); }

/* ================================================================== */
/* configuration                                                       */

typedef struct {
    char manifest_url[PC_MANIFEST_URL_MAX];
    char channel[32];
    char installed[48];
    char skip_version[48];
    char install_dir[MAX_PATH];
    char app_dir[MAX_PATH];         /* directory of this PCUpdate.exe */
    DWORD last_seq, check_on_startup, last_check;
    int desktop_shortcut;           /* -1 unknown */
    pc_http_opts net;
    DWORD app_pid;
} config_t;

static config_t cfg;

static void load_proxy(pc_http_opts *o)
{
    char v[512];
    DWORD en = 0;
    o->proxy_host[0] = 0;
    /* Our own override: "direct" or a proxy "host:port". */
    if (reg_get_str(HKEY_CURRENT_USER, REG_UPD, "Proxy", v, sizeof v)) {
        if (pc_stricmp(v, "direct") && pc_proxy_parse(v, o->proxy_host, sizeof o->proxy_host, &o->proxy_port))
            o->proxy_host[0] = 0;
        logmsg("proxy (PhotoCraft setting): %s", o->proxy_host[0] ? o->proxy_host : "direct");
        return;
    }
    /* Internet Explorer's manual proxy (Control Panel > Internet). */
    if (reg_get_dword(HKEY_CURRENT_USER, REG_IE, "ProxyEnable", &en) && en &&
        reg_get_str(HKEY_CURRENT_USER, REG_IE, "ProxyServer", v, sizeof v)) {
        if (pc_proxy_parse(v, o->proxy_host, sizeof o->proxy_host, &o->proxy_port)) o->proxy_host[0] = 0;
        reg_get_str(HKEY_CURRENT_USER, REG_IE, "ProxyOverride", o->proxy_bypass, sizeof o->proxy_bypass);
    }
    if (o->proxy_host[0]) logmsg("proxy (Internet Explorer): %s:%u", o->proxy_host, (unsigned)o->proxy_port);
}

static void load_config(void)
{
    char v[64];
    DWORD d;
    memset(&cfg, 0, sizeof cfg);
    if (!reg_get_str(HKEY_CURRENT_USER, REG_UPD, "ManifestURL", cfg.manifest_url, sizeof cfg.manifest_url))
        pc_strlcpy(cfg.manifest_url, DEFAULT_URL, sizeof cfg.manifest_url);
    if (!reg_get_str(HKEY_CURRENT_USER, REG_UPD, "Channel", cfg.channel, sizeof cfg.channel))
        pc_strlcpy(cfg.channel, "stable", sizeof cfg.channel);
    reg_get_dword(HKEY_CURRENT_USER, REG_UPD, "LastSequence", &cfg.last_seq);
    cfg.check_on_startup = 1;
    reg_get_dword(HKEY_CURRENT_USER, REG_UPD, "CheckOnStartup", &cfg.check_on_startup);
    reg_get_dword(HKEY_CURRENT_USER, REG_UPD, "LastCheckTime", &cfg.last_check);
    reg_get_str(HKEY_CURRENT_USER, REG_UPD, "SkipVersion", cfg.skip_version, sizeof cfg.skip_version);

    /* Installed version: compiled-in, or the MSI-written value if higher. */
    pc_strlcpy(cfg.installed, PC_VERSION, sizeof cfg.installed);
    if (reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "Version", v, sizeof v) && pc_version_cmp_str(v, cfg.installed) > 0)
        pc_strlcpy(cfg.installed, v, sizeof cfg.installed);
    reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "InstallDir", cfg.install_dir, sizeof cfg.install_dir);
    cfg.desktop_shortcut = -1;
    if (reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "DesktopShortcut", v, sizeof v)) cfg.desktop_shortcut = v[0] == '1';
    else if (reg_get_dword(HKEY_LOCAL_MACHINE, REG_APP, "DesktopShortcut", &d)) cfg.desktop_shortcut = d != 0;

    GetModuleFileNameA(NULL, cfg.app_dir, sizeof cfg.app_dir);
    path_dir(cfg.app_dir);

    pc_http_opts_init(&cfg.net);
    load_proxy(&cfg.net);
}

/* PRNG seed carried between runs (helps on Windows 95, where the
 * per-run entropy sources are weak). */
static void seed_load(void)
{
    unsigned char s[32];
    if (reg_get_bin(HKEY_CURRENT_USER, REG_UPD, "RandomSeed", s, sizeof s)) pc_net_add_entropy(s, sizeof s);
    memset(s, 0, sizeof s);
}

static void seed_save(void)
{
    unsigned char s[32];
    pc_net_seed_out(s);
    reg_set(HKEY_CURRENT_USER, REG_UPD, "RandomSeed", REG_BINARY, s, sizeof s);
    memset(s, 0, sizeof s);
}

/* ================================================================== */
/* background jobs                                                     */

enum { JOB_MANIFEST = 1, JOB_MSI, JOB_HASHFILE };

typedef struct {
    int kind;
    char url[PC_MANIFEST_URL_MAX];
    char path[MAX_PATH];            /* MSI destination / file to hash */
    unsigned long expect_size;
    pc_http_opts opts;
    HWND notify;
    volatile long cancel;

    CRITICAL_SECTION cs;
    char status[300];
    volatile unsigned long done, total;
    int last_permille;

    pc_http_result res;
    pc_membuf buf;
    HANDLE file;
    br_sha256_context sha;
    unsigned char digest[32];
    unsigned long hashed;
    int ok;
} job_t;

static void job_status(void *ctx, const char *text)
{
    job_t *j = (job_t *)ctx;
    EnterCriticalSection(&j->cs);
    pc_strlcpy(j->status, text, sizeof j->status);
    LeaveCriticalSection(&j->cs);
    if (j->notify) PostMessageA(j->notify, WM_JOB_STATUS, 0, 0);
}

static void job_progress(void *ctx, unsigned long done, unsigned long total)
{
    job_t *j = (job_t *)ctx;
    int pm = total ? (int)((double)done * 1000.0 / (double)total) : 0;
    j->done = done;
    j->total = total;
    if (pm != j->last_permille || done == 0) {
        j->last_permille = pm;
        if (j->notify) PostMessageA(j->notify, WM_JOB_STATUS, 1, 0);
    }
}


/* One sink for all job kinds: the manifest goes to memory, the MSI to a
 * file; both are hashed as they arrive. */
static int job_sink(void *ctx, const void *data, size_t len)
{
    job_t *j = (job_t *)ctx;
    if (j->kind == JOB_MANIFEST) return pc_membuf_sink(&j->buf, data, len);
    {
        DWORD w = 0;
        if (!WriteFile(j->file, data, (DWORD)len, &w, NULL) || w != (DWORD)len) return -1;
        br_sha256_update(&j->sha, data, len);
        return 0;
    }
}

static void hash_file_job(job_t *j)
{
    static unsigned char chunk[32768];
    HANDLE f = CreateFileA(j->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got;
    if (f == INVALID_HANDLE_VALUE) {
        j->res.error = PC_NET_ERR_SINK;
        pc_snprintf(j->res.message, sizeof j->res.message, "Could not open %s.", j->path);
        return;
    }
    size = GetFileSize(f, NULL);
    job_status(j, "Verifying the update file...");
    br_sha256_init(&j->sha);
    j->hashed = 0;
    job_progress(j, 0, size);
    for (;;) {
        if (j->cancel) { j->res.error = PC_NET_ERR_CANCELLED; break; }
        if (!ReadFile(f, chunk, sizeof chunk, &got, NULL)) { j->res.error = PC_NET_ERR_SINK; pc_strlcpy(j->res.message, "Could not read the update file.", sizeof j->res.message); break; }
        if (!got) break;
        br_sha256_update(&j->sha, chunk, got);
        j->hashed += got;
        job_progress(j, j->hashed, size);
        if (j->hashed > PC_MANIFEST_MSI_MAX) break;
    }
    CloseHandle(f);
    br_sha256_out(&j->sha, j->digest);
    j->res.bytes = j->hashed;
}

static void job_run(job_t *j)
{
    pc_http_opts *o = &j->opts;
    o->status = job_status;
    o->progress = job_progress;
    o->sink = job_sink;
    o->ctx = j;
    o->cancel = &j->cancel;
    if (j->kind == JOB_HASHFILE) { hash_file_job(j); return; }
    if (j->kind == JOB_MANIFEST) {
        o->max_bytes = PC_MANIFEST_MAX_BYTES;
        o->expect_size = 0;
        j->buf.max = PC_MANIFEST_MAX_BYTES;
        logmsg("fetching manifest %s", j->url);
        pc_https_get(j->url, o, &j->res);
    } else {
        char part[MAX_PATH];
        pc_snprintf(part, sizeof part, "%s.part", j->path);
        o->max_bytes = j->expect_size;
        o->expect_size = j->expect_size;
        j->file = CreateFileA(part, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (j->file == INVALID_HANDLE_VALUE) {
            j->res.error = PC_NET_ERR_SINK;
            pc_snprintf(j->res.message, sizeof j->res.message, "Could not create %s.", part);
            return;
        }
        br_sha256_init(&j->sha);
        logmsg("downloading %s", j->url);
        pc_https_get(j->url, o, &j->res);
        CloseHandle(j->file);
        j->file = INVALID_HANDLE_VALUE;
        br_sha256_out(&j->sha, j->digest);
        if (j->res.error == PC_NET_OK) {
            DeleteFileA(j->path);
            if (!MoveFileA(part, j->path)) {
                j->res.error = PC_NET_ERR_SINK;
                pc_strlcpy(j->res.message, "Could not save the downloaded file.", sizeof j->res.message);
            }
        }
        if (j->res.error != PC_NET_OK) DeleteFileA(part);
    }
    logmsg("job %d finished: error=%d http=%d tls=%d bytes=%lu %s", j->kind, j->res.error, j->res.http_status,
           j->res.tls_error, j->res.bytes, j->res.message);
}

static DWORD WINAPI job_thread(LPVOID p)
{
    job_t *j = (job_t *)p;
    job_run(j);
    if (j->notify) PostMessageA(j->notify, WM_JOB_DONE, 0, 0);
    return 0;
}

static job_t *job_new(int kind)
{
    job_t *j = (job_t *)calloc(1, sizeof *j);
    if (!j) return NULL;
    j->kind = kind;
    j->opts = cfg.net;
    j->file = INVALID_HANDLE_VALUE;
    j->last_permille = -1;
    InitializeCriticalSection(&j->cs);
    return j;
}

static void job_free(job_t *j)
{
    if (!j) return;
    DeleteCriticalSection(&j->cs);
    pc_membuf_free(&j->buf);
    free(j);
}

/* ================================================================== */
/* windows                                                             */

typedef struct ui ui_t;
struct ui {
    HWND h;
    volatile int done;
    int result;
    int (*on_command)(ui_t *u, int id, int code);   /* returns 1 if handled */
    job_t *job;
    HWND status, detail, bar, cancel, startup;
};

static LRESULT CALLBACK ui_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    LRESULT lr;
    ui_t *u;
    if (pc_chrome_proc(h, m, wp, lp, &lr)) return lr;
    u = (ui_t *)pc_chrome_param(h);
    if (!u) return DefWindowProcA(h, m, wp, lp);
    switch (m) {
    case WM_COMMAND:
        if (u->on_command && u->on_command(u, LOWORD(wp), HIWORD(wp))) return 0;
        if (LOWORD(wp) == IDCANCEL) { u->result = IDCANCEL; u->done = 1; }
        return 0;
    case WM_CLOSE:
        if (u->on_command && u->on_command(u, IDCANCEL, 0)) return 0;
        u->result = IDCANCEL;
        u->done = 1;
        return 0;
    case WM_JOB_STATUS:
        if (u->job) {
            char s[300], d[96], a[32], b[32];
            EnterCriticalSection(&u->job->cs);
            pc_strlcpy(s, u->job->status, sizeof s);
            LeaveCriticalSection(&u->job->cs);
            if (u->status && !u->job->cancel) SetWindowTextA(u->status, s);
            if (u->bar && u->job->total) pc_progress_set(u->bar, (int)((double)u->job->done * 1000.0 / (double)u->job->total));
            if (u->detail && wp == 1) {
                fmt_size(a, sizeof a, u->job->done);
                fmt_size(b, sizeof b, u->job->total);
                if (u->job->total) pc_snprintf(d, sizeof d, "%s of %s", a, b);
                else pc_snprintf(d, sizeof d, "%s", a);
                SetWindowTextA(u->detail, d);
            }
        }
        return 0;
    case WM_JOB_DONE:
        u->result = IDOK;
        u->done = 1;
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

static int progress_command(ui_t *u, int id, int code)
{
    if (id == IDCANCEL || (id == ID_CANCEL_BTN && code == BN_CLICKED)) {
        if (u->job && !u->job->cancel) {
            u->job->cancel = 1;
            SetWindowTextA(u->status, "Cancelling...");
            EnableWindow(u->cancel, FALSE);
        }
        return 1;   /* wait for the worker to finish */
    }
    return 0;
}

/* Run a job on a worker thread behind a small progress window.  With
 * owner == NULL and silent != 0 no window is shown at all. */
static void run_job(job_t *j, HWND owner, const char *title, const char *heading, int with_bar, int silent)
{
    ui_t u;
    HANDLE th;
    DWORD tid;
    if (silent) { job_run(j); return; }
    memset(&u, 0, sizeof u);
    u.job = j;
    u.on_command = progress_command;
    u.h = pc_chrome_create(WND_CLASS, title, 340, with_bar ? 142 : 104, owner, &u);
    if (!u.h) { job_run(j); return; }
    pc_ctl_icon(u.h, (LPCSTR)IDI_INFORMATION, 12, 12);
    pc_ctl_label_bold(u.h, -1, heading, 56, 12, 272, 16);
    u.status = pc_ctl_label(u.h, ID_STATUS, "Starting...", 56, 32, 272, 30);
    if (with_bar) {
        u.bar = pc_ctl_progress(u.h, ID_BAR, 12, 66, 316, 18);
        u.detail = pc_ctl_label(u.h, ID_DETAIL, "", 12, 90, 200, 14);
    }
    u.cancel = pc_ctl_button(u.h, ID_CANCEL_BTN, "Cancel", 340 - 12 - 75, (with_bar ? 142 : 104) - 12 - 23, 75, 23, 1);
    j->notify = u.h;
    th = CreateThread(NULL, 0, job_thread, j, 0, &tid);
    if (!th) { j->notify = NULL; job_run(j); DestroyWindow(u.h); return; }
    pc_chrome_modal(u.h, owner, &u.done);
    WaitForSingleObject(th, INFINITE);   /* the thread posted WM_JOB_DONE */
    CloseHandle(th);
    j->notify = NULL;
    DestroyWindow(u.h);
    pc_chrome_pump();
}

/* ---------------- "update available" dialog ---------------- */

static int avail_command(ui_t *u, int id, int code)
{
    if (code != BN_CLICKED && code != 0) return 0;
    if (id == ID_INSTALL || id == ID_LATER || id == ID_SKIP) { u->result = id; u->done = 1; return 1; }
    if (id == IDCANCEL) { u->result = ID_LATER; u->done = 1; return 1; }
    if (id == IDOK) { u->result = ID_INSTALL; u->done = 1; return 1; }
    return 0;
}

/* Returns ID_INSTALL, ID_LATER or ID_SKIP. */
static int show_update_available(const pc_manifest *m, int from_file, int allow_skip)
{
    ui_t u;
    char line[200], size[32];
    const int cw = 420, ch = 320;
    int x;
    memset(&u, 0, sizeof u);
    u.on_command = avail_command;
    u.h = pc_chrome_create(WND_CLASS, UPD_TITLE, cw, ch, NULL, &u);
    if (!u.h) return ID_LATER;
    pc_ctl_icon(u.h, (LPCSTR)IDI_INFORMATION, 12, 12);
    pc_ctl_label_bold(u.h, -1, from_file ? "This update for PhotoCraft Classic is ready to install."
                                         : "A new version of PhotoCraft Classic is available.", 56, 12, cw - 68, 16);
    pc_snprintf(line, sizeof line, "Version %s (released %s). You have version %s.", m->version_str, m->released, cfg.installed);
    pc_ctl_label(u.h, -1, line, 56, 32, cw - 68, 14);
    fmt_size(size, sizeof size, m->msi_size);
    pc_snprintf(line, sizeof line, from_file ? "Update size: %s (verified)." : "Download size: %s.", size);
    pc_ctl_label(u.h, -1, line, 56, 48, cw - 68, 14);
    pc_ctl_label(u.h, -1, "What's new:", 12, 72, 200, 14);
    pc_ctl_edit(u.h, ID_NOTES, m->notes[0] ? m->notes : "(No release notes.)", 12, 88, cw - 24, 140,
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL);
    u.startup = pc_ctl_check(u.h, ID_STARTUP, "Check for updates when PhotoCraft Classic starts", 12, 236, cw - 24, 18,
                             cfg.check_on_startup != 0);
    pc_ctl_separator(u.h, 12, ch - 12 - 23 - 12, cw - 24);
    x = cw - 12;
    x -= 100; pc_ctl_button(u.h, ID_LATER, "Remind Me Later", x, ch - 12 - 23, 100, 23, 0);
    x -= 6 + 116; pc_ctl_button(u.h, ID_INSTALL, from_file ? "Install" : "Download && Install", x, ch - 12 - 23, 116, 23, 1);
    if (allow_skip) pc_ctl_button(u.h, ID_SKIP, "Skip This Version", 12, ch - 12 - 23, 110, 23, 0);
    MessageBeep(MB_ICONASTERISK);
    pc_chrome_modal(u.h, NULL, &u.done);
    {
        DWORD on = pc_ctl_get_check(u.startup) ? 1 : 0;
        if (on != cfg.check_on_startup) { cfg.check_on_startup = on; reg_set_dword("CheckOnStartup", on); }
    }
    DestroyWindow(u.h);
    pc_chrome_pump();
    return u.result ? u.result : ID_LATER;
}

/* ================================================================== */
/* verification + apply hand-off                                       */

static void remember_sequence(const pc_manifest *m)
{
    if (strcmp(m->channel, cfg.channel)) return;
    if (m->sequence > cfg.last_seq) {
        cfg.last_seq = m->sequence;
        reg_set_dword("LastSequence", m->sequence);
    }
}

static const char *policy_text(int p)
{
    switch (p) {
    case PC_POLICY_REPLAY:
        return "The update information is older than information this computer has already seen. "
               "It was rejected to protect against a downgrade attack.";
    case PC_POLICY_WRONG_CHANNEL: return "The update information is for a different release channel.";
    case PC_POLICY_BAD_INSTALLED: return "The installed version number could not be determined.";
    default: return "";
    }
}

/* Copy this executable to the temp folder and start it with --apply, so
 * Windows Installer can replace the installed PCUpdate.exe. */
static int launch_apply(const char *msi, const unsigned char sha[32])
{
    char self[MAX_PATH], copy[MAX_PATH], cmd[3 * MAX_PATH + 160], hex[65];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    GetModuleFileNameA(NULL, self, sizeof self);
    path_join(copy, sizeof copy, g_tempdir, "PCUpdate-apply.exe");
    if (pc_stricmp(self, copy)) {
        DeleteFileA(copy);
        if (!CopyFileA(self, copy, FALSE)) { logmsg("CopyFile to %s failed (%lu)", copy, GetLastError()); pc_strlcpy(copy, self, sizeof copy); }
    }
    pc_hex_encode(sha, 32, hex);
    pc_snprintf(cmd, sizeof cmd, "\"%s\" --apply \"%s\" %lu %s \"%s\"", copy, msi, (unsigned long)cfg.app_pid, hex, cfg.app_dir);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    logmsg("starting %s", cmd);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, g_tempdir, &si, &pi)) {
        logmsg("CreateProcess failed (%lu)", GetLastError());
        return -1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

static void msi_path_for(const pc_manifest *m, char *out, size_t n)
{
    char name[80];
    pc_snprintf(name, sizeof name, "PhotoCraftClassic-%s-Setup.msi", m->version_str);
    path_join(out, n, g_tempdir, name);
}

/* Download, verify and hand off.  Returns 0 if --apply was started. */
static int download_and_apply(const pc_manifest *m)
{
    job_t *j = job_new(JOB_MSI);
    char msg[700];
    int rc = -1;
    if (!j) return -1;
    pc_strlcpy(j->url, m->msi_url, sizeof j->url);
    msi_path_for(m, j->path, sizeof j->path);
    j->expect_size = m->msi_size;
    run_job(j, NULL, UPD_TITLE, "Downloading the update...", 1, 0);
    if (j->res.error == PC_NET_ERR_CANCELLED) { rc = 1; goto out; }
    if (j->res.error) {
        pc_snprintf(msg, sizeof msg, "The update could not be downloaded.\n\n%s", j->res.message);
        pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL);
        goto out;
    }
    if (j->res.bytes != m->msi_size || memcmp(j->digest, m->msi_sha256, 32)) {
        DeleteFileA(j->path);
        logmsg("downloaded file failed verification (size %lu, expected %lu)", j->res.bytes, m->msi_size);
        pc_msgbox(NULL, UPD_TITLE, "The downloaded file is damaged or has been tampered with, and was deleted.\n\n"
                  "Nothing was changed on your computer. Please try again later.", (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        goto out;
    }
    logmsg("download verified: %s", j->path);
    if (launch_apply(j->path, m->msi_sha256)) {
        pc_msgbox(NULL, UPD_TITLE, "The update installer could not be started.", (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        goto out;
    }
    rc = 0;
out:
    job_free(j);
    return rc;
}

/* ================================================================== */
/* --check / --auto                                                    */

static int mode_from_file(const char *manifest_arg, const char *msi_arg);

static int mode_check(int automatic, int force)
{
    job_t *j;
    pc_manifest *m = NULL;
    char err[200], msg[900];
    int rc = 0, pol, mf;

    if (automatic) {
        unsigned long now = now_unix();
        if (!cfg.check_on_startup) { logmsg("auto: disabled by CheckOnStartup=0"); return 0; }
        if (!force && cfg.last_check && now >= cfg.last_check && now - cfg.last_check < 86400UL) {
            logmsg("auto: last check %lu s ago, skipping", now - cfg.last_check);
            return 0;
        }
        reg_set_dword("LastCheckTime", now);
    }

    j = job_new(JOB_MANIFEST);
    m = (pc_manifest *)malloc(sizeof *m);
    if (!j || !m) { job_free(j); free(m); return 1; }
    pc_strlcpy(j->url, cfg.manifest_url, sizeof j->url);
    run_job(j, NULL, UPD_TITLE, "Checking for updates...", 0, automatic);

    if (j->res.error == PC_NET_ERR_CANCELLED) { rc = 1; goto out; }
    if (j->res.error) {
        if (automatic) { rc = 1; goto out; }
        pc_snprintf(msg, sizeof msg, "PhotoCraft Classic could not check for updates.\n\n%s\n\n"
                    "If this computer has no Internet access, you can download the update on another computer "
                    "and install it with \"Install from File\".", j->res.message);
        if (pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_EXCLAMATION, "OK", "Install from File...", NULL) == 2)
            rc = mode_from_file(NULL, NULL);
        else
            rc = 1;
        goto out;
    }
    mf = pc_manifest_load(j->buf.data ? j->buf.data : "", j->buf.len, pc_update_pubkeys, PC_UPDATE_NUM_PUBKEYS, m, err, sizeof err);
    if (mf) {
        logmsg("manifest rejected: %d %s", mf, err);
        if (!automatic) {
            pc_snprintf(msg, sizeof msg, "The update information could not be verified and was rejected.\n\n(%s)", err);
            pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        }
        rc = 1;
        goto out;
    }
    pol = pc_manifest_policy(m, cfg.installed, cfg.last_seq, cfg.channel);
    logmsg("manifest ok: version %s seq %lu (installed %s, last seq %lu) -> policy %d", m->version_str, m->sequence,
           cfg.installed, (unsigned long)cfg.last_seq, pol);
    if (pol != PC_POLICY_REPLAY) remember_sequence(m);
    if (pol == PC_POLICY_CURRENT) {
        if (!automatic) {
            pc_snprintf(msg, sizeof msg, "PhotoCraft Classic is up to date.\n\nInstalled version: %s\nLatest version: %s",
                        cfg.installed, m->version_str);
            pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        }
        goto out;
    }
    if (pol != PC_POLICY_UPDATE) {
        if (!automatic) pc_msgbox(NULL, UPD_TITLE, policy_text(pol), (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL);
        rc = 1;
        goto out;
    }
    if (automatic && cfg.skip_version[0] && !strcmp(cfg.skip_version, m->version_str)) {
        logmsg("auto: version %s skipped by user", m->version_str);
        goto out;
    }
    switch (show_update_available(m, 0, automatic)) {
    case ID_INSTALL:
        rc = download_and_apply(m);
        break;
    case ID_SKIP:
        reg_set_str("SkipVersion", m->version_str);
        break;
    default:
        break;
    }
out:
    job_free(j);
    free(m);
    return rc;
}

/* ================================================================== */
/* --from-file                                                         */

typedef BOOL (WINAPI *getopen_fn)(LPOPENFILENAMEA);

/* File picker: comdlg32's GetOpenFileNameA when available (loaded at run
 * time), otherwise a minimal "type the path" window. */
typedef struct { ui_t u; HWND edit; char *out; size_t outsz; } prompt_t;

static int prompt_command(ui_t *u, int id, int code)
{
    prompt_t *p = (prompt_t *)u;
    if (id == IDOK || (id == ID_INSTALL && code == BN_CLICKED)) {
        GetWindowTextA(p->edit, p->out, (int)p->outsz);
        u->result = IDOK; u->done = 1; return 1;
    }
    if (id == IDCANCEL || (id == ID_LATER && code == BN_CLICKED)) { u->result = IDCANCEL; u->done = 1; return 1; }
    return 0;
}

static int pick_file(const char *title, const char *filter, char *out, size_t outsz)
{
    HMODULE cd;
    UINT em = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    cd = LoadLibraryA("COMDLG32.DLL");
    SetErrorMode(em);
    if (cd) {
        FARPROC fp = GetProcAddress(cd, "GetOpenFileNameA");
        if (fp) {
            getopen_fn fn;
            OPENFILENAMEA ofn;
            int ok;
            memcpy(&fn, &fp, sizeof fp);
            memset(&ofn, 0, sizeof ofn);
            /* Windows 95/98/NT4 reject the larger Windows 2000 structure
             * (mingw's OPENFILENAMEA is 88 bytes even for WINVER 0x0400). */
#ifdef OPENFILENAME_SIZE_VERSION_400A
            ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400A;
#else
            ofn.lStructSize = 76;
#endif
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = out;
            ofn.nMaxFile = (DWORD)outsz;
            ofn.lpstrTitle = title;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            out[0] = 0;
            ok = fn(&ofn);
            FreeLibrary(cd);
            return ok && out[0];
        }
        FreeLibrary(cd);
    }
    {
        prompt_t p;
        memset(&p, 0, sizeof p);
        p.out = out;
        p.outsz = outsz;
        p.u.on_command = prompt_command;
        p.u.h = pc_chrome_create(WND_CLASS, title, 360, 110, NULL, &p.u);
        if (!p.u.h) return 0;
        pc_ctl_label(p.u.h, -1, "Type the full path of the file:", 12, 12, 336, 14);
        p.edit = pc_ctl_edit(p.u.h, ID_NOTES, "", 12, 30, 336, 22, ES_AUTOHSCROLL);
        pc_ctl_button(p.u.h, ID_INSTALL, "OK", 360 - 12 - 75 - 6 - 75, 110 - 12 - 23, 75, 23, 1);
        pc_ctl_button(p.u.h, ID_LATER, "Cancel", 360 - 12 - 75, 110 - 12 - 23, 75, 23, 0);
        pc_chrome_modal(p.u.h, NULL, &p.u.done);
        DestroyWindow(p.u.h);
        return p.u.result == IDOK && out[0];
    }
}

static long read_small_file(const char *path, char *buf, size_t max)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    if (f == INVALID_HANDLE_VALUE) return -1;
    size = GetFileSize(f, NULL);
    if (size == 0xFFFFFFFF || size > max) { CloseHandle(f); return -2; }
    if (!ReadFile(f, buf, size, &got, NULL) || got != size) { CloseHandle(f); return -1; }
    CloseHandle(f);
    return (long)got;
}

static int mode_from_file(const char *manifest_arg, const char *msi_arg)
{
    static char buf[PC_MANIFEST_MAX_BYTES];
    char mpath[MAX_PATH], msi[MAX_PATH], err[200], msg[900];
    pc_manifest *m;
    job_t *j = NULL;
    long n;
    int pol, rc = 1;

    m = (pc_manifest *)malloc(sizeof *m);
    if (!m) return 1;
    if (manifest_arg && *manifest_arg) pc_strlcpy(mpath, manifest_arg, sizeof mpath);
    else {
        pc_msgbox(NULL, UPD_TITLE, "To install an update from a file, select the update information file "
                  "(update-manifest.txt) that was downloaded together with PhotoCraftClassic-Setup.msi.\n\n"
                  "Both files are available on the PhotoCraft Classic releases page.",
                  (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        if (!pick_file("Select update-manifest.txt", "Update information (*.txt)\0*.txt\0All files (*.*)\0*.*\0", mpath, sizeof mpath))
            goto out;
    }
    n = read_small_file(mpath, buf, sizeof buf);
    if (n < 0) {
        pc_snprintf(msg, sizeof msg, "Could not read %s.", mpath);
        pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        goto out;
    }
    if (pc_manifest_load(buf, (size_t)n, pc_update_pubkeys, PC_UPDATE_NUM_PUBKEYS, m, err, sizeof err)) {
        logmsg("from-file: manifest rejected: %s", err);
        pc_snprintf(msg, sizeof msg, "This file is not valid PhotoCraft Classic update information, or it has been "
                    "modified.\n\n(%s)", err);
        pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        goto out;
    }
    pol = pc_manifest_policy(m, cfg.installed, cfg.last_seq, cfg.channel);
    if (pol == PC_POLICY_CURRENT) {
        pc_snprintf(msg, sizeof msg, "This update (version %s) is not newer than the installed version (%s).",
                    m->version_str, cfg.installed);
        pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        goto out;
    }
    if (pol != PC_POLICY_UPDATE) { pc_msgbox(NULL, UPD_TITLE, policy_text(pol), (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL); goto out; }
    remember_sequence(m);

    /* The MSI: given, else next to the manifest under its published name. */
    if (msi_arg && *msi_arg) pc_strlcpy(msi, msi_arg, sizeof msi);
    else {
        const char *base = strrchr(m->msi_url, '/');
        char dir[MAX_PATH];
        pc_strlcpy(dir, mpath, sizeof dir);
        path_dir(dir);
        path_join(msi, sizeof msi, dir, base ? base + 1 : "PhotoCraftClassic-Setup.msi");
        if (strchr(msi, '?')) *strchr(msi, '?') = 0;
        if (!file_exists(msi)) {
            pc_snprintf(msg, sizeof msg, "Please locate the installer file for version %s (%s).", m->version_str,
                        base ? base + 1 : "PhotoCraftClassic-Setup.msi");
            pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
            if (!pick_file("Locate the update installer", "Windows Installer packages (*.msi)\0*.msi\0All files (*.*)\0*.*\0", msi, sizeof msi))
                goto out;
        }
    }
    j = job_new(JOB_HASHFILE);
    if (!j) goto out;
    pc_strlcpy(j->path, msi, sizeof j->path);
    run_job(j, NULL, UPD_TITLE, "Verifying the update...", 1, 0);
    if (j->res.error == PC_NET_ERR_CANCELLED) goto out;
    if (j->res.error || j->hashed != m->msi_size || memcmp(j->digest, m->msi_sha256, 32)) {
        logmsg("from-file: %s failed verification", msi);
        pc_snprintf(msg, sizeof msg, "The file %s does not match the update information: it is damaged, "
                    "incomplete, or belongs to a different version.\n\nNothing was changed on your computer.", msi);
        pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        goto out;
    }
    if (show_update_available(m, 1, 0) != ID_INSTALL) { rc = 0; goto out; }
    rc = launch_apply(msi, m->msi_sha256) ? 1 : 0;
    if (rc) pc_msgbox(NULL, UPD_TITLE, "The update installer could not be started.", (LPCSTR)IDI_HAND, "OK", NULL, NULL);
out:
    job_free(j);
    free(m);
    return rc;
}

/* ================================================================== */
/* --apply                                                             */

/* Wait for a process handle while keeping windows painted.  Returns 0 when
 * it exited, -1 on timeout. */
static int wait_handle(HANDLE h, DWORD timeout_ms)
{
    DWORD start = GetTickCount();
    for (;;) {
        DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, 200, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return 0;
        if (r == WAIT_FAILED) return -2;
        pc_chrome_pump();
        if (timeout_ms != INFINITE && GetTickCount() - start >= timeout_ms) return -1;
    }
}

static void sleep_pumping(DWORD ms)
{
    DWORD start = GetTickCount();
    while (GetTickCount() - start < ms) {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
        pc_chrome_pump();
    }
}

/* Ask every running PhotoCraft Classic to close (it prompts to save) and
 * wait.  Returns 0 when none is left, -1 if the user cancelled. */
static int close_app(HWND status, DWORD pid)
{
    UINT msg = RegisterWindowMessageA(APP_CLOSE_MSG);
    int rounds = 0;
    for (;;) {
        HWND w = FindWindowA(APP_WNDCLASS, NULL);
        DWORD wpid = 0;
        HANDLE ph = NULL;
        int r;
        if (w) GetWindowThreadProcessId(w, &wpid);
        else wpid = pid;
        if (wpid) ph = OpenProcess(SYNCHRONIZE, FALSE, wpid);
        pid = 0;  /* the explicit PID is only used once */
        if (!w && !ph) return 0;
        SetWindowTextA(status, "Waiting for PhotoCraft Classic to close...");
        if (w) { logmsg("asking window %p (pid %lu) to close", (void *)w, (unsigned long)wpid); PostMessageA(w, msg, 0, 0); }
        if (ph) {
            r = wait_handle(ph, 60000);
            CloseHandle(ph);
        } else {
            /* no process handle: poll for the window */
            DWORD start = GetTickCount();
            r = -1;
            while (GetTickCount() - start < 60000) {
                if (!IsWindow(w)) { r = 0; break; }
                sleep_pumping(250);
            }
        }
        if (r == 0) { sleep_pumping(500); if (++rounds > 20) return 0; continue; }
        if (pc_msgbox(NULL, UPD_TITLE, "PhotoCraft Classic is still running.\n\nPlease save your work and close "
                      "PhotoCraft Classic, then click Retry.", (LPCSTR)IDI_EXCLAMATION, "Retry", "Cancel", NULL) != 1) {
            logmsg("user cancelled while waiting for the application");
            return -1;
        }
    }
}

static void relaunch_app(const char *fallback_dir)
{
    char dir[MAX_PATH], exe[MAX_PATH];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    if (!reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "InstallDir", dir, sizeof dir)) pc_strlcpy(dir, fallback_dir, sizeof dir);
    path_join(exe, sizeof exe, dir, APP_EXE);
    if (!file_exists(exe)) { logmsg("relaunch: %s not found", exe); return; }
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    {
        char cmd[MAX_PATH + 4];
        pc_snprintf(cmd, sizeof cmd, "\"%s\"", exe);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            logmsg("relaunched %s", exe);
        }
    }
}

/* Only delete MSIs we downloaded into our temp folder, never a file the
 * user pointed us at with --from-file. */
static void delete_if_ours(const char *path)
{
    size_t n = strlen(g_tempdir), i;
    if (!n || strlen(path) <= n) return;
    for (i = 0; i < n; i++) if (pc_tolower((unsigned char)path[i]) != pc_tolower((unsigned char)g_tempdir[i])) return;
    if (strchr(path + n, '\\')) return;
    DeleteFileA(path);
}

static int app_present(const char *fallback_dir)
{
    char dir[MAX_PATH], exe[MAX_PATH];
    if (!reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "InstallDir", dir, sizeof dir)) pc_strlcpy(dir, fallback_dir, sizeof dir);
    path_join(exe, sizeof exe, dir, APP_EXE);
    return file_exists(exe);
}

static int apply_command(ui_t *u, int id, int code) { (void)u; (void)id; (void)code; return 1; /* not cancellable */ }

static int mode_apply(const char *msi, DWORD pid, const char *sha_hex, const char *appdir)
{
    ui_t u;
    char cmd[4 * MAX_PATH], sysdir[MAX_PATH], msiexec[MAX_PATH], msg[600];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    int r;
    char fallback_dir[MAX_PATH];

    pc_strlcpy(fallback_dir, appdir && *appdir ? appdir : cfg.app_dir, sizeof fallback_dir);
    logmsg("apply: msi=%s pid=%lu dir=%s", msi, (unsigned long)pid, fallback_dir);

    memset(&u, 0, sizeof u);
    u.on_command = apply_command;
    u.h = pc_chrome_create(WND_CLASS, UPD_TITLE, 320, 92, NULL, &u);
    if (u.h) {
        pc_ctl_icon(u.h, (LPCSTR)IDI_INFORMATION, 12, 12);
        pc_ctl_label_bold(u.h, -1, "Installing the PhotoCraft Classic update", 56, 12, 252, 16);
        u.status = pc_ctl_label(u.h, ID_STATUS, "Preparing...", 56, 34, 252, 44);
        ShowWindow(u.h, SW_SHOW);
        UpdateWindow(u.h);
    }

    /* Re-verify: the file sat in a user-writable folder in between. */
    if (sha_hex && *sha_hex) {
        job_t *j = job_new(JOB_HASHFILE);
        unsigned char want[32];
        int ok = 0;
        if (j && pc_hex_decode(sha_hex, strlen(sha_hex), want, 32) == 32) {
            pc_strlcpy(j->path, msi, sizeof j->path);
            if (u.status) SetWindowTextA(u.status, "Verifying the update...");
            job_run(j);
            ok = j->res.error == PC_NET_OK && !memcmp(j->digest, want, 32);
        }
        job_free(j);
        if (!ok) {
            logmsg("apply: %s failed re-verification", msi);
            pc_msgbox(u.h, UPD_TITLE, "The update file failed verification and was not installed.",
                      (LPCSTR)IDI_HAND, "OK", NULL, NULL);
            if (u.h) DestroyWindow(u.h);
            return 1;
        }
    }

    if (close_app(u.status, pid)) {
        if (u.h) DestroyWindow(u.h);
        pc_msgbox(NULL, UPD_TITLE, "The update was cancelled. PhotoCraft Classic was not changed.",
                  (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        return 1602;
    }

    if (u.status) SetWindowTextA(u.status, "Windows Installer is installing the update. This may take a few minutes.");
    if (!GetSystemDirectoryA(sysdir, sizeof sysdir)) sysdir[0] = 0;
    path_join(msiexec, sizeof msiexec, sysdir, "msiexec.exe");
    if (!file_exists(msiexec)) pc_strlcpy(msiexec, "msiexec.exe", sizeof msiexec);
    pc_snprintf(cmd, sizeof cmd, "\"%s\" /i \"%s\" /qb REBOOT=ReallySuppress", msiexec, msi);
    /* Keep the existing folder and desktop-shortcut choice across the
     * major upgrade (the MSI also reads them back from the registry). */
    if (cfg.install_dir[0]) {
        char d[MAX_PATH];
        size_t n;
        pc_strlcpy(d, cfg.install_dir, sizeof d);
        n = strlen(d);
        if (n > 3 && d[n - 1] == '\\') d[n - 1] = 0;    /* a trailing \ would escape the quote */
        pc_strlcat(cmd, " INSTALLDIR=\"", sizeof cmd);
        pc_strlcat(cmd, d, sizeof cmd);
        pc_strlcat(cmd, "\"", sizeof cmd);
    }
    if (cfg.desktop_shortcut >= 0) pc_strlcat(cmd, cfg.desktop_shortcut ? " DESKTOPSHORTCUT=1" : " DESKTOPSHORTCUT=0", sizeof cmd);
    logmsg("apply: %s", cmd);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, g_tempdir, &si, &pi)) {
        logmsg("apply: cannot start msiexec (%lu)", GetLastError());
        if (u.h) DestroyWindow(u.h);
        pc_msgbox(NULL, UPD_TITLE, "Windows Installer could not be started, so the update was not installed.\n\n"
                  "PhotoCraft Classic was not changed.", (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        relaunch_app(fallback_dir);
        return 1;
    }
    CloseHandle(pi.hThread);
    wait_handle(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code)) code = 1;
    CloseHandle(pi.hProcess);
    logmsg("apply: msiexec exit code %lu", (unsigned long)code);
    if (u.h) { DestroyWindow(u.h); u.h = NULL; }

    switch (code) {
    case 0:
    case 1641:   /* ERROR_SUCCESS_REBOOT_INITIATED */
        delete_if_ours(msi);
        relaunch_app(fallback_dir);
        r = 0;
        break;
    case 3010:   /* ERROR_SUCCESS_REBOOT_REQUIRED */
        delete_if_ours(msi);
        pc_msgbox(NULL, UPD_TITLE, "The update was installed. Please restart Windows to complete the update.",
                  (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        relaunch_app(fallback_dir);
        r = 0;
        break;
    case 1602:   /* ERROR_INSTALL_USEREXIT */
        pc_msgbox(NULL, UPD_TITLE, "The update was cancelled. PhotoCraft Classic was not changed.",
                  (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        relaunch_app(fallback_dir);
        r = 1602;
        break;
    case 1618:   /* ERROR_INSTALL_ALREADY_RUNNING */
        pc_msgbox(NULL, UPD_TITLE, "Another installation is in progress. Please finish it and check for updates again.\n\n"
                  "PhotoCraft Classic was not changed.", (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL);
        relaunch_app(fallback_dir);
        r = (int)code;
        break;
    default:
        /* Windows Installer rolls the whole transaction back (the old
         * version is removed inside it), so the previous version should
         * still be there.  Check instead of assuming: if it is gone (an
         * installer without working rollback), keep the MSI and say how
         * to recover. */
        if (app_present(fallback_dir)) {
            pc_snprintf(msg, sizeof msg, "The update could not be installed (Windows Installer error %lu).\n\n"
                        "Your previous version of PhotoCraft Classic has been kept and will be started again.",
                        (unsigned long)code);
            pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
            relaunch_app(fallback_dir);
        } else {
            logmsg("apply: application missing after failed install");
            pc_snprintf(msg, sizeof msg, "The update could not be installed (Windows Installer error %lu), and "
                        "PhotoCraft Classic could not be restored automatically.\n\nPlease install it again by "
                        "opening this file:\n%s\n\nYour pictures and settings were not changed.",
                        (unsigned long)code, msi);
            pc_msgbox(NULL, UPD_TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        }
        r = (int)code;
        break;
    }
    return r;
}

/* ================================================================== */
/* entry                                                               */

static void cleanup_tempdir(void)
{
    WIN32_FIND_DATAA fd;
    char pat[MAX_PATH], p[MAX_PATH];
    HANDLE h;
    path_join(pat, sizeof pat, g_tempdir, "*.*");
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const char *n = fd.cFileName, *dot = strrchr(n, '.');
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!pc_stricmp(n, "PCUpdate-apply.exe") || (dot && (!pc_stricmp(dot, ".msi") || !pc_stricmp(dot, ".part")))) {
            path_join(p, sizeof p, g_tempdir, n);
            DeleteFileA(p);   /* fails harmlessly for a file in use */
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* Split a command line into at most 'max' arguments (quotes respected). */
static int split_args(char *s, char **argv, int max)
{
    int n = 0;
    while (*s && n < max) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '"') {
            argv[n++] = ++s;
            while (*s && *s != '"') s++;
        } else {
            argv[n++] = s;
            while (*s && *s != ' ' && *s != '\t') s++;
        }
        if (*s) *s++ = 0;
    }
    return n;
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int show)
{
    static char line[2048];
    char *argv[12];
    int argc, i, rc = 0, automatic = 0, force = 0;
    const char *mode = "check", *a1 = NULL, *a2 = NULL, *a3 = NULL, *a4 = NULL;
    HANDLE mutex = NULL;
    (void)prev; (void)show;

    pc_strlcpy(line, cmdline ? cmdline : "", sizeof line);
    argc = split_args(line, argv, 12);
    for (i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!pc_stricmp(a, "--check") || !pc_stricmp(a, "/check")) mode = "check";
        else if (!pc_stricmp(a, "--auto") || !pc_stricmp(a, "/auto")) { mode = "check"; automatic = 1; }
        else if (!pc_stricmp(a, "--force")) force = 1;
        else if (!pc_stricmp(a, "--pid") && i + 1 < argc) cfg.app_pid = (DWORD)pc_atol(argv[++i]);
        else if (!pc_stricmp(a, "--from-file") || !pc_stricmp(a, "/from-file")) {
            mode = "file";
            if (i + 1 < argc && argv[i + 1][0] != '-') a1 = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') a2 = argv[++i];
        } else if (!pc_stricmp(a, "--apply") && i + 2 < argc) {
            mode = "apply";
            a1 = argv[++i];
            a2 = argv[++i];
            if (i + 1 < argc) a3 = argv[++i];
            if (i + 1 < argc) a4 = argv[++i];
        } else if (!strcmp(a, "/?") || !pc_stricmp(a, "--help")) mode = "help";
    }

    {
        DWORD pid = cfg.app_pid;
        init_tempdir();
        load_config();
        cfg.app_pid = pid;
    }
    pc_chrome_init(hinst);
    pc_chrome_register_class(WND_CLASS, ui_proc);
    seed_load();
    logmsg("PCUpdate %s start: mode=%s auto=%d installed=%s", PC_VERSION, mode, automatic, cfg.installed);

    if (!strcmp(mode, "help")) {
        pc_msgbox(NULL, UPD_TITLE,
                  "PCUpdate.exe [--check]\tcheck for updates now\n"
                  "PCUpdate.exe --auto\tdaily background check\n"
                  "PCUpdate.exe --from-file [manifest [msi]]\n\t\tinstall an update downloaded elsewhere",
                  (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        return 0;
    }
    if (!strcmp(mode, "apply")) {
        rc = mode_apply(a1, (DWORD)pc_atol(a2), a3, a4);
        seed_save();
        logmsg("PCUpdate exit %d", rc);
        return rc;
    }

    mutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        logmsg("another updater is running");
        if (!automatic) {
            HWND other = FindWindowA(WND_CLASS, NULL);
            if (other) SetForegroundWindow(other);
        }
        CloseHandle(mutex);
        return 0;
    }
    cleanup_tempdir();
    if (!strcmp(mode, "file")) rc = mode_from_file(a1, a2);
    else rc = mode_check(automatic, force);
    seed_save();
    pc_net_cleanup();
    logmsg("PCUpdate exit %d", rc);
    if (mutex) CloseHandle(mutex);
    return rc;
}
