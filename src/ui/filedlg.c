/*
 * Open and Save As dialogs in the Windows 95 common-dialog layout, drawn
 * by the application itself (the system dialog would be the Windows 10
 * Explorer dialog).  Look in / Save in drop-down with drives, Up One Level,
 * a file list with folder and image icons, File name, Files of type, and
 * Photoshop 5.0's File Size readout.
 */
#include "dlg.h"

typedef struct { char name[260]; int dir; DWORD size; } FEntry;

typedef struct {
    char dir[260];
    FEntry *e;
    int n, cap;
    int save;
    int fmt;        /* type filter / save format; 0 = all supported */
    int layered;
    char result[260];
} FileCtx;

static const char *const k_open_types[] = {
    "All Formats", "Photoshop (*.PSD)", "BMP (*.BMP)", "CompuServe GIF (*.GIF)", "JPEG (*.JPG;*.JPE)",
    "PNG (*.PNG)", "TIFF (*.TIF)", "Targa (*.TGA;*.VDA;*.ICB;*.VST)", "All Files (*.*)"
};

static int ext_matches(const char *name, int filter)
{
    int f = pc_format_from_ext(name);
    if (filter == 0) return f != PC_FMT_UNKNOWN;
    if (filter == 8) return 1;
    return f == filter;
}

static void add_entry(FileCtx *c, const char *name, int dir, DWORD size)
{
    if (c->n >= c->cap) {
        int nc = c->cap ? c->cap * 2 : 128;
        FEntry *ne = (FEntry *)realloc(c->e, sizeof(FEntry) * (size_t)nc);
        if (!ne) return;
        c->e = ne; c->cap = nc;
    }
    pc_strlcpy(c->e[c->n].name, name, sizeof c->e[c->n].name);
    c->e[c->n].dir = dir;
    c->e[c->n].size = size;
    c->n++;
}

static int cmp_entry(const void *a, const void *b)
{
    const FEntry *x = (const FEntry *)a, *y = (const FEntry *)b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return pc_stricmp(x->name, y->name);
}

static void scan(Dlg *d)
{
    FileCtx *c = (FileCtx *)d->user;
    char pat[280];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    HWND lb = dlg_item(d, 110);
    UINT old = SetErrorMode(SEM_FAILCRITICALERRORS);
    int i;
    c->n = 0;
    pc_snprintf(pat, sizeof pat, "%s*.*", c->dir);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            int dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
            if (!dir && !ext_matches(fd.cFileName, c->save ? c->fmt : c->fmt)) continue;
            add_entry(c, fd.cFileName, dir, fd.nFileSizeLow);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    SetErrorMode(old);
    if (c->n > 1) qsort(c->e, (size_t)c->n, sizeof(FEntry), cmp_entry);
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < c->n; i++) SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)i);
    /* "Look in" combo: the drives plus the current path's folders. */
    {
        HWND cb = dlg_item(d, 100);
        DWORD drives = GetLogicalDrives();
        int k, cur = 0;
        char item[280];
        SendMessageA(cb, CB_RESETCONTENT, 0, 0);
        for (k = 0; k < 26; k++) {
            if (!(drives & (1u << k))) continue;
            pc_snprintf(item, sizeof item, "%c:\\", 'A' + k);
            SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)item);
            if (pc_toupper((unsigned char)c->dir[0]) == 'A' + k) {
                /* Insert the deeper folders of the current path below its drive. */
                const char *p = c->dir + 3;
                char acc[260];
                pc_strlcpy(acc, c->dir, 4);
                cur = (int)SendMessageA(cb, CB_GETCOUNT, 0, 0) - 1;
                while (*p) {
                    const char *e = strchr(p, '\\');
                    size_t len = e ? (size_t)(e - p) : strlen(p);
                    size_t al = strlen(acc);
                    if (al + len + 2 >= sizeof acc) break;
                    memcpy(acc + al, p, len); acc[al + len] = '\\'; acc[al + len + 1] = 0;
                    SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)acc);
                    cur = (int)SendMessageA(cb, CB_GETCOUNT, 0, 0) - 1;
                    if (!e) break;
                    p = e + 1;
                }
            }
        }
        d->busy++;
        SendMessageA(cb, CB_SETCURSEL, (WPARAM)cur, 0);
        d->busy--;
    }
}

