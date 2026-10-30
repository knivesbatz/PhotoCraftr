/*
 * Actions palette: record a sequence of commands with their dialog
 * settings and play it back on any document (Photoshop 4/5 Actions).
 * Steps store the command id and the parameter block the command's
 * dialog produced, so playback runs without dialogs unless the step's
 * dialog toggle is on.
 */
#include "app.h"
#include "dlg.h"

#define MAX_SETS 8
#define MAX_ACTIONS 48
#define MAX_STEPS 32
#define MAX_PARAM 2048

typedef struct { int cmd; int n; u8 *data; int enabled; int dialog; } Step;
typedef struct { char name[48]; int set; int expanded; int enabled; Step steps[MAX_STEPS]; int nsteps; } Action;
typedef struct { char name[48]; int expanded; } ActSet;

static ActSet g_sets[MAX_SETS];
static int g_nsets;
static Action g_acts[MAX_ACTIONS];
static int g_nacts;
static int g_sel_kind = 1, g_sel_index = 0;   /* 0 set, 1 action, 2 step (index = action*100+step) */
static int g_recording_action = -1;
static int g_playing;
static const void *g_play_params;
static int g_play_n;

/* ---------------- model ---------------- */

static void free_action(Action *a)
{
    int k;
    for (k = 0; k < a->nsteps; k++) free(a->steps[k].data);
    a->nsteps = 0;
}

static int add_action(int set, const char *name)
{
    Action *a;
    if (g_nacts >= MAX_ACTIONS) return -1;
    a = &g_acts[g_nacts];
    memset(a, 0, sizeof *a);
    pc_strlcpy(a->name, name, sizeof a->name);
    a->set = set; a->enabled = 1;
    return g_nacts++;
}

static void add_step(int ai, int cmd, const void *params, int n)
{
    Action *a;
    Step *s;
    if (ai < 0 || ai >= g_nacts) return;
    a = &g_acts[ai];
    if (a->nsteps >= MAX_STEPS || n < 0 || n > MAX_PARAM) return;
    s = &a->steps[a->nsteps];
    memset(s, 0, sizeof *s);
    s->cmd = cmd; s->enabled = 1; s->n = n;
    if (n) { s->data = (u8 *)malloc((size_t)n); if (!s->data) return; memcpy(s->data, params, (size_t)n); }
    a->nsteps++;
}

static void defaults(void)
{
    int a;
    g_nsets = 1;
    pc_strlcpy(g_sets[0].name, "Default Actions", sizeof g_sets[0].name);
    g_sets[0].expanded = 1;
    g_nacts = 0;
    {
        PcColorBalance cb;
        memset(&cb, 0, sizeof cb);
        cb.v[1][0] = 30; cb.v[1][2] = -25; cb.v[0][0] = 10; cb.preserve_luminosity = 1;
        a = add_action(0, "Sepia Toning (layer)");
        add_step(a, CMD_DESATURATE, NULL, 0);
        add_step(a, CMD_COLOR_BALANCE, &cb, sizeof cb);
    }
    {
        PcFilterParams p;
        pc_filter_defaults(PC_FILTER_UNSHARP_MASK, &p);
        p.amount = 80; p.radius = 0.8f; p.threshold = 2;
        a = add_action(0, "Sharpen for Screen");
        add_step(a, CMD_F_UNSHARP, &p, sizeof p);
    }
    {
        PcFilterParams p;
        pc_filter_defaults(PC_FILTER_GAUSSIAN_BLUR, &p);
        p.radius = 6;
        a = add_action(0, "Soft Focus Copy");
        add_step(a, CMD_DUPLICATE_LAYER, NULL, 0);
        add_step(a, CMD_F_GAUSSIAN, &p, sizeof p);
    }
    {
        PcFilterParams p;
        pc_filter_defaults(PC_FILTER_EMBOSS, &p);
        a = add_action(0, "Emboss Copy");
        add_step(a, CMD_DUPLICATE_LAYER, NULL, 0);
        add_step(a, CMD_F_EMBOSS, &p, sizeof p);
    }
    a = add_action(0, "Flatten and Grayscale");
    add_step(a, CMD_FLATTEN, NULL, 0);
    add_step(a, CMD_MODE_GRAY, NULL, 0);
}

