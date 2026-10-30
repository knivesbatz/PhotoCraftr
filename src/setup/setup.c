/*
 * PhotoCraft Classic - Setup.exe bootstrapper.
 *
 * A Windows 95 style wizard (Welcome, License, Destination, Options,
 * Ready, Installing, Finished) in front of PhotoCraftClassic-Setup.msi:
 *
 *  1. detects the Windows version (GetVersionExA) and the Windows Installer
 *     version (msi.dll file version via version.dll);
 *  2. if Windows Installer is missing or older than 1.1, runs the matching
 *     redistributable from Setup.exe's own folder: InstMsiA.exe on Windows
 *     95/98/Me, InstMsiW.exe on Windows NT 4.0 (never the other one), and
 *     resumes after the reboot it usually needs (RunOnce);
 *  3. runs msiexec /i PhotoCraftClassic-Setup.msi INSTALLDIR=... DESKTOPSHORTCUT=...
 *
 *   Setup.exe [/S] [/NOSHORTCUT] [/D=<folder>]
 *     /S            silent: no wizard, Windows Installer runs with /qn
 *     /NOSHORTCUT   do not create the desktop shortcut
 *     /D=<folder>   destination (must be last; may contain spaces)
 *   Exit code: 0 success, 3010 success but restart needed, 1602 cancelled,
 *   otherwise an error (Windows Installer error codes are passed through).
 */
#include <windows.h>
#include "pcrt.h"
#include "../updater/chrome.h"

#ifndef PC_VERSION
#define PC_VERSION "0.0.0"
#endif

#define TITLE       "PhotoCraft Classic Setup"
#define MSI_NAME    "PhotoCraftClassic-Setup.msi"
#define APP_EXE     "PhotoCraftClassic.exe"
#define REG_APP     "Software\\PhotoCraft Classic"
#define RUNONCE_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
#define RUNONCE_VAL "PhotoCraftClassicSetup"
#define WIZ_CLASS   "PCSetupWizard"
#define DIR_CLASS   "PCSetupFolder"

extern const char pc_license_text[];

enum {
    ID_BACK = 200, ID_NEXT, ID_CANCEL, ID_ACCEPT, ID_DIR, ID_BROWSE, ID_SHORTCUT, ID_LAUNCH,
    ID_LICENSE, ID_STATUS, ID_BAR, ID_LIST, ID_PATH, ID_OK
};

enum { PG_WELCOME, PG_LICENSE, PG_DEST, PG_OPTIONS, PG_READY, PG_INSTALL, PG_DONE, PG_COUNT };

/* ================================================================== */
/* environment detection                                               */

typedef struct {
    int platform;       /* VER_PLATFORM_* */
    DWORD major, minor, build;
    char name[96];
    int msi_major, msi_minor;  /* 0,0 = not installed */
} sysinfo_t;

static sysinfo_t sys;
static char setup_dir[MAX_PATH];

static void detect_os(void)
{
    OSVERSIONINFOA v;
    const char *n = "Windows";
    memset(&v, 0, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    GetVersionExA(&v);
    sys.platform = (int)v.dwPlatformId;
    sys.major = v.dwMajorVersion;
    sys.minor = v.dwMinorVersion;
    sys.build = v.dwBuildNumber & 0xFFFF;
    if (v.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        n = v.dwMinorVersion >= 90 ? "Windows Me" : v.dwMinorVersion >= 10 ? "Windows 98" : "Windows 95";
    } else if (v.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        if (v.dwMajorVersion <= 4) n = "Windows NT 4.0";
        else if (v.dwMajorVersion == 5 && v.dwMinorVersion == 0) n = "Windows 2000";
        else if (v.dwMajorVersion == 5 && v.dwMinorVersion == 1) n = "Windows XP";
        else if (v.dwMajorVersion == 5) n = "Windows Server 2003";
        else if (v.dwMajorVersion == 6 && v.dwMinorVersion == 0) n = "Windows Vista";
        else if (v.dwMajorVersion == 6 && v.dwMinorVersion == 1) n = "Windows 7";
        else n = "Windows 8 or later";
    } else {
        n = "Win32s";
    }
    pc_snprintf(sys.name, sizeof sys.name, "%s%s%s", n, v.szCSDVersion[0] ? " " : "", v.szCSDVersion);
}

static void detect_msi(void)
{
    char path[MAX_PATH];
    DWORD h = 0, size;
    void *buf;
    VS_FIXEDFILEINFO *fi = NULL;
    UINT len = 0;
    sys.msi_major = sys.msi_minor = 0;
    if (!GetSystemDirectoryA(path, sizeof path)) return;
    pc_strlcat(path, "\\msi.dll", sizeof path);
    size = GetFileVersionInfoSizeA(path, &h);
    if (!size) return;
    buf = malloc(size);
    if (!buf) return;
    if (GetFileVersionInfoA(path, 0, size, buf) && VerQueryValueA(buf, "\\", (void **)&fi, &len) && fi && len >= sizeof *fi) {
        sys.msi_major = HIWORD(fi->dwFileVersionMS);
        sys.msi_minor = LOWORD(fi->dwFileVersionMS);
    }
    free(buf);
}

/* Windows Installer 1.1 ships msi.dll 1.10.x (1.0 was 1.0.x, 1.2 = 1.20, 2.0 = 2.0). */
static int msi_ok(void) { return sys.msi_major > 1 || (sys.msi_major == 1 && sys.msi_minor >= 10); }

static int file_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != 0xFFFFFFFF && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int dir_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int reg_get_str(HKEY root, const char *sub, const char *name, char *out, DWORD outsz)
{
    HKEY k;
    DWORD type = 0, sz = outsz;
    LONG rc;
    out[0] = 0;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    rc = RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz);
    RegCloseKey(k);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) { out[0] = 0; return 0; }
    out[sz < outsz ? sz : outsz - 1] = 0;
    return out[0] != 0;
}

