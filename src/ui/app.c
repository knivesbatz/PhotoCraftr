/*
 * PhotoCraft Classic - main window, layout, documents, keyboard routing.
 */
#include "app.h"

App g_app;

/* ---------------- accessors ---------------- */

DocView *app_view(void)
{
    if (g_app.active < 0 || g_app.active >= g_app.nviews) return NULL;
    return g_app.views[g_app.active];
}

PcDoc *app_doc(void)
{
    DocView *v = app_view();
    return v ? v->doc : NULL;
}

void app_status(const char *fmt, ...)
{
    /* CS6 has no application status bar; transient messages go to the
     * active document's status area through the title of the options bar. */
    (void)fmt;
}

void app_error(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    pc_vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    dlg_message(APP_NAME, buf, MB_ICON_ERR_, 0);
}

int app_confirm(const char *title, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    pc_vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return dlg_message(title, buf, MB_ICON_WARN_, 2);
}

void app_beep_done(void)
{
    if (g_app.prefs.beep_when_done) MessageBeep(MB_OK);
}

/* ---------------- layout ---------------- */

void app_layout(void)
{
    RECT r;
    int y = 0, x0 = 0, x1, mh = 0, oh = 0, tw = 0, dw = 0;
    int full = g_app.screen_mode != 0;
    if (!g_app.main) return;
    GetClientRect(g_app.main, &r);
    x1 = r.right;
    if (g_app.screen_mode < 2) mh = menubar_height();
    if (!g_app.hide_all && !g_app.options_hidden) oh = options_height();
    if (!g_app.hide_all && !g_app.tools_hidden) tw = toolbox_width();
    if (!g_app.hide_all && !g_app.hide_panels) dw = dock_width();
    (void)full;
    SetWindowPos(g_app.menubar, NULL, 0, 0, r.right, mh, SWP_NOZORDER | (mh ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    y = mh;
    SetWindowPos(g_app.options, NULL, 0, y, r.right, oh, SWP_NOZORDER | (oh ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    y += oh;
    SetWindowPos(g_app.toolbox, NULL, 0, y, tw, r.bottom - y, SWP_NOZORDER | (tw ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    x0 = tw;
    SetWindowPos(g_app.dock, NULL, x1 - dw, y, dw, r.bottom - y, SWP_NOZORDER | (dw ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    x1 -= dw;
    SetWindowPos(g_app.docarea, NULL, x0, y, PC_MAX(1, x1 - x0), PC_MAX(1, r.bottom - y), SWP_NOZORDER);
    docarea_layout();
}

void app_refresh(int what)
{
    DocView *v = app_view();
    if ((what & R_CANVAS) && v) view_invalidate(v, NULL);
    if (what & (R_LAYERS | R_HISTORY | R_CHANNELS | R_PATHS | R_NAV | R_INFO | R_COLOR)) dock_refresh(what);
    if (what & R_TABS) docarea_layout();
    if (what & R_TOOLS) toolbox_refresh();
    if (what & R_OPTIONS) options_refresh();
    if ((what & R_TITLE) && v) {
        char t[300];
        view_title(v, t, sizeof t);
        if (v->frame) SetWindowTextA(v->frame, t);
        docarea_layout();
    }
}

/* ---------------- documents ---------------- */

void app_set_active(int index)
{
    if (index < 0 || index >= g_app.nviews) index = g_app.nviews ? 0 : -1;
    if (g_app.active >= 0 && g_app.active < g_app.nviews && g_app.active != index) {
        DocView *old = g_app.views[g_app.active];
        tool_commit(old);
    }
    g_app.active = index;
    if (index >= 0) {
        DocView *v = g_app.views[index];
        if (v->frame) SetWindowPos(v->frame, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    docarea_layout();
    app_refresh(R_ALL);
}

DocView *app_add_doc(PcDoc *d, const char *path)
{
    DocView *v;
    if (g_app.nviews >= MAX_VIEWS) { app_error("Too many documents are open."); pc_doc_free(d); return NULL; }
    v = (DocView *)calloc(1, sizeof *v);
    if (!v) { pc_doc_free(d); return NULL; }
    v->doc = d;
    v->zoom = 1.0;
    v->show_channel = -1; v->edit_channel = -1;
    v->temp_tool = -1;
    v->show_edges = 1;
    d->hist_limit = g_app.prefs.history_states;
    if (path && *path) {
        pc_strlcpy(d->path, path, sizeof d->path);
        pc_strlcpy(d->title, plat_basename(path), sizeof d->title);
    } else if (!d->title[0]) {
        v->number = ++g_app.next_untitled;
        pc_snprintf(d->title, sizeof d->title, "Untitled-%d", v->number);
    }
    g_app.views[g_app.nviews++] = v;
    pc_set_text_renderer(plat_text_render, NULL);
    view_create(v);
    if (!g_app.prefs.tab_docs) view_float(v, 1);
    app_set_active(g_app.nviews - 1);
    view_zoom_fit(v, 0);
    if (v->zoom > 1.0) view_set_zoom(v, 1.0, -1, -1);
    return v;
}

int app_close_view(int index, int ask)
{
    DocView *v;
    int i;
    if (index < 0 || index >= g_app.nviews) return 0;
    v = g_app.views[index];
    tool_cancel(v);
    if (ask && v->doc->dirty) {
        int r = app_confirm(APP_NAME, "Save changes to the document \"%s\" before closing?", v->doc->title);
        if (r < 0) return 0;
        if (r == 1 && app_save_view(v, 0)) return 0;
    }
    view_destroy(v);
    pc_doc_free(v->doc);
    pc_buf_unref(v->comp);
    free(v->poly);
    if (v->has_saved) pc_state_release(&v->saved);
    free(v);
    for (i = index; i < g_app.nviews - 1; i++) g_app.views[i] = g_app.views[i + 1];
    g_app.nviews--;
    if (g_app.active >= g_app.nviews) g_app.active = g_app.nviews - 1;
    else if (g_app.active > index) g_app.active--;
    app_set_active(g_app.active);
    return 1;
}

const char *plat_basename(const char *path)
{
    const char *a = strrchr(path, '\\'), *b = strrchr(path, '/');
    const char *s = a > b ? a : b;
    return s ? s + 1 : path;
}

void app_add_recent(const char *path)
{
    int i, j;
    for (i = 0; i < g_app.nrecent; i++) if (!pc_stricmp(g_app.recent[i], path)) break;
    if (i == g_app.nrecent && g_app.nrecent < MAX_RECENT) g_app.nrecent++;
    if (i >= MAX_RECENT) i = MAX_RECENT - 1;
    for (j = PC_MIN(i, g_app.nrecent - 1); j > 0; j--) pc_strlcpy(g_app.recent[j], g_app.recent[j - 1], 260);
    pc_strlcpy(g_app.recent[0], path, 260);
    settings_save();
}

int app_open_file(const char *path)
{
    u8 *data = NULL;
    size_t n = 0;
    char err[256] = "";
    PcDoc *d;
    int i;
    for (i = 0; i < g_app.nviews; i++)
        if (!pc_stricmp(g_app.views[i]->doc->path, path)) { app_set_active(i); return 0; }
    if (plat_read_file(path, &data, &n, err, sizeof err)) { app_error("Could not open \"%s\" because %s", path, err); return -1; }
    {
        HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
        d = pc_load_mem(data, n, path, err, sizeof err);
        SetCursor(old);
    }
    free(data);
    if (!d) { app_error("Could not open \"%s\": %s", plat_basename(path), err); return -1; }
    if (err[0]) dlg_message(APP_NAME, err, MB_ICON_WARN_, 0);   /* loaded with a warning */
    {
        /* Remember the folder for the next Open. */
        const char *base = plat_basename(path);
        size_t dl = (size_t)(base - path);
        if (dl > 0 && dl < sizeof g_app.last_dir) { memcpy(g_app.last_dir, path, dl); g_app.last_dir[dl] = 0; }
    }
    if (!app_add_doc(d, path)) return -1;
    app_add_recent(path);
    return 0;
}

int app_save_view(DocView *v, int save_as)
{
    char path[260], err[256];
    int fmt = v->doc->format;
    PcSaveOpts o;
    u8 *out = NULL;
    size_t n = 0;
    int layered = v->doc->s.nlayers > 1 || v->doc->s.layers[0]->kind == PC_LAYER_TEXT || !v->doc->s.layers[0]->is_background || v->doc->s.nchannels > 0;
    pc_save_opts_default(&o);
    o.jpeg_quality = g_app.prefs.jpeg_quality;
    pc_strlcpy(path, v->doc->path, sizeof path);
    if (fmt == PC_FMT_UNKNOWN) fmt = PC_FMT_PSD;
    /* Plain Save goes straight to disk only when the document's own file and
     * format can hold it; a layered image in a flat format asks for Save As
     * (Photoshop's behaviour), proposing Photoshop format.  The written format
     * must always agree with the file name's extension. */
    if (save_as == 0 && (!path[0] || (layered && !pc_format_can_layers(fmt)) || pc_format_from_ext(path) != fmt)) save_as = 1;
    if (save_as == 1 && layered && !pc_format_can_layers(fmt)) fmt = PC_FMT_PSD;
    if (save_as) {
        if (!path[0]) pc_snprintf(path, sizeof path, "%s%s.%s", g_app.last_dir, v->doc->title, pc_format_ext(fmt));
        else {
            /* Offer the document name with the extension of the proposed format. */
            char *dot = strrchr(path, '.');
            if (dot && dot > strrchr(path, '\\')) *dot = 0;
            pc_strlcat(path, ".", sizeof path);
            pc_strlcat(path, pc_format_ext(fmt), sizeof path);
        }
        if (!dlg_save_file(path, sizeof path, &fmt, layered)) return -1;
        if (!dlg_format_options(fmt, &o)) return -1;
    }
    {
        HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
        int rc = pc_save_mem(v->doc, fmt, &o, &out, &n, err, sizeof err);
        if (!rc) rc = plat_write_file(path, out, n, err, sizeof err);
        SetCursor(old);
        free(out);
        if (rc) { app_error("Could not save \"%s\": %s", plat_basename(path), err); return -1; }
    }
    if (save_as != 2) {
        pc_strlcpy(v->doc->path, path, sizeof v->doc->path);
        pc_strlcpy(v->doc->title, plat_basename(path), sizeof v->doc->title);
        v->doc->format = fmt;
        v->doc->dirty = 0;
    }
    app_add_recent(path);
    app_refresh(R_TITLE | R_TABS);
    return 0;
}

/* ---------------- history / channels ---------------- */

void app_apply_channel_restriction(DocView *v, const PcDocState *before)
{
    /* Editing a single colour channel: keep the other channels as they were. */
    PcLayer *l;
    int li;
    size_t i, n;
    u8 *p;
    if (!v || v->edit_channel < 0 || v->edit_channel > 2 || !before) return;
    li = v->doc->s.active;
    if (li < 0 || li >= v->doc->s.nlayers || li >= before->nlayers) return;
    l = v->doc->s.layers[li];
    if (l->pix == before->layers[li]->pix) return;
    p = pc_layer_pixels_w(l);
    if (!p) return;
    n = (size_t)v->doc->s.w * v->doc->s.h;
    for (i = 0; i < n; i++) {
        int c;
        for (c = 0; c < 4; c++) if (c != v->edit_channel) p[i * 4 + c] = before->layers[li]->pix->px[i * 4 + c];
    }
}

void app_commit(PcDoc *d, const char *label)
{
    DocView *v = app_view();
    if (v && v->doc == d && v->edit_channel >= 0 && v->edit_channel <= 2) {
        const PcDocState *prev = pc_hist_state(d, d->hist_cur);
        app_apply_channel_restriction(v, prev);
    }
    pc_hist_commit(d, label);
    app_refresh(R_CANVAS | R_LAYERS | R_HISTORY | R_CHANNELS | R_PATHS | R_NAV | R_TITLE);
}

/* ---------------- colours / tools ---------------- */

void app_set_colors(u32 fg, u32 bg)
{
    g_app.fg = fg; g_app.bg = bg;
    app_refresh(R_TOOLS | R_COLOR);
}

void app_set_tool(int tool)
{
    DocView *v = app_view();
    if (tool < 0 || tool >= T_COUNT) return;
    if (v) tool_commit(v);
    g_app.tool = tool;
    g_app.group_tool[g_tool_info[tool].group] = tool;
    app_refresh(R_TOOLS | R_OPTIONS);
    if (v) view_update_cursor(v);
}

u32 app_sample_color(DocView *v, int x, int y, int size, int merged)
{
    PcDoc *d = v->doc;
    PcBuf *b;
    int r = size == 2 ? 2 : size == 1 ? 1 : 0, i, j, n = 0;
    unsigned acc[3] = { 0, 0, 0 };
    if (x < 0 || y < 0 || x >= d->s.w || y >= d->s.h) return g_app.fg;
    if (merged) { if (!v->comp || v->comp_rev != d->revision) { pc_buf_unref(v->comp); v->comp = pc_doc_composite(d); v->comp_rev = d->revision; } b = v->comp; }
    else { PcLayer *l = pc_doc_active(d); b = l ? l->pix : NULL; }
    if (!b) return g_app.fg;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++) {
            int xx = PC_CLAMP(x + i, 0, d->s.w - 1), yy = PC_CLAMP(y + j, 0, d->s.h - 1);
            const u8 *p = b->px + ((size_t)yy * d->s.w + xx) * 4;
            /* Sample against white where transparent. */
            acc[0] += (p[0] * p[3] + 255 * (255 - p[3])) / 255;
            acc[1] += (p[1] * p[3] + 255 * (255 - p[3])) / 255;
            acc[2] += (p[2] * p[3] + 255 * (255 - p[3])) / 255;
            n++;
        }
    return pc_rgb((int)(acc[0] / n), (int)(acc[1] / n), (int)(acc[2] / n));
}

/* ---------------- screen modes / panels ---------------- */

void app_toggle_panels(int all)
{
    if (all) { g_app.hide_all = !g_app.hide_all; g_app.hide_panels = 0; }
    else g_app.hide_panels = !g_app.hide_panels;
    app_layout();
}

static RECT g_restore_rect;
static int g_restore_max;

void app_set_screen_mode(int mode)
{
    int old = g_app.screen_mode;
    mode = PC_CLAMP(mode, 0, 2);
    if (mode == old) return;
    if (old == 0) { GetWindowRect(g_app.main, &g_restore_rect); g_restore_max = g_app.fr.maximized; }
    g_app.screen_mode = mode;
    if (mode == 0) {
        g_app.fr.flags &= ~W95_FRAME_NOCAPTION;
        g_app.fr.maximized = g_restore_max;
        SetWindowPos(g_app.main, NULL, g_restore_rect.left, g_restore_rect.top, g_restore_rect.right - g_restore_rect.left,
                     g_restore_rect.bottom - g_restore_rect.top, SWP_NOZORDER | SWP_FRAMECHANGED);
    } else {
        g_app.fr.flags |= W95_FRAME_NOCAPTION;
        g_app.fr.maximized = 1;   /* no border */
        SetWindowPos(g_app.main, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
    }
    app_layout();
    app_refresh(R_ALL);
}

/* ---------------- keyboard ---------------- */

/* GetAncestor is Windows 98+, so walk the parents ourselves. */
static HWND root_of(HWND h)
{
    while (h && (GetWindowLongA(h, GWL_STYLE) & WS_CHILD)) h = GetParent(h);
    return h;
}

static int is_text_control(HWND h)
{
    char cls[32];
    if (!h) return 0;
    GetClassNameA(h, cls, sizeof cls);
    return !pc_stricmp(cls, "Edit") || !pc_stricmp(cls, "ComboBox") || !pc_stricmp(cls, "ListBox");
}

/* Returns 1 when the key was consumed. */
static int app_key(MSG *m)
{
    int vk = (int)m->wParam;
    int ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0, alt = GetKeyState(VK_MENU) < 0;
    DocView *v = app_view();
    HWND focus = GetFocus();
    int cmd;
    if (g_app.modal) return 0;
    if (root_of(m->hwnd) != g_app.main) return 0;
    if (m->message == WM_KEYUP || m->message == WM_SYSKEYUP) {
        if (v && !is_text_control(focus)) return tool_key(v, vk, 0);
        return 0;
    }
    if (is_text_control(focus)) {
        /* Enter/Esc leave a palette field and give the keyboard back to the canvas. */
        if (vk == VK_RETURN || vk == VK_ESCAPE) { SetFocus(g_app.main); return vk == VK_ESCAPE; }
        if (vk == VK_TAB) { SetFocus(g_app.main); return 1; }
        return 0;
    }
    if (m->message == WM_SYSKEYDOWN && !ctrl) {
        if (vk == VK_MENU || vk == VK_F10) return 0;   /* handled on key up as in Windows */
        if (vk >= 'A' && vk <= 'Z' && menubar_key(g_app.main, vk, 1)) return 1;
    }
    if (vk == VK_F10 && !ctrl && !alt) return menubar_key(g_app.main, vk, 0);
    if (v && tool_key(v, vk, 1)) return 1;
    cmd = cmd_from_key(vk, ctrl, shift, alt);
    if (cmd) { cmd_run(cmd); return 1; }
    if (vk == VK_TAB && !ctrl && !alt) { app_toggle_panels(!shift); return 1; }
    if (!ctrl && !alt) {
        int t;
        if (vk == 'D') { cmd_run(CMD_DEFAULT_COLORS); return 1; }
        if (vk == 'X') { cmd_run(CMD_SWAP_COLORS); return 1; }
        if (vk == 'F') { cmd_run(CMD_CYCLE_SCREEN); return 1; }
        if (vk == 'Q') { cmd_run(CMD_QUICK_MASK); return 1; }
        if (vk >= 'A' && vk <= 'Z') {
            /* Tool letters; Shift cycles through the hidden tools of a slot. */
            int cur = g_app.tool;
            if (shift && g_tool_info[cur].key == vk) {
                int g = g_tool_info[cur].group;
                for (t = cur + 1; t < cur + T_COUNT; t++) {
                    int tt = t % T_COUNT;
                    if (g_tool_info[tt].group == g) { app_set_tool(tt); return 1; }
                }
                return 1;
            }
            for (t = 0; t < T_COUNT; t++)
                if (g_tool_info[t].key == vk) {
                    int shown = g_app.group_tool[g_tool_info[t].group];
                    app_set_tool(g_tool_info[shown].key == vk ? shown : t);
                    return 1;
                }
        }
        if (vk >= '0' && vk <= '9' && tool_is_painting(g_app.tool)) {
            /* Number keys set the tool opacity in tens (0 = 100%), as in PS. */
            int pct = vk == '0' ? 100 : (vk - '0') * 10;
            g_app.topt[g_app.tool].opacity = pct;
            app_refresh(R_OPTIONS);
            return 1;
        }
        if (vk == 0xDB || vk == 0xDD) {
            /* [ and ] change brush size. */
            PcBrush *b = &g_app.topt[g_app.tool].brush;
            int step = b->diameter < 10 ? 1 : b->diameter < 50 ? 5 : 10;
            b->diameter = PC_CLAMP(b->diameter + (vk == 0xDD ? step : -step), 1, 999);
            app_refresh(R_OPTIONS);
            if (v) view_invalidate(v, NULL);
            return 1;
        }
    }
    return 0;
}

/* ---------------- main window ---------------- */

static void start_update_check(void)
{
    if (reg_get_int("Update", "CheckOnStartup", 1)) plat_launch_updater("--auto");
}

static LRESULT CALLBACK main_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;
    if (g_app.main && w95_frame_proc(hwnd, &g_app.fr, msg, wp, lp, &res)) {
        if (msg == WM_NCCALCSIZE || msg == WM_SIZE) app_layout();
        return res;
    }
    if (msg == g_app.msg_update_close && msg) {
        /* The updater asks us to close so it can install a new version. */
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        return 1;
    }
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_SIZE:
        app_layout();
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 480; mm->ptMinTrackSize.y = 360;
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(hwnd, &r);
        w95_fill((HDC)wp, &r, W95_FACE);
        return 1;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) { DocView *v = app_view(); if (v) view_update_cursor(v); }
        return 0;
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        UINT i, n = DragQueryFileA(drop, 0xFFFFFFFF, NULL, 0);
        for (i = 0; i < n; i++) {
            char path[260];
            if (DragQueryFileA(drop, i, path, sizeof path)) app_open_file(path);
        }
        DragFinish(drop);
        return 0;
    }
    case WM_SYSKEYUP:
        if ((wp == VK_MENU || wp == VK_F10) && !g_app.modal) { menubar_key(hwnd, (int)wp, 0); return 0; }
        break;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;   /* our own menu bar handles Alt */
        break;
    case WM_TIMER:
        if (wp == 1) {
            int i;
            for (i = 0; i < g_app.nviews; i++) tool_timer(g_app.views[i]);
        } else if (wp == 2) {
            KillTimer(hwnd, 2);
            start_update_check();
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) > 0 && LOWORD(wp) < CMD_COUNT && lp == 0) { cmd_run(LOWORD(wp)); return 0; }
        break;
    case WM_CLOSE: {
        while (g_app.nviews > 0) if (!app_close_view(g_app.nviews - 1, 1)) return 0;
        dock_save_workspace();
        settings_save();
        actions_save();
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void parse_cmdline(const char *cl)
{
    char path[260];
    while (*cl) {
        int n = 0;
        while (*cl == ' ') cl++;
        if (!*cl) break;
        if (*cl == '"') {
            cl++;
            while (*cl && *cl != '"' && n < 259) path[n++] = *cl++;
            if (*cl == '"') cl++;
        } else {
            while (*cl && *cl != ' ' && n < 259) path[n++] = *cl++;
        }
        path[n] = 0;
        if (n && path[0] != '-' && path[0] != '/') app_open_file(path);
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    MSG m;
    RECT wa;
    int w, h;
    (void)prev;
    memset(&g_app, 0, sizeof g_app);
    g_app.inst = inst;
    g_app.active = -1;
    g_app.fg = 0x000000; g_app.bg = 0xFFFFFF;
    w95_init(inst);
    g_app.msg_update_close = RegisterWindowMessageA("PhotoCraftClassic.UpdateClose");
    tool_defaults();
    settings_load();
    g_w95_gradient_captions = g_app.prefs.gradient_captions;

    w95_register_class(APP_FRAME_CLASS, main_proc, CS_DBLCLKS, NULL);
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    w = wa.right - wa.left; h = wa.bottom - wa.top;
    g_app.main = CreateWindowExA(WS_EX_APPWINDOW | WS_EX_ACCEPTFILES, APP_FRAME_CLASS, APP_NAME,
                                 WS_POPUP | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
                                 wa.left, wa.top, w, h, NULL, NULL, inst, NULL);
    if (!g_app.main) return 1;
    w95_frame_attach(g_app.main, &g_app.fr, W95_FRAME_SIZABLE | W95_FRAME_MINMAX | W95_FRAME_CLOSE);
    g_app.fr.min_w = 480; g_app.fr.min_h = 360;
    /* Start maximized like Photoshop does on first launch. */
    w95_frame_maximize(g_app.main, &g_app.fr, 1);

    g_app.menubar = menubar_create(g_app.main);
    g_app.options = options_create(g_app.main);
    g_app.toolbox = toolbox_create(g_app.main);
    g_app.dock = dock_create(g_app.main);
    g_app.docarea = docarea_create(g_app.main);
    dock_load_workspace();
    actions_load();
    app_layout();
    ShowWindow(g_app.main, show == SW_SHOWMINIMIZED ? SW_SHOWMINIMIZED : SW_SHOW);
    UpdateWindow(g_app.main);
    SetTimer(g_app.main, 1, 120, NULL);       /* marching ants, airbrush build-up */
    SetTimer(g_app.main, 2, 3000, NULL);      /* deferred update check */
    parse_cmdline(cmdline);

    while (GetMessageA(&m, NULL, 0, 0)) {
        if (m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN || m.message == WM_KEYUP || m.message == WM_SYSKEYUP) {
            if (app_key(&m)) continue;
        }
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return (int)m.wParam;
}