/* ---------------- persistence (HKCU, preserved across upgrades) ---------------- */

static void put(u8 **p, size_t *n, size_t *cap, const void *src, size_t len)
{
    if (*n + len > *cap) {
        size_t nc = (*cap ? *cap * 2 : 4096);
        u8 *np;
        while (nc < *n + len) nc *= 2;
        np = (u8 *)realloc(*p, nc);
        if (!np) return;
        *p = np; *cap = nc;
    }
    memcpy(*p + *n, src, len);
    *n += len;
}

void actions_save(void)
{
    u8 *buf = NULL;
    size_t n = 0, cap = 0;
    int i, k, magic = 0x50434131;   /* "PCA1" */
    put(&buf, &n, &cap, &magic, 4);
    put(&buf, &n, &cap, &g_nsets, 4);
    for (i = 0; i < g_nsets; i++) put(&buf, &n, &cap, &g_sets[i], sizeof g_sets[i]);
    put(&buf, &n, &cap, &g_nacts, 4);
    for (i = 0; i < g_nacts; i++) {
        Action *a = &g_acts[i];
        put(&buf, &n, &cap, a->name, sizeof a->name);
        put(&buf, &n, &cap, &a->set, 4);
        put(&buf, &n, &cap, &a->enabled, 4);
        put(&buf, &n, &cap, &a->nsteps, 4);
        for (k = 0; k < a->nsteps; k++) {
            Step *s = &a->steps[k];
            put(&buf, &n, &cap, &s->cmd, 4);
            put(&buf, &n, &cap, &s->enabled, 4);
            put(&buf, &n, &cap, &s->dialog, 4);
            put(&buf, &n, &cap, &s->n, 4);
            if (s->n) put(&buf, &n, &cap, s->data, (size_t)s->n);
        }
    }
    if (buf && n < 512 * 1024) { reg_set_int("Actions", "Size", (int)n); reg_set_bin("Actions", "Data", buf, (int)n); }
    free(buf);
}

static int get(const u8 *b, size_t n, size_t *pos, void *dst, size_t len)
{
    if (*pos + len > n) return -1;
    memcpy(dst, b + *pos, len);
    *pos += len;
    return 0;
}

void actions_load(void)
{
    int size = reg_get_int("Actions", "Size", 0), i, k, magic = 0, ns = 0, na = 0;
    u8 *b;
    size_t pos = 0;
    if (size <= 0 || size > 512 * 1024) { defaults(); return; }
    b = (u8 *)malloc((size_t)size);
    if (!b || !reg_get_bin("Actions", "Data", b, size)) { free(b); defaults(); return; }
    if (get(b, (size_t)size, &pos, &magic, 4) || magic != 0x50434131 || get(b, (size_t)size, &pos, &ns, 4) || ns < 1 || ns > MAX_SETS) goto bad;
    for (i = 0; i < ns; i++) if (get(b, (size_t)size, &pos, &g_sets[i], sizeof g_sets[i])) goto bad;
    for (i = 0; i < ns; i++) g_sets[i].name[sizeof g_sets[i].name - 1] = 0;
    g_nsets = ns;
    if (get(b, (size_t)size, &pos, &na, 4) || na < 0 || na > MAX_ACTIONS) goto bad;
    g_nacts = 0;
    for (i = 0; i < na; i++) {
        Action *a = &g_acts[g_nacts];
        int nsteps = 0;
        memset(a, 0, sizeof *a);
        if (get(b, (size_t)size, &pos, a->name, sizeof a->name) || get(b, (size_t)size, &pos, &a->set, 4) ||
            get(b, (size_t)size, &pos, &a->enabled, 4) || get(b, (size_t)size, &pos, &nsteps, 4)) goto bad;
        a->name[sizeof a->name - 1] = 0;
        if (a->set < 0 || a->set >= g_nsets || nsteps < 0 || nsteps > MAX_STEPS) goto bad;
        g_nacts++;
        for (k = 0; k < nsteps; k++) {
            Step *s = &a->steps[k];
            memset(s, 0, sizeof *s);
            if (get(b, (size_t)size, &pos, &s->cmd, 4) || get(b, (size_t)size, &pos, &s->enabled, 4) ||
                get(b, (size_t)size, &pos, &s->dialog, 4) || get(b, (size_t)size, &pos, &s->n, 4)) goto bad;
            if (s->cmd <= 0 || s->cmd >= CMD_COUNT || s->n < 0 || s->n > MAX_PARAM) goto bad;
            if (s->n) {
                s->data = (u8 *)malloc((size_t)s->n);
                if (!s->data || get(b, (size_t)size, &pos, s->data, (size_t)s->n)) goto bad;
            }
            a->nsteps++;
        }
    }
    free(b);
    return;
bad:
    free(b);
    for (i = 0; i < g_nacts; i++) free_action(&g_acts[i]);
    defaults();
}