static void default_dir(char *out, size_t n)
{
    char pf[MAX_PATH];
    size_t l;
    if (reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "InstallDir", out, (DWORD)n)) {
        l = strlen(out);
        if (l > 3 && out[l - 1] == '\\') out[l - 1] = 0;
        return;
    }
    if (!reg_get_str(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir", pf, sizeof pf))
        pc_strlcpy(pf, "C:\\Program Files", sizeof pf);
    l = strlen(pf);
    if (l && pf[l - 1] == '\\') pf[l - 1] = 0;
    pc_snprintf(out, n, "%s\\PhotoCraft Classic", pf);
}

/* "C:\..." or "\\server\share\...", no characters Windows forbids. */
static int valid_dir(const char *d)
{
    size_t n = strlen(d), i;
    if (n < 3 || n > MAX_PATH - 40) return 0;
    if (!((((d[0] | 32) >= 'a' && (d[0] | 32) <= 'z') && d[1] == ':' && d[2] == '\\') || (d[0] == '\\' && d[1] == '\\'))) return 0;
    for (i = 2; i < n; i++) if (strchr("<>\"|?*:", d[i]) || (unsigned char)d[i] < 32) return 0;
    return 1;
}

/* ================================================================== */
/* running programs                                                    */

static DWORD run_wait(const char *cmd, const char *cwd, int pump)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 0xFFFFFFFF;
    char buf[4 * MAX_PATH];
    pc_strlcpy(buf, cmd, sizeof buf);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, buf, NULL, NULL, FALSE, 0, NULL, cwd, &si, &pi)) return 0xFFFFFFFF;
    CloseHandle(pi.hThread);
    for (;;) {
        DWORD r = pump ? MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 100, QS_ALLINPUT)
                       : WaitForSingleObject(pi.hProcess, INFINITE);
        if (r == WAIT_OBJECT_0) break;
        if (r == WAIT_FAILED) break;
        if (pump) pc_chrome_pump();
    }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code;
}

static void quote_dir(char *out, size_t n, const char *dir)
{
    char d[MAX_PATH];
    size_t l;
    pc_strlcpy(d, dir, sizeof d);
    l = strlen(d);
    if (l > 3 && d[l - 1] == '\\') d[l - 1] = 0;   /* \" would escape the quote */
    pc_snprintf(out, n, "\"%s\"", d);
}

/* ================================================================== */
/* restart handling                                                    */

static void runonce_set(const char *args)
{
    char self[MAX_PATH], cmd[2 * MAX_PATH + 64];
    HKEY k;
    DWORD disp;
    GetModuleFileNameA(NULL, self, sizeof self);
    pc_snprintf(cmd, sizeof cmd, "\"%s\" /resume %s", self, args);
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, RUNONCE_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &k, &disp) == ERROR_SUCCESS) {
        RegSetValueExA(k, RUNONCE_VAL, 0, REG_SZ, (const BYTE *)cmd, (DWORD)strlen(cmd) + 1);
        RegCloseKey(k);
    }
}

static void runonce_clear(void)
{
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, RUNONCE_KEY, 0, KEY_WRITE, &k) == ERROR_SUCCESS) {
        RegDeleteValueA(k, RUNONCE_VAL);
        RegCloseKey(k);
    }
}

typedef BOOL (WINAPI *opt_fn)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *lpv_fn)(LPCSTR, LPCSTR, PLUID);
typedef BOOL (WINAPI *atp_fn)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);