static int icon_for(const FEntry *e)
{
    if (e->dir) return PC_ICON_FOLDER;
    return pc_format_from_ext(e->name) != PC_FMT_UNKNOWN ? PC_ICON_FILE_IMAGE : PC_ICON_FILE;
}

static void file_draw(Dlg *d, const DRAWITEMSTRUCT *di)
{
    FileCtx *c = (FileCtx *)d->user;
    RECT r = di->rcItem, t;
    int sel = (di->itemState & ODS_SELECTED) != 0;
    const FEntry *e;
    if ((int)di->itemID < 0 || (int)di->itemData >= c->n) { w95_fill(di->hDC, &r, W95_WINDOW); return; }
    e = &c->e[di->itemData];
    w95_fill(di->hDC, &r, W95_WINDOW);
    w95_icon(di->hDC, icon_for(e), r.left + 2, r.top);
    t = r; t.left += 22;
    {
        SIZE sz;
        HFONT of = (HFONT)SelectObject(di->hDC, w95_font(W95_FONT_NORMAL));
        GetTextExtentPoint32A(di->hDC, e->name, (int)strlen(e->name), &sz);
        SelectObject(di->hDC, of);
        t.right = PC_MIN(t.left + sz.cx + 4, r.right - 70);
    }
    if (sel) w95_fill(di->hDC, &t, W95_SELBK);
    { RECT tt = t; tt.left += 2; w95_text(di->hDC, W95_FONT_NORMAL, e->name, &tt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, sel ? W95_SELTEXT : W95_TEXT); }
    if (!e->dir) {
        char b[32];
        RECT s = r;
        s.right -= 6;
        pc_snprintf(b, sizeof b, "%dKB", (int)((e->size + 1023) / 1024));
        w95_text(di->hDC, W95_FONT_NORMAL, b, &s, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, W95_TEXT);
    }
    if (di->itemState & ODS_FOCUS) w95_focus_rect(di->hDC, &t);
}

static void go_dir(Dlg *d, const char *path)
{
    FileCtx *c = (FileCtx *)d->user;
    size_t l;
    pc_strlcpy(c->dir, path, sizeof c->dir);
    l = strlen(c->dir);
    if (l && c->dir[l - 1] != '\\' && l + 1 < sizeof c->dir) { c->dir[l] = '\\'; c->dir[l + 1] = 0; }
    scan(d);
}

static void go_up(Dlg *d)
{
    FileCtx *c = (FileCtx *)d->user;
    size_t l = strlen(c->dir);
    if (l <= 3) return;
    c->dir[l - 1] = 0;
    {
        char *s = strrchr(c->dir, '\\');
        if (s) s[1] = 0;
    }
    scan(d);
}

static int file_accept(Dlg *d)
{
    FileCtx *c = (FileCtx *)d->user;
    char name[260], full[300];
    DWORD a;
    dlg_get_text(d, 120, name, sizeof name);
    if (!name[0]) return 0;
    if (name[1] == ':' || (name[0] == '\\' && name[1] == '\\')) pc_strlcpy(full, name, sizeof full);
    else pc_snprintf(full, sizeof full, "%s%s", c->dir, name);
    a = GetFileAttributesA(full);
    if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY)) { go_dir(d, full); dlg_set_text(d, 120, ""); return 0; }
    if (strchr(name, '*') || strchr(name, '?')) return 0;
    if (c->save) {
        /* Add the extension of the chosen format. */
        if (pc_format_from_ext(full) != c->fmt) { pc_strlcat(full, ".", sizeof full); pc_strlcat(full, pc_format_ext(c->fmt), sizeof full); }
        if (plat_file_exists(full)) {
            char q[400];
            pc_snprintf(q, sizeof q, "%s already exists.\r\nDo you want to replace it?", full);
            if (dlg_message("Save As", q, MB_ICON_WARN_, 2) != 1) return 0;
        }
    } else if (!plat_file_exists(full)) {
        char q[400];
        pc_snprintf(q, sizeof q, "%s\r\nFile not found.\r\nPlease verify the correct file name was given.", full);
        dlg_message("Open", q, MB_ICON_WARN_, 0);
        return 0;
    }
    pc_strlcpy(c->result, full, sizeof c->result);
    return 1;
}