/* ---------------- record / play ---------------- */

void actions_record(int cmd, const void *params, int nbytes)
{
    if (g_playing || g_recording_action < 0 || !g_app.recording) return;
    add_step(g_recording_action, cmd, params, nbytes);
    dock_refresh(R_ALL);
}

void actions_begin_record(void)
{
    char name[48];
    int set = 0, a;
    pc_snprintf(name, sizeof name, "Action %d", g_nacts + 1);
    if (!dlg_action_name(name, sizeof name)) return;
    if (g_sel_kind == 0) set = g_sel_index;
    else if (g_sel_kind == 1 && g_sel_index < g_nacts) set = g_acts[g_sel_index].set;
    a = add_action(set, name);
    if (a < 0) { app_error("There are too many actions."); return; }
    g_acts[a].expanded = 1;
    g_sets[set].expanded = 1;
    g_sel_kind = 1; g_sel_index = a;
    g_recording_action = a;
    g_app.recording = 1;
    dock_refresh(R_ALL);
}

void actions_stop(void)
{
    g_app.recording = 0;
    g_recording_action = -1;
    actions_save();
    dock_refresh(R_ALL);
}

int actions_is_playing(void) { return g_playing; }

const void *actions_params(int *nbytes)
{
    if (!g_playing || !g_play_params) return NULL;
    *nbytes = g_play_n;
    return g_play_params;
}

static void play_action(int ai, int from_step)
{
    Action *a = &g_acts[ai];
    int k;
    if (!app_doc()) {
        /* Most steps need an open document. */
        int needs = 0;
        for (k = 0; k < a->nsteps; k++) if (a->steps[k].cmd != CMD_NEW && a->steps[k].cmd != CMD_OPEN) needs = 1;
        if (needs) { app_error("The action \"%s\" needs an open document.", a->name); return; }
    }
    g_playing = 1;
    for (k = from_step; k < a->nsteps; k++) {
        Step *s = &a->steps[k];
        if (!s->enabled) continue;
        g_play_params = s->dialog ? NULL : s->data;
        g_play_n = s->n;
        if (!s->n) g_play_params = NULL;
        cmd_run(s->cmd);
        UpdateWindow(g_app.main);
    }
    g_play_params = NULL;
    g_playing = 0;
}

void actions_play_selected(void)
{
    if (g_app.recording) return;
    if (g_sel_kind == 1 && g_sel_index < g_nacts) play_action(g_sel_index, 0);
    else if (g_sel_kind == 2) { int ai = g_sel_index / 100, st = g_sel_index % 100; if (ai < g_nacts) play_action(ai, st); }
    else if (g_sel_kind == 0) {
        int i;
        for (i = 0; i < g_nacts; i++) if (g_acts[i].set == g_sel_index && g_acts[i].enabled) play_action(i, 0);
    }
}

/* ---------------- palette ---------------- */

#define ROW 18
typedef struct { int kind, index; } RowRef;