static void restart_windows(void)
{
    if (sys.platform == VER_PLATFORM_WIN32_NT) {
        /* NT needs SE_SHUTDOWN_NAME; the token API does not exist on 9x,
         * so it is looked up at run time. */
        HMODULE adv = LoadLibraryA("ADVAPI32.DLL");
        if (adv) {
            FARPROC a = GetProcAddress(adv, "OpenProcessToken"), b = GetProcAddress(adv, "LookupPrivilegeValueA"),
                    c = GetProcAddress(adv, "AdjustTokenPrivileges");
            if (a && b && c) {
                opt_fn opt; lpv_fn lpv; atp_fn atp;
                HANDLE tok;
                TOKEN_PRIVILEGES tp;
                memcpy(&opt, &a, sizeof a); memcpy(&lpv, &b, sizeof b); memcpy(&atp, &c, sizeof c);
                if (opt(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) {
                    memset(&tp, 0, sizeof tp);
                    tp.PrivilegeCount = 1;
                    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    if (lpv(NULL, "SeShutdownPrivilege", &tp.Privileges[0].Luid)) atp(tok, FALSE, &tp, 0, NULL, NULL);
                    CloseHandle(tok);
                }
            }
        }
    }
    ExitWindowsEx(EWX_REBOOT, 0);
}

/* ================================================================== */
/* install steps (shared by wizard and /S)                             */

typedef struct {
    char dir[MAX_PATH];
    int shortcut;
    int silent;
    HWND owner;          /* wizard window, or NULL */
    HWND status;         /* status label, or NULL */
} options_t;

static void say(options_t *o, const char *s)
{
    if (o->status) { SetWindowTextA(o->status, s); pc_chrome_pump(); }
}

static void error_box(options_t *o, const char *text)
{
    if (!o->silent) pc_msgbox(o->owner, TITLE, text, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
}

/* Returns 0 if Windows Installer is usable now, 3010 if a restart is
 * needed first, or an error code. */
static DWORD ensure_windows_installer(options_t *o)
{
    const char *redist;
    char path[MAX_PATH], cmd[MAX_PATH + 16], msg[700], args[MAX_PATH + 40];
    DWORD rc;
    if (msi_ok()) return 0;
    if (sys.platform == VER_PLATFORM_WIN32_WINDOWS) redist = "InstMsiA.exe";
    else if (sys.platform == VER_PLATFORM_WIN32_NT && sys.major == 4) redist = "InstMsiW.exe";
    else {
        pc_snprintf(msg, sizeof msg, "Windows Installer 1.1 or later is required, but it is %s on this computer.\n\n"
                    "Please install the Windows Installer update for %s from Windows Update, then run Setup again.",
                    sys.msi_major ? "too old" : "missing", sys.name);
        error_box(o, msg);
        return 1603;
    }
    pc_snprintf(path, sizeof path, "%s\\%s", setup_dir, redist);
    if (!file_exists(path)) {
        pc_snprintf(msg, sizeof msg,
                    "PhotoCraft Classic needs Windows Installer 1.1 or later, which is %s on this computer (%s).\n\n"
                    "Setup can install it automatically if the file %s is in the same folder as Setup.exe:\n%s\n\n"
                    "%s is Microsoft's \"Windows Installer 2.0 Redistributable for %s\". It is no longer offered by "
                    "Microsoft; copy it from an old Windows/Office/MSDN CD or another trusted archive, put it next to "
                    "Setup.exe and run Setup again.",
                    sys.msi_major ? "too old" : "not installed", sys.name, redist, setup_dir, redist,
                    sys.platform == VER_PLATFORM_WIN32_WINDOWS ? "Windows 95, 98 and Me" : "Windows NT 4.0");
        error_box(o, msg);
        return 1603;
    }
    say(o, "Installing Windows Installer...");
    /* Register the resume entry first, in case the redistributable
     * restarts Windows on its own. */
    pc_snprintf(args, sizeof args, "%s%s/D=%s", o->silent ? "/S " : "", o->shortcut ? "" : "/NOSHORTCUT ", o->dir);
    runonce_set(args);
    pc_snprintf(cmd, sizeof cmd, "\"%s\" /q", path);
    rc = run_wait(cmd, setup_dir, o->status != NULL);
    if (rc == 0) {
        detect_msi();
        if (msi_ok()) { runonce_clear(); return 0; }
        return 3010;   /* installed but not active until restart (typical on 9x) */
    }
    if (rc == 3010 || rc == 1641) return 3010;
    runonce_clear();
    pc_snprintf(msg, sizeof msg, "Windows Installer could not be installed (%s returned error %lu).", redist, (unsigned long)rc);
    error_box(o, msg);
    return rc ? rc : 1603;
}

static DWORD run_msi(options_t *o)
{
    char msi[MAX_PATH], cmd[4 * MAX_PATH], qd[MAX_PATH + 4], sysdir[MAX_PATH], exe[MAX_PATH], logp[MAX_PATH], msg[400];
    DWORD rc;
    pc_snprintf(msi, sizeof msi, "%s\\%s", setup_dir, MSI_NAME);
    if (!file_exists(msi)) {
        pc_snprintf(msg, sizeof msg, "The installation package %s was not found in\n%s\n\nPlease copy the complete "
                    "PhotoCraft Classic release folder and run Setup again.", MSI_NAME, setup_dir);
        error_box(o, msg);
        return 1619;   /* ERROR_INSTALL_PACKAGE_OPEN_FAILED */
    }
    if (!GetSystemDirectoryA(sysdir, sizeof sysdir)) sysdir[0] = 0;
    pc_snprintf(exe, sizeof exe, "%s\\msiexec.exe", sysdir);
    if (!file_exists(exe)) pc_strlcpy(exe, "msiexec.exe", sizeof exe);
    if (!GetTempPathA(sizeof logp, logp)) logp[0] = 0;
    pc_strlcat(logp, "PhotoCraftClassic-Setup.log", sizeof logp);
    quote_dir(qd, sizeof qd, o->dir);
    /* EXPLICITOPTIONS=1: on an upgrade the MSI would otherwise keep the
     * previous folder/shortcut choice instead of what the user picked. */
    pc_snprintf(cmd, sizeof cmd, "\"%s\" /i \"%s\" INSTALLDIR=%s DESKTOPSHORTCUT=%d EXPLICITOPTIONS=1 %s /l*v \"%s\"",
                exe, msi, qd, o->shortcut ? 1 : 0, o->silent ? "/qn" : "/qb", logp);
    say(o, "Windows Installer is installing PhotoCraft Classic...");
    rc = run_wait(cmd, setup_dir, o->status != NULL);
    if (rc == 0xFFFFFFFF) { error_box(o, "Windows Installer (msiexec.exe) could not be started."); return 1603; }
    return rc;
}

static DWORD install(options_t *o)
{
    DWORD rc = ensure_windows_installer(o);
    if (rc) return rc;
    rc = run_msi(o);
    return rc;
}

static void launch_app(const char *dir)
{
    char exe[MAX_PATH + 2], cmd[MAX_PATH + 4];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    pc_snprintf(exe, sizeof exe, "%s\\%s", dir, APP_EXE);
    if (!file_exists(exe)) return;
    pc_snprintf(cmd, sizeof cmd, "\"%s\"", exe);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}

/* ================================================================== */
/* folder browser (own dialog: no shell32 dependency)                  */

typedef struct {
    volatile int done;
    int ok;
    char cur[MAX_PATH];      /* directory being listed (exists) */
    char result[MAX_PATH];
    HWND h, path, list;
} dirdlg_t;

static int cmp_names(const void *a, const void *b)
{
    return pc_stricmp(*(const char *const *)a, *(const char *const *)b);
}

static void dir_fill(dirdlg_t *d)
{
    WIN32_FIND_DATAA fd;
    char pat[MAX_PATH + 4];
    HANDLE f;
    DWORD drives = GetLogicalDrives();
    int i;
    SendMessageA(d->list, WM_SETREDRAW, FALSE, 0);
    SendMessageA(d->list, LB_RESETCONTENT, 0, 0);
    if (strlen(d->cur) > 3) SendMessageA(d->list, LB_ADDSTRING, 0, (LPARAM)"[..]");
    pc_snprintf(pat, sizeof pat, "%s%s*.*", d->cur, d->cur[strlen(d->cur) - 1] == '\\' ? "" : "\\");
    f = FindFirstFileA(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        /* folders sorted case-insensitively, then drives (Windows 3.1 style) */
        char **names = (char **)malloc(512 * sizeof *names);
        int n = 0, k;
        do {
            size_t l;
            if (!names || n >= 512) break;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
            l = strlen(fd.cFileName);
            names[n] = (char *)malloc(l + 3);
            if (!names[n]) break;
            pc_snprintf(names[n], l + 3, "[%s]", fd.cFileName);
            n++;
        } while (FindNextFileA(f, &fd));
        FindClose(f);
        if (names) {
            qsort(names, (size_t)n, sizeof *names, cmp_names);
            for (k = 0; k < n; k++) { SendMessageA(d->list, LB_ADDSTRING, 0, (LPARAM)names[k]); free(names[k]); }
            free(names);
        }
    }
    for (i = 2; i < 26; i++) {   /* drives, Windows 3.1 style; skip A: and B: */
        char root[4], item[8];
        UINT t;
        if (!(drives & (1UL << i))) continue;
        pc_snprintf(root, sizeof root, "%c:\\", 'A' + i);
        t = GetDriveTypeA(root);
        if (t != DRIVE_FIXED && t != DRIVE_REMOTE) continue;
        pc_snprintf(item, sizeof item, "[-%c-]", 'a' + i);
        SendMessageA(d->list, LB_ADDSTRING, 0, (LPARAM)item);
    }
    SendMessageA(d->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(d->list, NULL, TRUE);
}

static void dir_enter(dirdlg_t *d)
{
    char item[MAX_PATH + 4], next[MAX_PATH];
    int sel = (int)SendMessageA(d->list, LB_GETCURSEL, 0, 0);
    size_t n;
    if (sel < 0 || SendMessageA(d->list, LB_GETTEXTLEN, sel, 0) >= (LRESULT)sizeof item) return;
    SendMessageA(d->list, LB_GETTEXT, sel, (LPARAM)item);
    n = strlen(item);
    if (n < 3) return;
    if (!strcmp(item, "[..]")) {
        char *s;
        pc_strlcpy(next, d->cur, sizeof next);
        s = strrchr(next, '\\');
        if (s) { if (s - next == 2) s[1] = 0; else *s = 0; }
    } else if (item[1] == '-' && n == 5) {
        pc_snprintf(next, sizeof next, "%c:\\", pc_toupper(item[2]));
    } else {
        item[n - 1] = 0;
        pc_snprintf(next, sizeof next, "%s%s%s", d->cur, d->cur[strlen(d->cur) - 1] == '\\' ? "" : "\\", item + 1);
    }
    if (!dir_exists(next)) return;
    pc_strlcpy(d->cur, next, sizeof d->cur);
    SetWindowTextA(d->path, d->cur);
    dir_fill(d);
}

static LRESULT CALLBACK dir_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    LRESULT lr;
    dirdlg_t *d;
    if (pc_chrome_proc(h, m, wp, lp, &lr)) return lr;
    d = (dirdlg_t *)pc_chrome_param(h);
    if (!d) return DefWindowProcA(h, m, wp, lp);
    if (m == WM_COMMAND) {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == ID_LIST && code == LBN_DBLCLK) { dir_enter(d); return 0; }
        if ((id == ID_OK && code == BN_CLICKED) || id == IDOK) {
            if (GetFocus() == d->list) { dir_enter(d); return 0; }
            GetWindowTextA(d->path, d->result, sizeof d->result);
            if (!valid_dir(d->result)) {
                pc_msgbox(h, "Choose Folder", "Please enter a complete folder path, for example\nC:\\Program Files\\PhotoCraft Classic",
                          (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL);
                return 0;
            }
            d->ok = 1; d->done = 1;
            return 0;
        }
        if ((id == ID_CANCEL && code == BN_CLICKED) || id == IDCANCEL) { d->done = 1; return 0; }
        return 0;
    }
    if (m == WM_CLOSE) { d->done = 1; return 0; }
    return DefWindowProcA(h, m, wp, lp);
}

static int browse_folder(HWND owner, char *dir, size_t n)
{
    dirdlg_t d;
    char *s;
    memset(&d, 0, sizeof d);
    /* list the nearest existing ancestor of the current choice */
    pc_strlcpy(d.cur, dir, sizeof d.cur);
    while (!dir_exists(d.cur) && (s = strrchr(d.cur, '\\')) != NULL) { if (s - d.cur <= 2) { s[1] = 0; break; } *s = 0; }
    if (!dir_exists(d.cur)) pc_strlcpy(d.cur, "C:\\", sizeof d.cur);
    d.h = pc_chrome_create(DIR_CLASS, "Choose Folder", 300, 270, owner, &d);
    if (!d.h) return 0;
    pc_ctl_label(d.h, -1, "Path:", 12, 12, 276, 14);
    d.path = pc_ctl_edit(d.h, ID_PATH, dir, 12, 28, 276, 22, ES_AUTOHSCROLL);
    pc_ctl_label(d.h, -1, "Directories (double-click to open):", 12, 58, 276, 14);
    d.list = pc_ctl_list(d.h, ID_LIST, 12, 74, 276, 150, 0);
    pc_ctl_button(d.h, ID_OK, "OK", 300 - 12 - 75 - 6 - 75, 270 - 12 - 23, 75, 23, 1);
    pc_ctl_button(d.h, ID_CANCEL, "Cancel", 300 - 12 - 75, 270 - 12 - 23, 75, 23, 0);
    dir_fill(&d);
    pc_chrome_modal(d.h, owner, &d.done);
    DestroyWindow(d.h);
    if (d.ok) pc_strlcpy(dir, d.result, n);
    return d.ok;
}

/* ================================================================== */
/* wizard                                                              */

#define CW 476
#define CH 330
#define PANEL_W 132
#define PX (12 + PANEL_W + 14)
#define PW (CW - PX - 14)

typedef struct {
    HWND h;
    volatile int done;
    int page, result;
    options_t opt;
    HWND back, next, cancel;
    HWND ctl[PG_COUNT][8];  /* per-page controls */
    HWND accept, dir, shortcut, launch, status, bar, summary, info;
    int installing;
    DWORD install_rc;
} wizard_t;

static void show_page(wizard_t *w, int pg)
{
    int i, j;
    for (i = 0; i < PG_COUNT; i++)
        for (j = 0; j < 8; j++)
            if (w->ctl[i][j]) ShowWindow(w->ctl[i][j], i == pg ? SW_SHOW : SW_HIDE);
    w->page = pg;
    EnableWindow(w->back, pg > PG_WELCOME && pg < PG_INSTALL);
    EnableWindow(w->next, pg != PG_INSTALL && (pg != PG_LICENSE || pc_ctl_get_check(w->accept)));
    EnableWindow(w->cancel, pg != PG_INSTALL && pg != PG_DONE);
    SetWindowTextA(w->next, pg == PG_READY ? "&Install" : pg == PG_DONE ? "&Finish" : "&Next >");
    if (pg == PG_READY) {
        char s[MAX_PATH + 300];
        GetWindowTextA(w->dir, w->opt.dir, sizeof w->opt.dir);
        pc_snprintf(s, sizeof s, "Destination folder:\r\n    %s\r\n\r\nStart Menu folder:\r\n    PhotoCraft Classic\r\n\r\n"
                    "Desktop shortcut:\r\n    %s\r\n\r\nWindows Installer:\r\n    %s",
                    w->opt.dir, pc_ctl_get_check(w->shortcut) ? "Yes" : "No",
                    msi_ok() ? "Installed" : sys.platform == VER_PLATFORM_WIN32_WINDOWS ? "Will be installed (InstMsiA.exe)" :
                    sys.platform == VER_PLATFORM_WIN32_NT && sys.major == 4 ? "Will be installed (InstMsiW.exe)" : "Missing");
        SetWindowTextA(w->summary, s);
    }
    InvalidateRect(w->h, NULL, TRUE);
    if (IsWindowEnabled(w->next)) {
        pc_ctl_set_default(w->h, ID_NEXT);
        /* SetFocus on a hidden window would activate it before it can
         * become the foreground window (caption would stay inactive). */
        if (IsWindowVisible(w->h)) SetFocus(w->next);
    }
}

static void paint_panel(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT r, mon, scr, base;
    HICON ic = LoadIconA(pc_chrome_instance(), MAKEINTRESOURCEA(1));
    int y;
    SetRect(&r, 12, 12, 12 + PANEL_W, CH - 12 - 23 - 14);
    pc_draw_sunken(dc, r);
    InflateRect(&r, -2, -2);
    /* teal "desktop" with a dithered horizon, like the Windows 95 Setup art */
    for (y = r.top; y < r.bottom; y += 4) {
        RECT b;
        int t = (y - r.top) * 255 / (r.bottom - r.top);
        SetRect(&b, r.left, y, r.right, y + 4 > r.bottom ? r.bottom : y + 4);
        pc_fill(dc, b, RGB(0, 128 - t / 4, 128 - t / 4));
    }
    /* monitor */
    SetRect(&mon, r.left + 20, r.top + 70, r.right - 20, r.top + 70 + 76);
    pc_fill(dc, mon, PC_RGB_FACE);
    pc_draw_raised(dc, mon);
    scr = mon;
    InflateRect(&scr, -8, -8);
    scr.bottom -= 6;
    pc_draw_sunken(dc, scr);
    InflateRect(&scr, -2, -2);
    pc_fill(dc, scr, PC_RGB_NAVY);
    if (ic) DrawIcon(dc, scr.left + (scr.right - scr.left - 32) / 2, scr.top + (scr.bottom - scr.top - 32) / 2, ic);
    SetRect(&base, mon.left + 24, mon.bottom, mon.right - 24, mon.bottom + 6);
    pc_fill(dc, base, PC_RGB_SHADOW);
    SetRect(&base, mon.left + 8, mon.bottom + 6, mon.right - 8, mon.bottom + 12);
    pc_fill(dc, base, PC_RGB_FACE);
    pc_draw_raised(dc, base);
    {
        HFONT of = (HFONT)SelectObject(dc, pc_chrome_bold());
        RECT t;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, PC_RGB_HILITE);
        SetRect(&t, r.left, r.top + 14, r.right, r.top + 60);
        DrawTextA(dc, "PhotoCraft\nClassic", -1, &t, DT_CENTER | DT_NOPREFIX);
        SelectObject(dc, pc_chrome_font());
        SetTextColor(dc, PC_RGB_FACE);
        SetRect(&t, r.left, r.bottom - 22, r.right, r.bottom - 6);
        DrawTextA(dc, "Version " PC_VERSION, -1, &t, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE);
        SelectObject(dc, of);
    }
    EndPaint(h, &ps);
}

static void do_install(wizard_t *w)
{
    char msg[600];
    DWORD rc;
    GetWindowTextA(w->dir, w->opt.dir, sizeof w->opt.dir);
    w->opt.shortcut = pc_ctl_get_check(w->shortcut);
    w->opt.owner = w->h;
    w->opt.status = w->status;
    show_page(w, PG_INSTALL);
    SetTimer(w->h, 1, 300, NULL);
    w->installing = 1;
    rc = install(&w->opt);
    w->installing = 0;
    KillTimer(w->h, 1);
    pc_progress_set(w->bar, 1000);
    w->install_rc = rc;
    switch (rc) {
    case 0:
        SetWindowTextA(w->info, "PhotoCraft Classic has been installed on your computer.\r\n\r\n"
                       "You can start it from the Start menu (Programs > PhotoCraft Classic).");
        show_page(w, PG_DONE);
        break;
    case 3010:
    case 1641:
        if (msi_ok()) {
            SetWindowTextA(w->info, "PhotoCraft Classic has been installed. Please restart Windows to complete the installation.");
            ShowWindow(w->launch, SW_HIDE);
            pc_ctl_set_check(w->launch, 0);
            show_page(w, PG_DONE);
            ShowWindow(w->launch, SW_HIDE);
        } else {
            /* Windows Installer itself needs a restart; RunOnce resumes Setup. */
            if (pc_msgbox(w->h, TITLE, "Windows must be restarted to finish installing Windows Installer.\n\n"
                          "Setup will continue automatically after the restart.", (LPCSTR)IDI_QUESTION,
                          "Restart Now", "Restart Later", NULL) == 1)
                restart_windows();
            w->result = 3010;
            w->done = 1;
        }
        break;
    case 1602:
        pc_msgbox(w->h, TITLE, "Installation was cancelled. Nothing was changed.", (LPCSTR)IDI_INFORMATION, "OK", NULL, NULL);
        show_page(w, PG_READY);
        break;
    default:
        if (rc != 1603 || msi_ok()) {
            pc_snprintf(msg, sizeof msg, "PhotoCraft Classic could not be installed (Windows Installer error %lu).\n\n"
                        "Any changes were rolled back. A log file was written to\n%%TEMP%%\\PhotoCraftClassic-Setup.log", (unsigned long)rc);
            if (rc != 1619) pc_msgbox(w->h, TITLE, msg, (LPCSTR)IDI_HAND, "OK", NULL, NULL);
        }
        show_page(w, PG_READY);
        break;
    }
}

static int confirm_cancel(wizard_t *w)
{
    return pc_msgbox(w->h, TITLE, "Setup is not complete. If you quit now, PhotoCraft Classic will not be installed.\n\n"
                     "Do you want to quit Setup?", (LPCSTR)IDI_QUESTION, "Yes", "No", NULL) == 1;
}

static LRESULT CALLBACK wiz_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    LRESULT lr;
    wizard_t *w;
    if (pc_chrome_proc(h, m, wp, lp, &lr)) return lr;
    w = (wizard_t *)pc_chrome_param(h);
    if (!w) return DefWindowProcA(h, m, wp, lp);
    switch (m) {
    case WM_PAINT:
        paint_panel(h);
        return 0;
    case WM_TIMER:
        if (w->installing) {
            int pos = (int)GetWindowLongA(w->bar, GWL_USERDATA);
            pc_progress_set(w->bar, pos < 900 ? pos + (900 - pos) / 25 + 1 : pos);
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (w->installing) return 0;
        if (id == ID_ACCEPT && code == BN_CLICKED) { EnableWindow(w->next, pc_ctl_get_check(w->accept)); return 0; }
        if (id == ID_BROWSE && code == BN_CLICKED) {
            char d[MAX_PATH];
            GetWindowTextA(w->dir, d, sizeof d);
            if (browse_folder(h, d, sizeof d)) SetWindowTextA(w->dir, d);
            return 0;
        }
        if ((id == ID_BACK && code == BN_CLICKED) && w->page > PG_WELCOME) { show_page(w, w->page - 1); return 0; }
        if ((id == ID_NEXT && code == BN_CLICKED) || id == IDOK) {
            if (!IsWindowEnabled(w->next)) return 0;
            if (w->page == PG_DEST) {
                char d[MAX_PATH];
                GetWindowTextA(w->dir, d, sizeof d);
                if (!valid_dir(d)) {
                    pc_msgbox(h, TITLE, "Please enter a complete folder path, for example\nC:\\Program Files\\PhotoCraft Classic",
                              (LPCSTR)IDI_EXCLAMATION, "OK", NULL, NULL);
                    return 0;
                }
            }
            if (w->page == PG_READY) { do_install(w); return 0; }
            if (w->page == PG_DONE) {
                if (IsWindowVisible(w->launch) && pc_ctl_get_check(w->launch)) {
                    char dir[MAX_PATH];
                    if (!reg_get_str(HKEY_LOCAL_MACHINE, REG_APP, "InstallDir", dir, sizeof dir)) pc_strlcpy(dir, w->opt.dir, sizeof dir);
                    else { size_t l = strlen(dir); if (l > 3 && dir[l - 1] == '\\') dir[l - 1] = 0; }
                    launch_app(dir);
                }
                w->result = (int)w->install_rc;
                w->done = 1;
                return 0;
            }
            show_page(w, w->page + 1);
            return 0;
        }
        if ((id == ID_CANCEL && code == BN_CLICKED) || id == IDCANCEL) {
            if (w->page == PG_DONE) { w->result = (int)w->install_rc; w->done = 1; return 0; }
            if (IsWindowEnabled(w->cancel) && confirm_cancel(w)) { w->result = 1602; w->done = 1; }
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        if (w->installing) return 0;
        if (w->page == PG_DONE) { w->result = (int)w->install_rc; w->done = 1; }
        else if (confirm_cancel(w)) { w->result = 1602; w->done = 1; }
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

static int run_wizard(const char *dir, int shortcut)
{
    wizard_t *w = (wizard_t *)calloc(1, sizeof *w);
    HWND h;
    char s[600];
    int rc;
    if (!w) return 1;
    w->opt.shortcut = shortcut;
    h = w->h = pc_chrome_create(WIZ_CLASS, TITLE, CW, CH, NULL, w);
    if (!h) { free(w); return 1; }

    /* Welcome */
    w->ctl[PG_WELCOME][0] = pc_ctl_label_bold(h, -1, "Welcome to PhotoCraft Classic Setup", PX, 12, PW, 16);
    pc_snprintf(s, sizeof s, "This program will install PhotoCraft Classic %s on your computer.\n\n"
                "It is strongly recommended that you exit all Windows programs before running this Setup program.\n\n"
                "Click Cancel to quit Setup, or click Next to continue.", PC_VERSION);
    w->ctl[PG_WELCOME][1] = pc_ctl_label(h, -1, s, PX, 40, PW, 110);
    pc_snprintf(s, sizeof s, "Detected: %s\nWindows Installer: %s", sys.name,
                sys.msi_major ? (msi_ok() ? "installed" : "too old (will be updated)") : "not installed (will be installed)");
    if (sys.msi_major) {
        char v[96];
        pc_snprintf(v, sizeof v, "Detected: %s\nWindows Installer: version %d.%d%s", sys.name, sys.msi_major, sys.msi_minor,
                    msi_ok() ? "" : " (too old, will be updated)");
        pc_strlcpy(s, v, sizeof s);
    }
    w->ctl[PG_WELCOME][2] = pc_ctl_label(h, -1, s, PX, 200, PW, 30);

    /* License */
    w->ctl[PG_LICENSE][0] = pc_ctl_label_bold(h, -1, "License Agreement", PX, 12, PW, 16);
    w->ctl[PG_LICENSE][1] = pc_ctl_label(h, -1, "Please read the following license agreement. Use the scroll bar to see the rest of it.", PX, 32, PW, 28);
    w->ctl[PG_LICENSE][2] = pc_ctl_edit(h, ID_LICENSE, pc_license_text, PX, 62, PW, 170,
                                        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL);
    w->ctl[PG_LICENSE][3] = w->accept = pc_ctl_check(h, ID_ACCEPT, "I &accept the terms of the license agreement", PX, 240, PW, 18, 0);

    /* Destination */
    w->ctl[PG_DEST][0] = pc_ctl_label_bold(h, -1, "Choose Destination Location", PX, 12, PW, 16);
    w->ctl[PG_DEST][1] = pc_ctl_label(h, -1, "Setup will install PhotoCraft Classic in the following folder.\n\n"
                                      "To install to this folder, click Next.\n\nTo install to a different folder, "
                                      "click Browse and select another folder, or type its name.", PX, 40, PW, 90);
    w->ctl[PG_DEST][2] = pc_ctl_label(h, -1, "Destination Folder", PX, 150, PW, 14);
    w->ctl[PG_DEST][3] = w->dir = pc_ctl_edit(h, ID_DIR, dir, PX, 168, PW - 87, 22, ES_AUTOHSCROLL);
    w->ctl[PG_DEST][4] = pc_ctl_button(h, ID_BROWSE, "B&rowse...", PX + PW - 81, 167, 81, 23, 0);

    /* Options */
    w->ctl[PG_OPTIONS][0] = pc_ctl_label_bold(h, -1, "Select Options", PX, 12, PW, 16);
    w->ctl[PG_OPTIONS][1] = pc_ctl_label(h, -1, "Setup will add a \"PhotoCraft Classic\" folder to the Programs menu of the "
                                         "Start menu, with shortcuts to PhotoCraft Classic and to Check for Updates.\n\n"
                                         "PhotoCraft Classic is added to the \"Open With\" list of common image files; "
                                         "your current default programs are not changed.", PX, 40, PW, 90);
    w->ctl[PG_OPTIONS][2] = w->shortcut = pc_ctl_check(h, ID_SHORTCUT, "Create a &desktop shortcut", PX, 140, PW, 18, shortcut);

    /* Ready */
    w->ctl[PG_READY][0] = pc_ctl_label_bold(h, -1, "Ready to Install", PX, 12, PW, 16);
    w->ctl[PG_READY][1] = pc_ctl_label(h, -1, "Setup has enough information to start copying the program files. "
                                       "Click Install to begin, or Back to change any settings.", PX, 32, PW, 28);
    w->ctl[PG_READY][2] = w->summary = pc_ctl_label(h, -1, "", PX + 8, 72, PW - 8, 170);

    /* Installing */
    w->ctl[PG_INSTALL][0] = pc_ctl_label_bold(h, -1, "Installing", PX, 12, PW, 16);
    w->ctl[PG_INSTALL][1] = w->status = pc_ctl_label(h, ID_STATUS, "Preparing...", PX, 40, PW, 30);
    w->ctl[PG_INSTALL][2] = w->bar = pc_ctl_progress(h, ID_BAR, PX, 76, PW, 18);

    /* Done */
    w->ctl[PG_DONE][0] = pc_ctl_label_bold(h, -1, "Setup Complete", PX, 12, PW, 16);
    w->ctl[PG_DONE][1] = w->info = pc_ctl_label(h, -1, "", PX, 40, PW, 60);
    w->ctl[PG_DONE][2] = w->launch = pc_ctl_check(h, ID_LAUNCH, "&Launch PhotoCraft Classic now", PX, 110, PW, 18, 1);

    pc_ctl_separator(h, 12, CH - 12 - 23 - 11, CW - 24);
    w->back = pc_ctl_button(h, ID_BACK, "< &Back", CW - 12 - 75 - 10 - 75 - 75, CH - 12 - 23, 75, 23, 0);
    w->next = pc_ctl_button(h, ID_NEXT, "&Next >", CW - 12 - 75 - 10 - 75, CH - 12 - 23, 75, 23, 1);
    w->cancel = pc_ctl_button(h, ID_CANCEL, "Cancel", CW - 12 - 75, CH - 12 - 23, 75, 23, 0);

    show_page(w, PG_WELCOME);
    pc_chrome_modal(h, NULL, &w->done);
    DestroyWindow(h);
    rc = w->result;
    free(w);
    return rc;
}

/* ================================================================== */
/* entry                                                               */

static DWORD ensure_or_restart_silent(options_t *o)
{
    DWORD rc = install(o);
    if (rc == 3010 && !msi_ok()) {
        /* Windows Installer installed; RunOnce resumes Setup after the
         * restart.  Silent mode never restarts Windows by itself. */
        return 3010;
    }
    return rc;
}

/* Case-insensitive match of the token at p (ends at blank or NUL). */
static int tok_is(const char *p, const char *t)
{
    while (*t && pc_tolower((unsigned char)*p) == pc_tolower((unsigned char)*t)) { p++; t++; }
    return !*t && (*p == 0 || *p == ' ' || *p == '\t');
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char dir[MAX_PATH], *p, *d;
    int silent = 0, shortcut = 1, resume = 0, rc;
    (void)prev; (void)show;

    GetModuleFileNameA(NULL, setup_dir, sizeof setup_dir);
    p = strrchr(setup_dir, '\\');
    if (p) *p = 0;
    detect_os();
    detect_msi();
    default_dir(dir, sizeof dir);

    /* /S /NOSHORTCUT /resume /D=<dir> (last) */
    for (p = cmdline ? cmdline : (char *)""; *p; ) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (p[0] == '"' && (p[1] == '/' || p[1] == '-') && (p[2] == 'D' || p[2] == 'd') && p[3] == '=') {
            /* "/D=C:\Some Dir" (whole switch quoted) */
            size_t l;
            pc_strlcpy(dir, p + 4, sizeof dir);
            if (strchr(dir, '"')) *strchr(dir, '"') = 0;
            l = strlen(dir);
            while (l > 3 && (dir[l - 1] == ' ' || dir[l - 1] == '\\')) dir[--l] = 0;
            break;
        }
        if ((p[0] == '/' || p[0] == '-') && (p[1] == 'D' || p[1] == 'd') && p[2] == '=') {
            size_t l;
            d = p + 3;
            if (*d == '"') { d++; pc_strlcpy(dir, d, sizeof dir); if (strchr(dir, '"')) *strchr(dir, '"') = 0; }
            else pc_strlcpy(dir, d, sizeof dir);
            l = strlen(dir);
            while (l > 3 && (dir[l - 1] == ' ' || dir[l - 1] == '\\')) dir[--l] = 0;
            break;
        }
        if (tok_is(p, "/S")) silent = 1;
        else if (tok_is(p, "/NOSHORTCUT")) shortcut = 0;
        else if (tok_is(p, "/resume")) resume = 1;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    (void)resume;

    if (sys.platform == VER_PLATFORM_WIN32s) {
        if (!silent) MessageBoxA(NULL, "PhotoCraft Classic requires Windows 95, Windows NT 4.0 or later.", TITLE, MB_ICONSTOP);
        return 1;
    }
    pc_chrome_init(hinst);
    pc_chrome_register_class(WIZ_CLASS, wiz_proc);
    pc_chrome_register_class(DIR_CLASS, dir_proc);

    if (silent) {
        options_t o;
        memset(&o, 0, sizeof o);
        pc_strlcpy(o.dir, dir, sizeof o.dir);
        o.shortcut = shortcut;
        o.silent = 1;
        if (!valid_dir(o.dir)) return 87;   /* ERROR_INVALID_PARAMETER */
        rc = (int)ensure_or_restart_silent(&o);
        return rc;
    }
    rc = run_wizard(dir, shortcut);
    return rc;
}