static void file_change(Dlg *d, int id, int code)
{
    FileCtx *c = (FileCtx *)d->user;
    if (id == 100 && code == DLG_CODE_CHANGE) {
        char p[280];
        int k = dlg_get_combo(d, 100);
        if (k >= 0) { SendMessageA(dlg_item(d, 100), CB_GETLBTEXT, (WPARAM)k, (LPARAM)p); go_dir(d, p); }
        return;
    }
    if (id == 101 && code == DLG_CODE_CLICK) { go_up(d); return; }
    if (id == 102 && code == DLG_CODE_CLICK) {
        char name[64] = "New Folder", full[320];
        if (dlg_text_input("New Folder", "Name:", name, sizeof name)) {
            pc_snprintf(full, sizeof full, "%s%s", c->dir, name);
            if (!CreateDirectoryA(full, NULL)) app_error("Could not create the folder \"%s\".", name);
            scan(d);
        }
        return;
    }
    if (id == 110 && (code == DLG_CODE_CHANGE || code == DLG_CODE_DBLCLK)) {
        int k = (int)SendMessageA(dlg_item(d, 110), LB_GETCURSEL, 0, 0);
        if (k < 0 || k >= c->n) return;
        if (c->e[k].dir) {
            if (code == DLG_CODE_DBLCLK) { char p[300]; pc_snprintf(p, sizeof p, "%s%s", c->dir, c->e[k].name); go_dir(d, p); }
            return;
        }
        dlg_set_text(d, 120, c->e[k].name);
        if (!c->save) { char b[64]; pc_snprintf(b, sizeof b, "File Size: %dK", (int)((c->e[k].size + 1023) / 1024)); dlg_set_text(d, 140, b); }
        if (code == DLG_CODE_DBLCLK && file_accept(d)) dlg_end(d, IDOK);
        return;
    }
    if (id == 130 && code == DLG_CODE_CHANGE) {
        int k = dlg_get_combo(d, 130);
        if (c->save) {
            c->fmt = k + 1;
            {
                /* Swap the extension in the file name field. */
                char name[260], *dot;
                dlg_get_text(d, 120, name, sizeof name);
                dot = strrchr(name, '.');
                if (dot) *dot = 0;
                pc_strlcat(name, ".", sizeof name);
                pc_strlcat(name, pc_format_ext(c->fmt), sizeof name);
                dlg_set_text(d, 120, name);
            }
            if (c->layered && c->fmt != PC_FMT_PSD) dlg_set_text(d, 140, "Note: layers will be flattened in this format.");
            else dlg_set_text(d, 140, "");
        } else c->fmt = k;
        scan(d);
        return;
    }
    if (id == IDOK && code == DLG_CODE_CLICK) {
        /* A refused name ends this round with 0 and the dialog runs again. */
        if (!file_accept(d)) dlg_end(d, 0);
        else dlg_end(d, IDOK);
    }
}

/* Up One Level / Create New Folder: flat toolbar buttons with icons (Win95). */
static int g_tb_down;