static int build_rows(RowRef *rows, int max)
{
    int n = 0, s, a, k;
    for (s = 0; s < g_nsets && n < max; s++) {
        rows[n].kind = 0; rows[n].index = s; n++;
        if (!g_sets[s].expanded) continue;
        for (a = 0; a < g_nacts && n < max; a++) {
            if (g_acts[a].set != s) continue;
            rows[n].kind = 1; rows[n].index = a; n++;
            if (!g_acts[a].expanded) continue;
            for (k = 0; k < g_acts[a].nsteps && n < max; k++) { rows[n].kind = 2; rows[n].index = a * 100 + k; n++; }
        }
    }
    return n;
}

int actions_panel_rows(void) { RowRef r[512]; return build_rows(r, 512); }

void actions_panel_paint(HDC dc, const RECT *r, int scroll)
{
    RowRef rows[512];
    int n = build_rows(rows, 512), i;
    RECT area, bar;
    static const int icons[6] = { PC_ICON_ACTION_STOP, PC_ICON_ACTION_RECORD, PC_ICON_ACTION_PLAY, PC_ICON_ACTION_NEW_SET, PC_ICON_ACTION_NEW, PC_ICON_TRASH };
    SetRect(&area, r->left, r->top, r->right, r->bottom - 20);
    w95_fill(dc, &area, W95_WINDOW);
    for (i = 0; i < n; i++) {
        int y = area.top + (i - scroll) * ROW, sel, indent, enabled, dialog = 0, has_dialog = 0;
        RECT ck, dg, nm;
        char label[96];
        if (y < area.top) continue;
        if (y + ROW > area.bottom) break;
        sel = rows[i].kind == g_sel_kind && rows[i].index == g_sel_index;
        SetRect(&ck, area.left, y, area.left + 18, y + ROW - 1);
        SetRect(&dg, ck.right, y, ck.right + 18, y + ROW - 1);
        w95_fill(dc, &ck, W95_FACE); w95_thin_raised(dc, &ck);
        w95_fill(dc, &dg, W95_FACE); w95_thin_raised(dc, &dg);
        if (rows[i].kind == 0) { pc_strlcpy(label, g_sets[rows[i].index].name, sizeof label); indent = 0; enabled = 1; }
        else if (rows[i].kind == 1) {
            Action *a = &g_acts[rows[i].index];
            int k;
            pc_strlcpy(label, a->name, sizeof label); indent = 14; enabled = a->enabled;
            for (k = 0; k < a->nsteps; k++) if (strstr(g_cmd_info[a->steps[k].cmd].label, "...")) { has_dialog = 1; if (a->steps[k].dialog) dialog = 1; }
        } else {
            Step *s = &g_acts[rows[i].index / 100].steps[rows[i].index % 100];
            int j, o = 0;
            for (j = 0; g_cmd_info[s->cmd].label[j] && o < 90; j++) if (g_cmd_info[s->cmd].label[j] != '&' && g_cmd_info[s->cmd].label[j] != '.') label[o++] = g_cmd_info[s->cmd].label[j];
            label[o] = 0;
            if (s->cmd >= CMD_ADJ_BRIGHTNESS_CONTRAST && s->cmd <= CMD_ADJ_POSTERIZE)
                pc_snprintf(label, sizeof label, "Make %s Layer", pc_adjust_name(s->cmd - CMD_ADJ_BRIGHTNESS_CONTRAST));
            indent = 28; enabled = s->enabled; dialog = s->dialog; has_dialog = strstr(g_cmd_info[s->cmd].label, "...") != NULL;
        }
        if (enabled) w95_icon(dc, PC_ICON_CHECK, ck.left + 4, y + 4);
        if (has_dialog && dialog) w95_icon(dc, PC_ICON_DIALOG_TOGGLE, dg.left + 3, y + 4);
        SetRect(&nm, dg.right, y, area.right, y + ROW - 1);
        w95_fill(dc, &nm, sel ? W95_SELBK : W95_WINDOW);
        if (rows[i].kind < 2) {
            int open = rows[i].kind == 0 ? g_sets[rows[i].index].expanded : g_acts[rows[i].index].expanded;
            w95_draw_arrow(dc, nm.left + indent + 6, y + 8, open ? 1 : 3, 4, sel ? W95_SELTEXT : W95_TEXT);
            if (rows[i].kind == 0) w95_icon(dc, PC_ICON_ACTION_NEW_SET, nm.left + indent + 12, y + 1);
        }
        nm.left += indent + (rows[i].kind == 0 ? 32 : 14);
        w95_text(dc, W95_FONT_NORMAL, label, &nm, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, sel ? W95_SELTEXT : W95_TEXT);
        w95_hline(dc, area.left, area.right, y + ROW - 1, W95_LIGHT);
    }
    SetRect(&bar, r->left, r->bottom - 20, r->right, r->bottom);
    w95_fill(dc, &bar, W95_FACE);
    w95_hline(dc, bar.left, bar.right, bar.top, W95_SHADOW);
    w95_hline(dc, bar.left, bar.right, bar.top + 1, W95_HILIGHT);
    {
        int k, x = r->right - 4 - 6 * 22;
        for (k = 0; k < 6; k++) {
            RECT b;
            int state = (k == 1 && g_app.recording) ? 3 : 0;
            SetRect(&b, x + k * 22, bar.top + 3, x + k * 22 + 21, bar.bottom - 1);
            w95_draw_icon_button(dc, &b, icons[k], state, (k == 0 && !g_app.recording) || (k == 2 && g_app.recording));
        }
    }
}

void actions_panel_mouse(HWND hwnd, const RECT *r, UINT msg, int x, int y, int scroll)
{
    RowRef rows[512];
    int n = build_rows(rows, 512), i;
    if (msg != WM_LBUTTONDOWN && msg != WM_LBUTTONDBLCLK) return;
    if (y >= r->bottom - 20) return;
    i = (y - r->top) / ROW + scroll;
    if (i < 0 || i >= n) return;
    if (x < 18) {
        if (rows[i].kind == 1) g_acts[rows[i].index].enabled = !g_acts[rows[i].index].enabled;
        else if (rows[i].kind == 2) { Step *s = &g_acts[rows[i].index / 100].steps[rows[i].index % 100]; s->enabled = !s->enabled; }
    } else if (x < 36) {
        if (rows[i].kind == 2) { Step *s = &g_acts[rows[i].index / 100].steps[rows[i].index % 100]; if (strstr(g_cmd_info[s->cmd].label, "...")) s->dialog = !s->dialog; }
        else if (rows[i].kind == 1) {
            Action *a = &g_acts[rows[i].index];
            int k, on = 0;
            for (k = 0; k < a->nsteps; k++) if (a->steps[k].dialog) on = 1;
            for (k = 0; k < a->nsteps; k++) if (strstr(g_cmd_info[a->steps[k].cmd].label, "...")) a->steps[k].dialog = !on;
        }
    } else {
        int indent = rows[i].kind == 0 ? 0 : rows[i].kind == 1 ? 14 : 28;
        g_sel_kind = rows[i].kind; g_sel_index = rows[i].index;
        if (rows[i].kind < 2 && x < 36 + indent + 12) {
            if (rows[i].kind == 0) g_sets[rows[i].index].expanded = !g_sets[rows[i].index].expanded;
            else g_acts[rows[i].index].expanded = !g_acts[rows[i].index].expanded;
        } else if (msg == WM_LBUTTONDBLCLK && rows[i].kind < 2) {
            char name[48];
            pc_strlcpy(name, rows[i].kind == 0 ? g_sets[rows[i].index].name : g_acts[rows[i].index].name, sizeof name);
            if (dlg_text_input(rows[i].kind == 0 ? "Set Options" : "Action Options", "Name:", name, sizeof name))
                pc_strlcpy(rows[i].kind == 0 ? g_sets[rows[i].index].name : g_acts[rows[i].index].name, name, 48);
        }
    }
    actions_save();
    InvalidateRect(hwnd, NULL, FALSE);
}