static void toolbtn_paint(Dlg *d, int id, HDC dc, const RECT *r)
{
    (void)d;
    w95_draw_icon_button(dc, r, id == 101 ? PC_ICON_UP_FOLDER : PC_ICON_NEW_FOLDER, g_tb_down == id ? 2 : 1, 0);
}

static void toolbtn_mouse(Dlg *d, int id, UINT msg, int x, int y, WPARAM wp)
{
    (void)wp;
    if (msg == WM_LBUTTONDOWN) { g_tb_down = id; dlg_redraw(d, id); }
    else if (msg == WM_LBUTTONUP && g_tb_down == id) {
        g_tb_down = 0;
        dlg_redraw(d, id);
        if (x >= 0 && y >= 0 && x < 24 && y < 22) file_change(d, id, DLG_CODE_CLICK);
    }
}

static int run_file_dialog(char *path, int n, int save, int *fmt, int layered)
{
    Dlg *d;
    FileCtx c;
    int rc, W = 440;
    memset(&c, 0, sizeof c);
    c.save = save; c.layered = layered;
    c.fmt = save ? (fmt && *fmt > 0 ? *fmt : PC_FMT_PSD) : 0;
    /* Start folder: the given path's folder, else the current directory. */
    {
        const char *b = plat_basename(path);
        size_t dl = (size_t)(b - path);
        if (dl > 0 && dl < sizeof c.dir) { memcpy(c.dir, path, dl); c.dir[dl] = 0; }
        else GetCurrentDirectoryA(sizeof c.dir, c.dir);
        if (!c.dir[0]) pc_strlcpy(c.dir, "C:\\", sizeof c.dir);
    }
    d = dlg_create(save ? "Save As" : "Open", W, 290, file_change, &c);
    if (!d) return 0;
    d->draw_item = file_draw;
    dlg_label(d, 10, 15, 60, 16, save ? "Save &in:" : "Look &in:");
    dlg_combo(d, 100, 70, 12, 230, 20, NULL, 0, 0);
    dlg_custom(d, 101, 306, 11, 24, 22, toolbtn_paint, toolbtn_mouse);
    dlg_custom(d, 102, 332, 11, 24, 22, toolbtn_paint, toolbtn_mouse);
    dlg_list_owner(d, 110, 10, 40, W - 20, 158);
    dlg_label(d, 10, 210, 70, 16, "File &name:");
    dlg_edit(d, 120, 90, 207, 240, 20, save ? plat_basename(path) : "", 0);
    dlg_label(d, 10, 238, 80, 16, save ? "Save &As:" : "Files of &type:");
    if (save) {
        const char *items[PC_FMT_COUNT - 1];
        int k;
        for (k = 1; k < PC_FMT_COUNT; k++) items[k - 1] = pc_format_name(k);
        dlg_combo(d, 130, 90, 235, 240, 20, items, PC_FMT_COUNT - 1, c.fmt - 1);
    } else {
        dlg_combo(d, 130, 90, 235, 240, 20, k_open_types, 9, 0);
    }
    dlg_label_id(d, 140, 10, 264, 320, 16, "", 0);
    dlg_button(d, IDOK, W - 86, 206, 75, 23, save ? "&Save" : "&Open", 1);
    dlg_button(d, IDCANCEL, W - 86, 234, 75, 23, "Cancel", 0);
    scan(d);
    for (;;) {
        rc = dlg_run(d);
        if (rc != 0) break;
        d->done = 0;
    }
    if (rc == IDOK && c.result[0]) {
        pc_strlcpy(path, c.result, (size_t)n);
        if (fmt) *fmt = c.save ? c.fmt : pc_format_from_ext(c.result);
    }
    free(c.e);
    dlg_destroy(d);
    return rc == IDOK && c.result[0];
}

int dlg_open_file(char *path, int n) { return run_file_dialog(path, n, 0, NULL, 0); }

int dlg_save_file(char *path, int n, int *fmt, int layered_ok)
{
    return run_file_dialog(path, n, 1, fmt, layered_ok);
}