static void delete_selected(void)
{
    if (g_sel_kind == 1 && g_sel_index < g_nacts) {
        free_action(&g_acts[g_sel_index]);
        memmove(&g_acts[g_sel_index], &g_acts[g_sel_index + 1], sizeof(Action) * (g_nacts - g_sel_index - 1));
        g_nacts--;
        if (g_sel_index >= g_nacts) g_sel_index = g_nacts - 1;
    } else if (g_sel_kind == 2) {
        int ai = g_sel_index / 100, st = g_sel_index % 100;
        if (ai < g_nacts && st < g_acts[ai].nsteps) {
            free(g_acts[ai].steps[st].data);
            memmove(&g_acts[ai].steps[st], &g_acts[ai].steps[st + 1], sizeof(Step) * (g_acts[ai].nsteps - st - 1));
            g_acts[ai].nsteps--;
            g_sel_kind = 1; g_sel_index = ai;
        }
    } else if (g_sel_kind == 0 && g_nsets > 1) {
        int s = g_sel_index, i;
        for (i = g_nacts - 1; i >= 0; i--) if (g_acts[i].set == s) { free_action(&g_acts[i]); memmove(&g_acts[i], &g_acts[i + 1], sizeof(Action) * (g_nacts - i - 1)); g_nacts--; }
        for (i = 0; i < g_nacts; i++) if (g_acts[i].set > s) g_acts[i].set--;
        memmove(&g_sets[s], &g_sets[s + 1], sizeof(ActSet) * (g_nsets - s - 1));
        g_nsets--;
        g_sel_index = 0;
    }
    actions_save();
}

void actions_panel_buttons(int which)
{
    switch (which) {
    case 0: if (g_app.recording) actions_stop(); break;
    case 1: if (!g_app.recording) actions_begin_record(); break;
    case 2: actions_play_selected(); break;
    case 3: {
        char name[48];
        pc_snprintf(name, sizeof name, "Set %d", g_nsets + 1);
        if (g_nsets < MAX_SETS && dlg_text_input("New Set", "Name:", name, sizeof name)) {
            memset(&g_sets[g_nsets], 0, sizeof g_sets[0]);
            pc_strlcpy(g_sets[g_nsets].name, name, sizeof g_sets[0].name);
            g_sets[g_nsets].expanded = 1;
            g_sel_kind = 0; g_sel_index = g_nsets;
            g_nsets++;
            actions_save();
        }
        break;
    }
    case 4: actions_begin_record(); break;
    case 5: if (app_confirm("Actions", "Delete the selection?") == 1) delete_selected(); break;
    }
    dock_refresh(R_ALL);
}

void actions_panel_menu(HWND hwnd, int sx, int sy)
{
    enum { M_NEW = CMD_COUNT + 1, M_SET, M_DEL, M_PLAY, M_REC, M_STOP, M_RESET };
    static const MenuItem items[] = {
        { 1, M_NEW, "New Action..." }, { 1, M_SET, "New Set..." }, { 1, M_DEL, "Delete" }, { 1, M_PLAY, "Play" },
        { 1, 0, "-" }, { 1, M_REC, "Start Recording" }, { 1, M_STOP, "Stop Recording" }, { 1, 0, "-" },
        { 1, M_RESET, "Reset Actions" }
    };
    int c = menu_popup_items(items, (int)(sizeof items / sizeof items[0]), sx, sy, hwnd);
    if (c == M_NEW) actions_panel_buttons(4);
    else if (c == M_SET) actions_panel_buttons(3);
    else if (c == M_DEL) actions_panel_buttons(5);
    else if (c == M_PLAY) actions_panel_buttons(2);
    else if (c == M_REC) actions_panel_buttons(1);
    else if (c == M_STOP) actions_panel_buttons(0);
    else if (c == M_RESET) {
        if (app_confirm("Actions", "Replace the current actions with the default actions?") == 1) {
            int i;
            for (i = 0; i < g_nacts; i++) free_action(&g_acts[i]);
            defaults();
            actions_save();
            dock_refresh(R_ALL);
        }
    }
}
