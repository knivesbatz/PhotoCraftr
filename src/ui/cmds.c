/*
 * Command handlers: every menu item and shortcut runs through cmd_run().
 * Commands that change the document record a history state and, while
 * the Actions panel is recording, an action step with their parameters.
 */
#include "app.h"
#include "dlg.h"

#define X(id, label, sc, vk, mods) { label, sc, vk, mods },
const CmdInfo g_cmd_info[CMD_COUNT] = { PC_COMMANDS(X) };
#undef X

/* ---------------- helpers ---------------- */

int brush_tool_for_stroke(void);

static int g_last_filter = -1;
static PcFilterParams g_last_filter_params;
static PcBuf *g_last_sel;           /* for Reselect */
static int g_last_sel_w, g_last_sel_h;

static int play_params(void *p, int n)
{
    int nb;
    const void *s = actions_params(&nb);
    if (s && nb == n) { memcpy(p, s, (size_t)n); return 1; }
    return 0;
}

#define REC(cmd, p) actions_record(cmd, &(p), (int)sizeof(p))
#define REC0(cmd) actions_record(cmd, NULL, 0)

static PcDoc *need_doc(void)
{
    PcDoc *d = app_doc();
    if (!d) MessageBeep(MB_OK);
    return d;
}

static PcLayer *need_layer(PcDoc *d)
{
    PcLayer *l = d ? pc_doc_active(d) : NULL;
    if (!l) MessageBeep(MB_OK);
    return l;
}

static int layer_editable(PcDoc *d, const char *what)
{
    PcLayer *l = pc_doc_active(d);
    if (!l) return 0;
    if (l->kind == PC_LAYER_ADJUST) {
        app_error("Could not complete the %s command because the active layer is an adjustment layer. Select a pixel layer, or paint on the adjustment layer's mask.", what);
        return 0;
    }
    if (l->kind == PC_LAYER_TEXT) {
        if (app_confirm(APP_NAME, "This type layer must be rasterized before proceeding. Its text will no longer be editable. Rasterize the type?") != 1) return 0;
        l->kind = PC_LAYER_PIXEL;
    }
    if (l->lock_pixels) { app_error("Could not complete the %s command because the layer is locked.", what); return 0; }
    return 1;
}

static void remember_selection(PcDoc *d)
{
    if (!d->s.sel) return;
    pc_buf_unref(g_last_sel);
    g_last_sel = pc_buf_ref(d->s.sel);
    g_last_sel_w = d->s.w; g_last_sel_h = d->s.h;
}

static void commit(PcDoc *d, const char *label) { app_commit(d, label); app_beep_done(); }

static void busy(int on)
{
    static HCURSOR old;
    if (on) old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    else SetCursor(old);
}

static int filter_of(int cmd)
{
    switch (cmd) {
    case CMD_F_BLUR: return PC_FILTER_BLUR; case CMD_F_BLUR_MORE: return PC_FILTER_BLUR_MORE;
    case CMD_F_GAUSSIAN: return PC_FILTER_GAUSSIAN_BLUR; case CMD_F_MOTION: return PC_FILTER_MOTION_BLUR;
    case CMD_F_RADIAL: return PC_FILTER_RADIAL_BLUR; case CMD_F_SHARPEN: return PC_FILTER_SHARPEN;
    case CMD_F_SHARPEN_MORE: return PC_FILTER_SHARPEN_MORE; case CMD_F_SHARPEN_EDGES: return PC_FILTER_SHARPEN_EDGES;
    case CMD_F_UNSHARP: return PC_FILTER_UNSHARP_MASK; case CMD_F_NOISE: return PC_FILTER_ADD_NOISE;
    case CMD_F_DESPECKLE: return PC_FILTER_DESPECKLE; case CMD_F_DUST: return PC_FILTER_DUST_SCRATCHES;
    case CMD_F_MEDIAN: return PC_FILTER_MEDIAN; case CMD_F_EMBOSS: return PC_FILTER_EMBOSS;
    case CMD_F_FIND_EDGES: return PC_FILTER_FIND_EDGES; case CMD_F_SOLARIZE: return PC_FILTER_SOLARIZE;
    case CMD_F_DIFFUSE: return PC_FILTER_DIFFUSE; case CMD_F_WIND: return PC_FILTER_WIND;
    case CMD_F_TRACE: return PC_FILTER_TRACE_CONTOUR; case CMD_F_MOSAIC: return PC_FILTER_MOSAIC;
    case CMD_F_TWIRL: return PC_FILTER_TWIRL; case CMD_F_PINCH: return PC_FILTER_PINCH;
    case CMD_F_SPHERIZE: return PC_FILTER_SPHERIZE; case CMD_F_RIPPLE: return PC_FILTER_RIPPLE;
    case CMD_F_CLOUDS: return PC_FILTER_CLOUDS; case CMD_F_DIFF_CLOUDS: return PC_FILTER_DIFF_CLOUDS;
    case CMD_F_OFFSET: return PC_FILTER_OFFSET; case CMD_F_HIGH_PASS: return PC_FILTER_HIGH_PASS;
    case CMD_F_MAXIMUM: return PC_FILTER_MAXIMUM; case CMD_F_MINIMUM: return PC_FILTER_MINIMUM;
    case CMD_F_CUSTOM: return PC_FILTER_CUSTOM;
    }
    return -1;
}

static void run_filter(int cmd, int filter, int ask)
{
    PcDoc *d = need_doc();
    PcFilterParams p;
    int has_dialog = strstr(g_cmd_info[cmd].label, "...") != NULL;
    if (!d || !layer_editable(d, pc_filter_name(filter))) return;
    pc_filter_defaults(filter, &p);
    if (filter == g_last_filter) p = g_last_filter_params;
    p.fg = g_app.fg; p.bg = g_app.bg;
    if (filter == PC_FILTER_CLOUDS || filter == PC_FILTER_DIFF_CLOUDS || filter == PC_FILTER_ADD_NOISE || filter == PC_FILTER_DIFFUSE || filter == PC_FILTER_WIND)
        p.seed = GetTickCount() | 1;
    if (!play_params(&p, sizeof p) && has_dialog && ask) {
        if (!dlg_filter(d, filter, &p)) return;
    }
    busy(1);
    if (!pc_filter_apply(d, filter, &p)) {
        g_last_filter = filter; g_last_filter_params = p;
        commit(d, pc_filter_name(filter));
        REC(cmd, p);
    }
    busy(0);
}

/* ---------------- labels / state ---------------- */

int cmd_label(int cmd, char *buf, int n)
{
    PcDoc *d = app_doc();
    if (cmd >= CMD_RECENT_0 && cmd <= CMD_RECENT_7) {
        int i = cmd - CMD_RECENT_0;
        if (i >= g_app.nrecent) { if (i == 0) { pc_strlcpy(buf, "(none)", (size_t)n); return 1; } return 0; }
        pc_snprintf(buf, (size_t)n, "&%d %s", i + 1, g_app.recent[i]);
        return 1;
    }
    if (cmd >= CMD_WIN_DOC_0 && cmd <= CMD_WIN_DOC_8) {
        int i = cmd - CMD_WIN_DOC_0;
        char t[200];
        if (i >= g_app.nviews) return 0;
        view_title(g_app.views[i], t, sizeof t);
        pc_snprintf(buf, (size_t)n, "&%d %s", i + 1, t);
        return 1;
    }
    if (cmd == CMD_UNDO) {
        if (d && d->hist_cur + 1 < d->nhist && !d->hist[d->hist_cur + 1].snapshot && d->hist_cur > 0 && 0) {}
        if (d && d->hist_cur > 0) pc_snprintf(buf, (size_t)n, "&Undo %s", d->hist[d->hist_cur].label);
        else if (d && d->hist_cur + 1 < d->nhist) pc_snprintf(buf, (size_t)n, "&Redo %s", d->hist[d->hist_cur + 1].label);
        else pc_strlcpy(buf, "Can't &Undo", (size_t)n);
        return 1;
    }
    if (cmd == CMD_LAST_FILTER) {
        if (g_last_filter >= 0) pc_snprintf(buf, (size_t)n, "&%s", pc_filter_name(g_last_filter));
        else pc_strlcpy(buf, "&Last Filter", (size_t)n);
        return 1;
    }
    if (cmd == CMD_MASK_TOGGLE) {
        PcLayer *l = d ? pc_doc_active(d) : NULL;
        pc_strlcpy(buf, l && l->mask && !l->mask_enabled ? "En&able" : "Dis&able", (size_t)n);
        return 1;
    }
    pc_strlcpy(buf, g_cmd_info[cmd].label, (size_t)n);
    return 1;
}

void cmd_state(int cmd, int *en, int *ck)
{
    PcDoc *d = app_doc();
    PcLayer *l = d ? pc_doc_active(d) : NULL;
    DocView *v = app_view();
    int sel = d && d->s.sel != NULL;
    *en = 1; *ck = 0;
    switch (cmd) {
    case CMD_NEW: case CMD_OPEN: case CMD_EXIT: case CMD_PREFERENCES: case CMD_HELP: case CMD_SHORTCUTS: case CMD_ABOUT:
    case CMD_CHECK_UPDATES: case CMD_RELEASE_NOTES: case CMD_PAGE_SETUP:
    case CMD_WS_ESSENTIALS: case CMD_WS_RESET: case CMD_WS_SAVE: case CMD_WS_LOAD: case CMD_WS_CLASSIC98:
        return;
    case CMD_RECENT_0: case CMD_RECENT_1: case CMD_RECENT_2: case CMD_RECENT_3:
    case CMD_RECENT_4: case CMD_RECENT_5: case CMD_RECENT_6: case CMD_RECENT_7:
        *en = cmd - CMD_RECENT_0 < g_app.nrecent;
        return;
    case CMD_AUTO_UPDATES: *ck = reg_get_int("Update", "CheckOnStartup", 1); return;
    case CMD_PASTE: case CMD_PASTE_INTO:
        *en = d && (g_app.clip.rgba || IsClipboardFormatAvailable(CF_DIB)) && (cmd == CMD_PASTE || sel);
        return;
    case CMD_WIN_ACTIONS: *ck = dock_panel_visible(P_ACTIONS); return;
    case CMD_WIN_ADJUSTMENTS: *ck = dock_panel_visible(P_ADJUSTMENTS); return;
    case CMD_WIN_BRUSHES: *ck = dock_panel_visible(P_BRUSHES); return;
    case CMD_WIN_CHANNELS: *ck = dock_panel_visible(P_CHANNELS); return;
    case CMD_WIN_COLOR: *ck = dock_panel_visible(P_COLOR); return;
    case CMD_WIN_HISTORY: *ck = dock_panel_visible(P_HISTORY); return;
    case CMD_WIN_INFO: *ck = dock_panel_visible(P_INFO); return;
    case CMD_WIN_LAYERS: *ck = dock_panel_visible(P_LAYERS); return;
    case CMD_WIN_NAVIGATOR: *ck = dock_panel_visible(P_NAVIGATOR); return;
    case CMD_WIN_PATHS: *ck = dock_panel_visible(P_PATHS); return;
    case CMD_WIN_SWATCHES: *ck = dock_panel_visible(P_SWATCHES); return;
    case CMD_WIN_OPTIONS: *ck = !g_app.options_hidden; return;
    case CMD_WIN_TOOLS: *ck = !g_app.tools_hidden; return;
    case CMD_SCREEN_STANDARD: *ck = g_app.screen_mode == 0; return;
    case CMD_SCREEN_FULL_MENU: *ck = g_app.screen_mode == 1; return;
    case CMD_SCREEN_FULL: *ck = g_app.screen_mode == 2; return;
    case CMD_QUICK_MASK: *en = d != NULL; *ck = g_app.quickmask; return;
    }
    if (cmd >= CMD_WIN_DOC_0 && cmd <= CMD_WIN_DOC_8) { *ck = cmd - CMD_WIN_DOC_0 == g_app.active; return; }
    if (!d) { *en = 0; return; }
    switch (cmd) {
    case CMD_UNDO: *en = d->hist_cur > 0 || d->hist_cur + 1 < d->nhist; break;
    case CMD_STEP_FORWARD: *en = pc_hist_can_redo(d); break;
    case CMD_STEP_BACKWARD: *en = pc_hist_can_undo(d); break;
    case CMD_REVERT: *en = d->path[0] && d->dirty; break;
    case CMD_CUT: case CMD_CLEAR: case CMD_COPY: case CMD_COPY_MERGED: *en = 1; break;
    case CMD_STROKE: case CMD_FEATHER: case CMD_SEL_BORDER: case CMD_SEL_SMOOTH: case CMD_SEL_EXPAND: case CMD_SEL_CONTRACT:
    case CMD_GROW: case CMD_SIMILAR: case CMD_SAVE_SELECTION: case CMD_CROP: case CMD_DEFINE_PATTERN: case CMD_DESELECT: case CMD_SEL_TO_PATH:
        *en = sel; break;
    case CMD_RESELECT: *en = g_last_sel && g_last_sel_w == d->s.w && g_last_sel_h == d->s.h; break;
    case CMD_LOAD_SELECTION: *en = d->s.nchannels > 0; break;
    case CMD_MODE_GRAY: *ck = d->s.mode == PC_MODE_GRAY; break;
    case CMD_MODE_RGB: *ck = d->s.mode == PC_MODE_RGB; break;
    case CMD_LAYER_FROM_BG: *en = l && l->is_background; break;
    case CMD_DELETE_LAYER: *en = d->s.nlayers > 1; break;
    case CMD_MERGE_DOWN: *en = d->s.active > 0 && l && d->s.layers[d->s.active - 1]->visible && d->s.layers[d->s.active - 1]->kind != PC_LAYER_ADJUST; break;
    case CMD_MERGE_VISIBLE: case CMD_FLATTEN: *en = d->s.nlayers > 1 || (l && !l->is_background); break;
    case CMD_LAYER_PROPS: *en = l && !l->is_background; break;
    case CMD_LAYER_STYLE: case CMD_LAYER_STYLE_SHADOW: case CMD_LAYER_STYLE_INNER: case CMD_LAYER_STYLE_GLOW: case CMD_LAYER_STYLE_BEVEL:
    case CMD_FREE_TRANSFORM: case CMD_TRANSFORM_NUMERIC:
    case CMD_LAYER_ROT180: case CMD_LAYER_ROT90CW: case CMD_LAYER_ROT90CCW: case CMD_LAYER_FLIPH: case CMD_LAYER_FLIPV:
        /* (layer effects and transforms need pixels: not for adjustment layers) */
        *en = l && (!l->is_background || (cmd >= CMD_LAYER_ROT180 && cmd <= CMD_LAYER_FLIPV)) && l->kind != PC_LAYER_ADJUST; break;
    case CMD_ADJ_EDIT: *en = l && l->kind == PC_LAYER_ADJUST && l->adj.type != PC_ADJ_INVERT; break;
    case CMD_STYLE_CLEAR: *en = l && l->fx_enabled; break;
    case CMD_MASK_REVEAL_ALL: case CMD_MASK_HIDE_ALL: *en = l && !l->is_background && !l->mask; break;
    case CMD_MASK_REVEAL_SEL: case CMD_MASK_HIDE_SEL: *en = l && !l->is_background && !l->mask && sel; break;
    case CMD_MASK_APPLY: *en = l && l->mask && l->kind != PC_LAYER_ADJUST; break;
    case CMD_MASK_DELETE: case CMD_MASK_TOGGLE: *en = l && l->mask; break;
    case CMD_CLIP_GROUP: *en = l && d->s.active > 0 && !l->clip && !l->is_background; break;
    case CMD_CLIP_UNGROUP: *en = l && l->clip; break;
    case CMD_BRING_FRONT: case CMD_BRING_FORWARD: *en = l && !l->is_background && d->s.active < d->s.nlayers - 1; break;
    case CMD_SEND_BACK: case CMD_SEND_BACKWARD: *en = l && !l->is_background && d->s.active > (d->s.layers[0]->is_background ? 1 : 0); break;
    case CMD_TYPE_EDIT: case CMD_TYPE_RASTERIZE: case CMD_TYPE_HORIZONTAL: case CMD_TYPE_VERTICAL:
        *en = l && l->kind == PC_LAYER_TEXT;
        if (cmd == CMD_TYPE_HORIZONTAL) *ck = *en && !l->text.vertical;
        if (cmd == CMD_TYPE_VERTICAL) *ck = *en && l->text.vertical;
        break;
    case CMD_TYPE_AA_NONE: case CMD_TYPE_AA_CRISP: case CMD_TYPE_AA_STRONG: case CMD_TYPE_AA_SMOOTH:
        *en = l && l->kind == PC_LAYER_TEXT;
        *ck = *en && l->text.antialias == cmd - CMD_TYPE_AA_NONE;
        break;
    case CMD_LAST_FILTER: *en = g_last_filter >= 0; break;
    case CMD_SHOW_EDGES: *ck = v && v->show_edges; break;
    case CMD_SHOW_GRID: *ck = v && v->show_grid; break;
    case CMD_SHOW_RULERS: *ck = v && v->show_rulers; break;
    case CMD_FLOAT_WINDOW: *en = v && !v->frame; break;
    case CMD_PATH_FILL: case CMD_PATH_STROKE: case CMD_PATH_TO_SEL: case CMD_PATH_SAVE: case CMD_PATH_DELETE:
        *en = d->s.active_path >= 0 && d->s.active_path < d->s.npaths && d->s.paths[d->s.active_path].n > 0; break;
    case CMD_CHANNEL_DELETE: case CMD_CHANNEL_DUPLICATE: *en = v && v->edit_channel >= 100; break;
    case CMD_HISTORY_DELETE: *en = d->hist_cur > 0; break;
    case CMD_PURGE_CLIPBOARD: *en = g_app.clip.rgba != NULL; break;
    case CMD_COMMIT: case CMD_CANCEL: *en = v && (v->crop_active || v->xf_active); break;
    }
}

int cmd_from_key(int vk, int ctrl, int shift, int alt)
{
    int c, mods = (ctrl ? MOD_C : 0) | (shift ? MOD_S : 0) | (alt ? MOD_A : 0);
    if (vk == VK_ADD && ctrl) vk = 0xBB;
    if (vk == VK_SUBTRACT && ctrl) vk = 0xBD;
    /* Ctrl+Alt+Z would otherwise be eaten by Ctrl+Z, and so on: exact modifier match. */
    for (c = 1; c < CMD_COUNT; c++) if (g_cmd_info[c].vk && g_cmd_info[c].vk == vk && g_cmd_info[c].mods == mods) return c;
    if (vk == VK_BACK && !ctrl && !shift) return alt ? 0 : 0;
    if (vk == VK_BACK && alt) return 0;
    return 0;
}

/* ---------------- handlers ---------------- */

static void cmd_new(void)
{
    int w = 640, h = 480, mode = PC_MODE_RGB, fill = 0;
    double dpi = 72;
    char name[64] = "";
    PcClip c;
    PcDoc *d;
    /* Photoshop pre-fills New with the clipboard's size. */
    memset(&c, 0, sizeof c);
    if (g_app.clip.rgba) { w = g_app.clip.w; h = g_app.clip.h; }
    else if (plat_clipboard_get(&c) == 0) { w = c.w; h = c.h; pc_clip_free(&c); }
    if (!dlg_new(&w, &h, &dpi, &mode, &fill, name, sizeof name)) return;
    d = pc_doc_new(w, h, mode, dpi, fill, g_app.bg);
    if (!d) { app_error("Could not create a new document because there is not enough memory (RAM)."); return; }
    if (name[0]) pc_strlcpy(d->title, name, sizeof d->title);
    app_add_doc(d, NULL);
}

static void cmd_open(void)
{
    char path[260];
    pc_strlcpy(path, g_app.last_dir, sizeof path);
    if (dlg_open_file(path, sizeof path)) app_open_file(path);
}

static void copy_to_clip(PcDoc *d, int merged)
{
    PcClip c;
    memset(&c, 0, sizeof c);
    if (pc_copy(d, merged, &c)) return;
    pc_clip_free(&g_app.clip);
    g_app.clip = c;
    if (g_app.prefs.export_clipboard) plat_clipboard_put(&c);
}

static void do_paste(PcDoc *d, int into)
{
    PcClip c;
    memset(&c, 0, sizeof c);
    /* Prefer the system clipboard when another program put a newer image there. */
    if (GetClipboardOwner() != g_app.main && plat_clipboard_get(&c) == 0) {
        if (!pc_paste(d, &c, into)) commit(d, into ? "Paste Into" : "Paste");
        pc_clip_free(&c);
        return;
    }
    if (g_app.clip.rgba && !pc_paste(d, &g_app.clip, into)) commit(d, into ? "Paste Into" : "Paste");
}

static void path_to_polys(PcDoc *d, float **xy, u8 **st, int *n)
{
    PcPath *p = &d->s.paths[d->s.active_path];
    int cap = 64 * PC_MAX(1, p->n) + 64;
    *xy = (float *)malloc(sizeof(float) * 2 * (size_t)cap);
    *st = (u8 *)malloc((size_t)cap);
    *n = (*xy && *st) ? pc_path_flatten(p, *xy, *st, cap) : 0;
}

/* Trace the selection outline into a path (marching squares on the 50% level,
 * then simplified): Paths > Make Work Path. */
static void selection_to_path(PcDoc *d)
{
    int w = d->s.w, h = d->s.h, x, y, k, total = 0;
    u8 *vis;
    PcPath *p;
    const u8 *s;
    if (!d->s.sel) return;
    s = d->s.sel->px;
    vis = (u8 *)calloc((size_t)(w + 1) * (h + 1), 1);
    p = pc_path_work(d);
    if (!vis || !p) { free(vis); return; }
    pc_path_free_pts(p);
#define SEL_IN(px, py) ((px) >= 0 && (py) >= 0 && (px) < w && (py) < h && s[(size_t)(py) * w + (px)] >= 128)
    for (y = 0; y < h && total < 20000; y++)
        for (x = 0; x < w && total < 20000; x++) {
            /* Start at a top edge of an inside pixel whose upper neighbour is outside. */
            int cx, cy, dir, steps = 0, n = 0, cap = 1024;
            float *pts;
            if (!SEL_IN(x, y) || SEL_IN(x, y - 1) || vis[(size_t)y * (w + 1) + x]) continue;
            pts = (float *)malloc(sizeof(float) * 2 * (size_t)cap);
            if (!pts) break;
            cx = x; cy = y; dir = 0;   /* 0 right, 1 down, 2 left, 3 up along pixel corners */
            do {
                if (n >= cap) { float *np = (float *)realloc(pts, sizeof(float) * 4 * (size_t)cap); if (!np) break; pts = np; cap *= 2; }
                pts[n * 2] = (float)cx; pts[n * 2 + 1] = (float)cy; n++;
                if (dir == 0) vis[(size_t)cy * (w + 1) + cx] = 1;
                /* Turn rules keeping the inside on the right. */
                {
                    int a, b;
                    static const int ddx[4] = { 1, 0, -1, 0 }, ddy[4] = { 0, 1, 0, -1 };
                    cx += ddx[dir]; cy += ddy[dir];
                    /* pixels ahead-left and ahead-right of the new corner */
                    switch (dir) {
                    case 0: a = SEL_IN(cx, cy - 1); b = SEL_IN(cx, cy); break;
                    case 1: a = SEL_IN(cx, cy); b = SEL_IN(cx - 1, cy); break;
                    case 2: a = SEL_IN(cx - 1, cy); b = SEL_IN(cx - 1, cy - 1); break;
                    default: a = SEL_IN(cx - 1, cy - 1); b = SEL_IN(cx, cy - 1); break;
                    }
                    if (a) dir = (dir + 3) % 4;
                    else if (!b) dir = (dir + 1) % 4;
                }
            } while ((cx != x || cy != y || dir != 0) && ++steps < 400000);
            if (n >= 3) {
                float *out = (float *)malloc(sizeof(float) * 2 * (size_t)n);
                if (out) {
                    int m = 0;
                    /* simplify collinear runs */
                    for (k = 0; k < n; k++) {
                        int pk = (k + n - 1) % n, nk = (k + 1) % n;
                        float ax = pts[pk * 2], ay = pts[pk * 2 + 1], bx = pts[k * 2], by = pts[k * 2 + 1], cx2 = pts[nk * 2], cy2 = pts[nk * 2 + 1];
                        if ((bx - ax) * (cy2 - by) - (by - ay) * (cx2 - bx) != 0) { out[m * 2] = bx; out[m * 2 + 1] = by; m++; }
                    }
                    for (k = 0; k < m; k++) {
                        PcPathPt q;
                        memset(&q, 0, sizeof q);
                        q.x = q.ix = q.ox = out[k * 2]; q.y = q.iy = q.oy = out[k * 2 + 1];
                        q.start = k == 0; q.closed = k == 0;
                        pc_path_add_point(p, &q);
                    }
                    total += m;
                    free(out);
                }
            }
            free(pts);
        }
#undef SEL_IN
    free(vis);
}

static void layer_style_quick(PcDoc *d, int which)
{
    PcLayer *l = need_layer(d);
    if (!l || l->is_background) return;
    l->fx_enabled = 1;
    if (which == 0) l->fx_shadow = 1;
    if (which == 1) l->fx_inner_shadow = 1;
    if (which == 2) l->fx_glow = 1;
    if (which == 3) l->fx_bevel = 1;
    if (dlg_layer_style(d, l)) commit(d, "Layer Style");
    else { pc_hist_goto(d, d->hist_cur); app_refresh(R_ALL); }
}

/* Layer > New Adjustment Layer: the layer goes in at once so the dialog
 * previews it; OK records one history state, Cancel leaves no trace. */
static void new_adjustment_layer(PcDoc *d, int cmd)
{
    int type = cmd - CMD_ADJ_BRIGHTNESS_CONTRAST, played;
    PcAdjust a;
    PcLayer *l;
    char label[64];
    pc_adjust_default(&a, type);
    played = play_params(&a, sizeof a);
    a.type = type;
    l = pc_doc_add_adjustment(d, &a, NULL, d->s.sel != NULL);
    if (!l) { app_error("Could not make a new layer because there are too many layers or not enough memory."); return; }
    if (d->s.sel) { remember_selection(d); pc_sel_clear(d); }   /* the selection became the mask */
    panels_set_mask_target(1);
    app_refresh(R_CANVAS | R_LAYERS | R_NAV);
    if (!played && !dlg_adjust_layer(d, &a)) {
        pc_hist_goto(d, d->hist_cur);
        panels_set_mask_target(0);
        app_refresh(R_ALL);
        return;
    }
    l = pc_doc_active(d);                   /* (previews replace the layer structs) */
    if (l && l->kind == PC_LAYER_ADJUST) l->adj = a;
    pc_snprintf(label, sizeof label, "New %s Layer", pc_adjust_name(type));
    commit(d, label);
    REC(cmd, a);
}

/* Layer Content Options / double-click on the thumbnail: edit the parameters. */
static void edit_adjustment_layer(PcDoc *d)
{
    PcLayer *l = pc_doc_active(d);
    PcAdjust a;
    char label[64];
    if (!l || l->kind != PC_LAYER_ADJUST || l->adj.type == PC_ADJ_INVERT) return;
    a = l->adj;
    if (!dlg_adjust_layer(d, &a)) { app_refresh(R_CANVAS | R_NAV); return; }
    l = pc_doc_active(d);
    if (!l || l->kind != PC_LAYER_ADJUST || !memcmp(&l->adj, &a, sizeof a)) return;
    l->adj = a;
    pc_snprintf(label, sizeof label, "Modify %s Layer", pc_adjust_name(a.type));
    commit(d, label);
}

void cmd_run(int cmd)
{
    PcDoc *d;
    PcLayer *l;
    DocView *v = app_view();
    int f;
    if (cmd <= CMD_NONE || cmd >= CMD_COUNT) return;
    {
        int en, ck;
        cmd_state(cmd, &en, &ck);
        if (!en) { MessageBeep(MB_OK); return; }
    }
    if (v && cmd != CMD_COMMIT && cmd != CMD_CANCEL && cmd != CMD_UNDO && (v->crop_active || v->xf_active) &&
        cmd != CMD_ZOOM_IN && cmd != CMD_ZOOM_OUT && cmd != CMD_FIT_SCREEN && cmd != CMD_ACTUAL_PIXELS)
        tool_commit(v);
    f = filter_of(cmd);
    if (f >= 0) { run_filter(cmd, f, 1); return; }
    switch (cmd) {
    /* ---- File ---- */
    case CMD_NEW: cmd_new(); return;
    case CMD_OPEN: cmd_open(); return;
    case CMD_CLOSE: if (g_app.active >= 0) app_close_view(g_app.active, 1); return;
    case CMD_CLOSE_ALL: while (g_app.nviews && app_close_view(g_app.nviews - 1, 1)) {} return;
    case CMD_SAVE: if (v) app_save_view(v, 0); return;
    case CMD_SAVE_AS: if (v) app_save_view(v, 1); return;
    case CMD_SAVE_COPY: if (v) app_save_view(v, 2); return;
    case CMD_REVERT: {
        char path[260];
        int idx = g_app.active;
        if (!v) return;
        pc_strlcpy(path, v->doc->path, sizeof path);
        v->doc->dirty = 0;
        app_close_view(idx, 0);
        app_open_file(path);
        return;
    }
    case CMD_PAGE_SETUP: dlg_print_setup(v); return;
    case CMD_PRINT: if (v) plat_print(v, g_app.main); return;
    case CMD_EXIT: PostMessageA(g_app.main, WM_CLOSE, 0, 0); return;
    case CMD_RECENT_0: case CMD_RECENT_1: case CMD_RECENT_2: case CMD_RECENT_3:
    case CMD_RECENT_4: case CMD_RECENT_5: case CMD_RECENT_6: case CMD_RECENT_7: {
        char path[260];
        pc_strlcpy(path, g_app.recent[cmd - CMD_RECENT_0], sizeof path);
        app_open_file(path);
        return;
    }
    /* ---- view / window (no document needed) ---- */
    case CMD_PREFERENCES: dlg_preferences(); return;
    case CMD_HELP: case CMD_SHORTCUTS:
        dlg_message(cmd == CMD_HELP ? "PhotoCraft Classic Help" : "Keyboard Shortcuts",
            "Tools: V Move, M Marquee, L Lasso, W Magic Wand, C Crop, I Eyedropper/Measure, B Paintbrush/Airbrush/Pencil, "
            "S Stamp, Y History Brush, E Eraser, G Gradient/Bucket, R Blur/Sharpen/Smudge, O Dodge/Burn/Sponge, "
            "P Pen, T Type, A Path Selection, U Shapes, H Hand, Z Zoom. Shift+letter cycles hidden tools.\r\n\r\n"
            "D default colors, X switch colors, Q Quick Mask, F screen modes, Tab hides all panels, Shift+Tab hides the palettes. "
            "Space = Hand, Ctrl+Space = Zoom in, Alt+Space = Zoom out. [ and ] change the brush size; 1-0 set the opacity.\r\n\r\n"
            "Ctrl+Z undo/redo, Shift+Ctrl+Z step forward, Alt+Ctrl+Z step backward. Ctrl+T Free Transform (Enter applies, Esc cancels). "
            "Shift adds to a selection, Alt subtracts, Shift+Alt intersects. Alt+click with the Rubber Stamp sets the source.",
            MB_ICON_INFO_, 0);
        return;
    case CMD_ABOUT: dlg_about(); return;
    case CMD_RELEASE_NOTES: {
        char exe[260], *sl;
        GetModuleFileNameA(NULL, exe, sizeof exe);
        sl = strrchr(exe, '\\');
        if (sl) {
            u8 *data; size_t n; char err[64];
            pc_strlcpy(sl + 1, "RELEASE-NOTES.txt", sizeof exe - (size_t)(sl + 1 - exe));
            if (!plat_read_file(exe, &data, &n, err, sizeof err)) {
                char *t = (char *)realloc(data, n + 1);
                if (t) { t[n] = 0; if (n > 1800) t[1800] = 0; dlg_message("Release Notes", t, MB_ICON_INFO_, 0); free(t); }
                else free(data);
                return;
            }
        }
        dlg_message("Release Notes", "PhotoCraft Classic " PC_VERSION "\r\n\r\nThe release notes file was not found in the installation folder.", MB_ICON_INFO_, 0);
        return;
    }
    case CMD_CHECK_UPDATES: plat_launch_updater("--check"); return;
    case CMD_AUTO_UPDATES: reg_set_int("Update", "CheckOnStartup", !reg_get_int("Update", "CheckOnStartup", 1)); return;
    case CMD_WS_ESSENTIALS: case CMD_WS_RESET: dock_reset_workspace(); return;
    case CMD_WS_CLASSIC98: dock_classic_workspace(); return;
    case CMD_WS_SAVE: dock_store_named(); dlg_message(APP_NAME, "The current palette arrangement was saved. Use Window > Workspace > Load Saved Workspace to restore it.", MB_ICON_INFO_, 0); return;
    case CMD_WS_LOAD: if (dock_load_named()) app_error("No workspace has been saved yet."); return;
    case CMD_WIN_ACTIONS: dock_toggle_panel(P_ACTIONS); return;
    case CMD_WIN_ADJUSTMENTS: dock_toggle_panel(P_ADJUSTMENTS); return;
    case CMD_WIN_BRUSHES: dock_toggle_panel(P_BRUSHES); return;
    case CMD_WIN_CHANNELS: dock_toggle_panel(P_CHANNELS); return;
    case CMD_WIN_COLOR: dock_toggle_panel(P_COLOR); return;
    case CMD_WIN_HISTORY: dock_toggle_panel(P_HISTORY); return;
    case CMD_WIN_INFO: dock_toggle_panel(P_INFO); return;
    case CMD_WIN_LAYERS: dock_toggle_panel(P_LAYERS); return;
    case CMD_WIN_NAVIGATOR: dock_toggle_panel(P_NAVIGATOR); return;
    case CMD_WIN_PATHS: dock_toggle_panel(P_PATHS); return;
    case CMD_WIN_SWATCHES: dock_toggle_panel(P_SWATCHES); return;
    case CMD_WIN_OPTIONS: g_app.options_hidden = !g_app.options_hidden; app_layout(); return;
    case CMD_WIN_TOOLS: g_app.tools_hidden = !g_app.tools_hidden; app_layout(); return;
    case CMD_SCREEN_STANDARD: app_set_screen_mode(0); return;
    case CMD_SCREEN_FULL_MENU: app_set_screen_mode(1); return;
    case CMD_SCREEN_FULL: app_set_screen_mode(2); return;
    case CMD_CYCLE_SCREEN: app_set_screen_mode((g_app.screen_mode + (GetKeyState(VK_SHIFT) < 0 ? 2 : 1)) % 3); return;
    case CMD_TOGGLE_ALL_PANELS: app_toggle_panels(1); return;
    case CMD_TOGGLE_PANELS: app_toggle_panels(0); return;
    case CMD_DEFAULT_COLORS: app_set_colors(0x000000, 0xFFFFFF); return;
    case CMD_SWAP_COLORS: app_set_colors(g_app.bg, g_app.fg); return;
    case CMD_CASCADE: docarea_cascade(); return;
    case CMD_TILE_H: docarea_tile(0); return;
    case CMD_TILE_V: docarea_tile(1); return;
    case CMD_FLOAT_WINDOW: if (v) view_float(v, 1); return;
    case CMD_FLOAT_ALL: docarea_float_all(1); return;
    case CMD_CONSOLIDATE: docarea_float_all(0); return;
    case CMD_PURGE_CLIPBOARD: pc_clip_free(&g_app.clip); return;
    }
    if (cmd >= CMD_WIN_DOC_0 && cmd <= CMD_WIN_DOC_8) { app_set_active(cmd - CMD_WIN_DOC_0); return; }

    d = need_doc();
    if (!d) return;
    l = pc_doc_active(d);
    switch (cmd) {
    /* ---- Edit ---- */
    case CMD_UNDO:
        if (v) tool_cancel(v);
        /* Ctrl+Z toggles between the last two states, as in Photoshop. */
        if (d->hist_cur > 0) pc_hist_undo(d); else pc_hist_redo(d);
        app_refresh(R_ALL);
        return;
    case CMD_STEP_FORWARD: if (v) tool_cancel(v); pc_hist_redo(d); app_refresh(R_ALL); return;
    case CMD_STEP_BACKWARD: if (v) tool_cancel(v); pc_hist_undo(d); app_refresh(R_ALL); return;
    case CMD_CUT:
        if (!layer_editable(d, "Cut")) return;
        copy_to_clip(d, 0);
        if (!pc_clear(d, g_app.bg)) { commit(d, "Cut Pixels"); REC0(cmd); }
        return;
    case CMD_COPY: copy_to_clip(d, 0); return;
    case CMD_COPY_MERGED: copy_to_clip(d, 1); return;
    case CMD_PASTE: do_paste(d, 0); REC0(cmd); return;
    case CMD_PASTE_INTO: do_paste(d, 1); REC0(cmd); return;
    case CMD_CLEAR: if (layer_editable(d, "Clear") && !pc_clear(d, g_app.bg)) { commit(d, "Clear"); REC0(cmd); } return;
    case CMD_FILL: {
        struct { int use, opacity, blend, preserve; } p = { 0, 100, 0, 0 };
        if (!(pc_layer_is_adjust(l) && l->mask) && !layer_editable(d, "Fill")) return;   /* adjustment: fills the mask */
        if (!play_params(&p, sizeof p) && !dlg_fill(&p.use, &p.opacity, &p.blend, &p.preserve)) return;
        {
            int rc;
            static const u32 fixed[4] = { 0x000000, 0x808080, 0xFFFFFF, 0 };
            if (p.use == 0) rc = pc_fill_color(d, g_app.fg, p.opacity, p.blend, p.preserve);
            else if (p.use == 1) rc = pc_fill_color(d, g_app.bg, p.opacity, p.blend, p.preserve);
            else if (p.use == 2) rc = pc_fill_pattern(d, p.opacity, p.blend);
            else if (p.use == 3) rc = pc_fill_history(d, p.opacity, p.blend);
            else rc = pc_fill_color(d, fixed[PC_CLAMP(p.use - 4, 0, 2)], p.opacity, p.blend, p.preserve);
            if (!rc) { commit(d, "Fill"); REC(cmd, p); }
            else if (p.use == 2) app_error("Could not fill because the pattern is not defined.");
        }
        return;
    }
    case CMD_STROKE: {
        struct { int width, location, opacity, blend; } p = { 1, 1, 100, 0 };
        if (!(pc_layer_is_adjust(l) && l->mask) && !layer_editable(d, "Stroke")) return;
        if (!play_params(&p, sizeof p) && !dlg_stroke(&p.width, &p.location, &p.opacity, &p.blend)) return;
        if (!pc_stroke_selection(d, p.width, p.location, g_app.fg, p.opacity, p.blend)) { commit(d, "Stroke"); REC(cmd, p); }
        return;
    }
    case CMD_FREE_TRANSFORM: if (v) tool_begin_free_transform(v); return;
    case CMD_TRANSFORM_NUMERIC: {
        struct { float dx, dy, sw, sh, angle, skx, sky; } p = { 0, 0, 100, 100, 0, 0, 0 };
        PcRect r;
        if (!play_params(&p, sizeof p) && !dlg_transform_numeric(&p.dx, &p.dy, &p.sw, &p.sh, &p.angle, &p.skx, &p.sky)) return;
        if (!pc_sel_bounds(d, &r) && !pc_layer_bounds(l, d->s.w, d->s.h, &r)) return;
        {
            float cx = (r.x0 + r.x1) / 2.0f, cy = (r.y0 + r.y1) / 2.0f, q[8];
            float hw = (r.x1 - r.x0) / 2.0f * p.sw / 100, hh = (r.y1 - r.y0) / 2.0f * p.sh / 100;
            double a = p.angle * M_PI / 180, ca = cos(a), sa = sin(a);
            float tx = (float)tan(p.skx * M_PI / 180), ty = (float)tan(p.sky * M_PI / 180);
            static const float cs[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };
            int k;
            for (k = 0; k < 4; k++) {
                float x = cs[k * 2] * hw, y = cs[k * 2 + 1] * hh;
                float x2 = x + y * tx, y2 = y + x * ty;
                q[k * 2] = cx + p.dx + (float)(x2 * ca - y2 * sa);
                q[k * 2 + 1] = cy + p.dy + (float)(x2 * sa + y2 * ca);
            }
            if (!pc_layer_transform_quad(d, &r, q, g_app.prefs.interpolation)) { commit(d, "Numeric Transform"); REC(cmd, p); }
        }
        return;
    }
    case CMD_LAYER_ROT180: if (!pc_layer_rotate(d, 2)) { commit(d, "Rotate 180\xB0"); REC0(cmd); } return;
    case CMD_LAYER_ROT90CW: if (!pc_layer_rotate(d, 1)) { commit(d, "Rotate 90\xB0 CW"); REC0(cmd); } return;
    case CMD_LAYER_ROT90CCW: if (!pc_layer_rotate(d, 3)) { commit(d, "Rotate 90\xB0 CCW"); REC0(cmd); } return;
    case CMD_LAYER_FLIPH: if (!pc_layer_flip(d, 0)) { commit(d, "Flip Horizontal"); REC0(cmd); } return;
    case CMD_LAYER_FLIPV: if (!pc_layer_flip(d, 1)) { commit(d, "Flip Vertical"); REC0(cmd); } return;
    case CMD_DEFINE_PATTERN: {
        PcClip c;
        memset(&c, 0, sizeof c);
        if (!pc_copy(d, 0, &c)) { pc_pattern_set(c.rgba, c.w, c.h); pc_clip_free(&c); }
        return;
    }
    case CMD_PURGE_UNDO: case CMD_PURGE_HISTORIES: case CMD_PURGE_ALL:
        if (app_confirm("Purge", "This cannot be undone. Continue?") != 1) return;
        if (cmd == CMD_PURGE_ALL) pc_clip_free(&g_app.clip);
        pc_hist_reset(d, "Purge");
        app_refresh(R_HISTORY);
        return;
    /* ---- Image ---- */
    case CMD_MODE_GRAY:
        if (d->s.mode == PC_MODE_GRAY) return;
        if (d->s.nlayers > 1 && app_confirm(APP_NAME, "Changing modes affects all layers. Discard color information?") != 1) return;
        if (!pc_doc_to_gray(d)) { commit(d, "Grayscale"); REC0(cmd); }
        return;
    case CMD_MODE_RGB: if (!pc_doc_to_rgb(d)) { commit(d, "RGB Color"); REC0(cmd); } return;
    case CMD_LEVELS: {
        PcLevels lv;
        pc_levels_default(&lv);
        if (!layer_editable(d, "Levels")) return;
        if (!play_params(&lv, sizeof lv) && !dlg_levels(d, &lv)) return;
        busy(1);
        if (!pc_adj_levels(d, &lv)) { commit(d, "Levels"); REC(cmd, lv); }
        busy(0);
        return;
    }
    case CMD_AUTO_LEVELS: if (layer_editable(d, "Auto Levels") && !pc_adj_auto_levels(d)) { commit(d, "Auto Levels"); REC0(cmd); } return;
    case CMD_CURVES: {
        PcCurves cv;
        pc_curves_default(&cv);
        if (!layer_editable(d, "Curves")) return;
        if (!play_params(&cv, sizeof cv) && !dlg_curves(d, &cv)) return;
        if (!pc_adj_curves(d, &cv)) { commit(d, "Curves"); REC(cmd, cv); }
        return;
    }
    case CMD_COLOR_BALANCE: {
        PcColorBalance cb;
        memset(&cb, 0, sizeof cb);
        cb.preserve_luminosity = 1;
        if (!layer_editable(d, "Color Balance")) return;
        if (!play_params(&cb, sizeof cb) && !dlg_color_balance(d, &cb)) return;
        if (!pc_adj_color_balance(d, &cb)) { commit(d, "Color Balance"); REC(cmd, cb); }
        return;
    }
    case CMD_BRIGHTNESS_CONTRAST: {
        struct { int b, c; } p = { 0, 0 };
        if (!layer_editable(d, "Brightness/Contrast")) return;
        if (!play_params(&p, sizeof p) && !dlg_brightness_contrast(d, &p.b, &p.c)) return;
        if (!pc_adj_brightness_contrast(d, p.b, p.c)) { commit(d, "Brightness/Contrast"); REC(cmd, p); }
        return;
    }
    case CMD_HUE_SAT: {
        PcHueSat hs;
        memset(&hs, 0, sizeof hs);
        if (!layer_editable(d, "Hue/Saturation")) return;
        if (!play_params(&hs, sizeof hs) && !dlg_hue_sat(d, &hs)) return;
        busy(1);
        if (!pc_adj_hue_sat(d, &hs)) { commit(d, "Hue/Saturation"); REC(cmd, hs); }
        busy(0);
        return;
    }
    case CMD_DESATURATE: if (layer_editable(d, "Desaturate") && !pc_adj_desaturate(d)) { commit(d, "Desaturate"); REC0(cmd); } return;
    case CMD_CHANNEL_MIXER: {
        struct { int m[3][4]; int mono; } p = { { { 100, 0, 0, 0 }, { 0, 100, 0, 0 }, { 0, 0, 100, 0 } }, 0 };
        if (!layer_editable(d, "Channel Mixer")) return;
        if (!play_params(&p, sizeof p) && !dlg_channel_mixer(d, p.m, &p.mono)) return;
        if (!pc_adj_channel_mixer(d, (const int (*)[4])p.m, p.mono)) { commit(d, "Channel Mixer"); REC(cmd, p); }
        return;
    }
    case CMD_INVERT: if (layer_editable(d, "Invert") && !pc_adj_invert(d)) { commit(d, "Invert"); REC0(cmd); } return;
    case CMD_EQUALIZE: if (layer_editable(d, "Equalize") && !pc_adj_equalize(d)) { commit(d, "Equalize"); REC0(cmd); } return;
    case CMD_THRESHOLD: {
        int lv = 128;
        if (!layer_editable(d, "Threshold")) return;
        if (!play_params(&lv, sizeof lv) && !dlg_threshold(d, &lv)) return;
        if (!pc_adj_threshold(d, lv)) { commit(d, "Threshold"); REC(cmd, lv); }
        return;
    }
    case CMD_POSTERIZE: {
        int lv = 4;
        if (!layer_editable(d, "Posterize")) return;
        if (!play_params(&lv, sizeof lv) && !dlg_posterize(d, &lv)) return;
        if (!pc_adj_posterize(d, lv)) { commit(d, "Posterize"); REC(cmd, lv); }
        return;
    }
    case CMD_DUPLICATE: {
        char name[64];
        int merged = 0;
        PcDoc *nd;
        pc_snprintf(name, sizeof name, "%s copy", d->title);
        if (!dlg_duplicate(name, sizeof name, d->s.nlayers > 1, &merged)) return;
        nd = (PcDoc *)calloc(1, sizeof *nd);
        if (!nd) return;
        pc_state_copy(&nd->s, &d->s);
        nd->hist_limit = d->hist_limit;
        if (merged) pc_doc_merge_visible(nd);
        pc_hist_reset(nd, "Duplicate Image");
        pc_strlcpy(nd->title, name, sizeof nd->title);
        nd->dirty = 1;
        app_add_doc(nd, NULL);
        return;
    }
    case CMD_IMAGE_SIZE: {
        struct { int w, h, method; double dpi; } p;
        p.w = d->s.w; p.h = d->s.h; p.method = g_app.prefs.interpolation; p.dpi = d->s.dpi;
        if (!play_params(&p, sizeof p) && !dlg_image_size(d, &p.w, &p.h, &p.dpi, &p.method)) return;
        busy(1);
        if (!pc_image_resize(d, p.w, p.h, p.method, p.dpi)) { commit(d, "Image Size"); REC(cmd, p); if (v) view_zoom_fit(v, 0); }
        else app_error("Could not resize the image because there is not enough memory (RAM).");
        busy(0);
        return;
    }
    case CMD_CANVAS_SIZE: {
        struct { int w, h, anchor; } p;
        p.w = d->s.w; p.h = d->s.h; p.anchor = 4;
        if (!play_params(&p, sizeof p) && !dlg_canvas_size(d, &p.w, &p.h, &p.anchor)) return;
        if (!pc_canvas_resize(d, p.w, p.h, p.anchor, g_app.bg)) { commit(d, "Canvas Size"); REC(cmd, p); if (v) view_zoom_fit(v, 0); }
        return;
    }
    case CMD_ROT180: if (!pc_image_rotate(d, 2)) { commit(d, "Rotate Canvas"); REC0(cmd); } return;
    case CMD_ROT90CW: if (!pc_image_rotate(d, 1)) { commit(d, "Rotate Canvas"); REC0(cmd); if (v) view_zoom_fit(v, 0); } return;
    case CMD_ROT90CCW: if (!pc_image_rotate(d, 3)) { commit(d, "Rotate Canvas"); REC0(cmd); if (v) view_zoom_fit(v, 0); } return;
    case CMD_ROT_ARBITRARY: {
        double deg = 0;
        if (!play_params(&deg, sizeof deg) && !dlg_rotate(&deg)) return;
        busy(1);
        if (!pc_image_rotate_arbitrary(d, deg, g_app.bg)) { commit(d, "Rotate Canvas"); REC(cmd, deg); if (v) view_zoom_fit(v, 0); }
        busy(0);
        return;
    }
    case CMD_FLIP_CANVAS_H: if (!pc_image_flip(d, 0)) { commit(d, "Flip Canvas Horizontal"); REC0(cmd); } return;
    case CMD_FLIP_CANVAS_V: if (!pc_image_flip(d, 1)) { commit(d, "Flip Canvas Vertical"); REC0(cmd); } return;
    case CMD_CROP: {
        PcRect r;
        if (pc_sel_bounds(d, &r) && !pc_image_crop(d, &r)) { commit(d, "Crop"); REC0(cmd); if (v) view_zoom_fit(v, 0); }
        return;
    }
    case CMD_TRIM: if (!pc_image_trim(d, !d->s.layers[0]->is_background)) { commit(d, "Trim"); REC0(cmd); if (v) view_zoom_fit(v, 0); } return;
    case CMD_HISTOGRAM: dlg_histogram(d); return;
    /* ---- Layer ---- */
    case CMD_NEW_LAYER: case CMD_NEW_LAYER_QUICK: {
        char name[64];
        int op = 100, blend = 0, clip = 0;
        PcLayer *nl;
        pc_doc_unique_layer_name(d, name, sizeof name);
        if (cmd == CMD_NEW_LAYER && !dlg_new_layer(name, sizeof name, &op, &blend, &clip)) return;
        nl = pc_doc_add_layer(d, name);
        if (!nl) { app_error("Could not make a new layer because there are too many layers or not enough memory."); return; }
        nl->opacity = op * 255 / 100; nl->blend = blend; nl->clip = clip && d->s.active > 0;
        panels_set_mask_target(0);
        commit(d, "New Layer");
        REC0(CMD_NEW_LAYER_QUICK);
        return;
    }
    case CMD_LAYER_VIA_COPY: case CMD_LAYER_VIA_CUT: {
        PcClip c;
        memset(&c, 0, sizeof c);
        if (pc_copy(d, 0, &c)) return;
        if (cmd == CMD_LAYER_VIA_CUT) { if (!layer_editable(d, "Layer via Cut")) { pc_clip_free(&c); return; } pc_clear(d, g_app.bg); }
        {
            int keep_sel_x = c.x, keep_sel_y = c.y;
            c.x = keep_sel_x; c.y = keep_sel_y;
            if (!pc_paste(d, &c, 0)) { commit(d, cmd == CMD_LAYER_VIA_CUT ? "Layer via Cut" : "Layer via Copy"); REC0(cmd); }
        }
        pc_clip_free(&c);
        return;
    }
    case CMD_LAYER_FROM_BG: {
        char name[64] = "Layer 0";
        int op = 100, blend = 0, clip = 0;
        if (!dlg_layer_props(name, sizeof name, &op, &blend, &clip)) return;
        if (!pc_doc_layer_from_background(d)) {
            l = d->s.layers[0];
            pc_strlcpy(l->name, name, sizeof l->name);
            l->opacity = op * 255 / 100; l->blend = blend;
            commit(d, "Make Layer");
        }
        return;
    }
    case CMD_DUPLICATE_LAYER: if (!pc_doc_duplicate_layer(d, d->s.active)) { commit(d, "Duplicate Layer"); REC0(cmd); } return;
    case CMD_DELETE_LAYER: if (!pc_doc_delete_layer(d, d->s.active)) { panels_set_mask_target(0); commit(d, "Delete Layer"); REC0(cmd); } return;
    case CMD_LAYER_PROPS: {
        char name[64];
        int op, blend, clip;
        if (!l) return;
        pc_strlcpy(name, l->name, sizeof name);
        op = (l->opacity * 100 + 127) / 255; blend = l->blend; clip = l->clip;
        if (!dlg_layer_props(name, sizeof name, &op, &blend, &clip)) return;
        pc_strlcpy(l->name, name, sizeof l->name);
        l->opacity = op * 255 / 100; l->blend = blend; l->clip = clip && d->s.active > 0;
        commit(d, "Layer Properties");
        return;
    }
    case CMD_LAYER_STYLE: if (l && !l->is_background) { if (dlg_layer_style(d, l)) commit(d, "Layer Style"); } return;
    case CMD_LAYER_STYLE_SHADOW: layer_style_quick(d, 0); return;
    case CMD_LAYER_STYLE_INNER: layer_style_quick(d, 1); return;
    case CMD_LAYER_STYLE_GLOW: layer_style_quick(d, 2); return;
    case CMD_LAYER_STYLE_BEVEL: layer_style_quick(d, 3); return;
    case CMD_STYLE_CLEAR: l->fx_enabled = 0; l->fx_shadow = l->fx_glow = l->fx_bevel = l->fx_inner_shadow = 0; commit(d, "Clear Layer Style"); return;
    case CMD_MASK_REVEAL_ALL: case CMD_ADD_MASK_QUICK:
        if (pc_doc_add_mask(d, d->s.active, 1, cmd == CMD_ADD_MASK_QUICK && d->s.sel != NULL) == 0) { panels_set_mask_target(1); commit(d, "Add Layer Mask"); REC0(cmd); }
        return;
    case CMD_MASK_HIDE_ALL: if (!pc_doc_add_mask(d, d->s.active, 0, 0)) { panels_set_mask_target(1); commit(d, "Add Layer Mask"); REC0(cmd); } return;
    case CMD_MASK_REVEAL_SEL: if (!pc_doc_add_mask(d, d->s.active, 1, 1)) { pc_sel_clear(d); panels_set_mask_target(1); commit(d, "Add Layer Mask"); REC0(cmd); } return;
    case CMD_MASK_HIDE_SEL: if (!pc_doc_add_mask(d, d->s.active, 0, 1)) { pc_sel_clear(d); panels_set_mask_target(1); commit(d, "Add Layer Mask"); REC0(cmd); } return;
    case CMD_MASK_APPLY: if (!pc_doc_apply_mask(d, d->s.active, 1)) { panels_set_mask_target(0); commit(d, "Apply Layer Mask"); REC0(cmd); } return;
    case CMD_MASK_DELETE: if (!pc_doc_apply_mask(d, d->s.active, 0)) { panels_set_mask_target(0); commit(d, "Delete Layer Mask"); REC0(cmd); } return;
    case CMD_MASK_TOGGLE: l->mask_enabled = !l->mask_enabled; commit(d, l->mask_enabled ? "Enable Layer Mask" : "Disable Layer Mask"); return;
    case CMD_CLIP_GROUP: l->clip = 1; commit(d, "Group with Previous"); REC0(cmd); return;
    case CMD_CLIP_UNGROUP: l->clip = 0; commit(d, "Ungroup"); REC0(cmd); return;
    case CMD_BRING_FRONT: if (!pc_doc_move_layer(d, d->s.active, d->s.nlayers - 1)) commit(d, "Bring to Front"); return;
    case CMD_BRING_FORWARD: if (!pc_doc_move_layer(d, d->s.active, d->s.active + 1)) commit(d, "Bring Forward"); return;
    case CMD_SEND_BACKWARD: if (!pc_doc_move_layer(d, d->s.active, d->s.active - 1)) commit(d, "Send Backward"); return;
    case CMD_SEND_BACK: if (!pc_doc_move_layer(d, d->s.active, d->s.layers[0]->is_background ? 1 : 0)) commit(d, "Send to Back"); return;
    case CMD_MERGE_DOWN: if (!pc_doc_merge_down(d, d->s.active)) { commit(d, "Merge Down"); REC0(cmd); } return;
    case CMD_MERGE_VISIBLE: if (!pc_doc_merge_visible(d)) { commit(d, "Merge Visible"); REC0(cmd); } return;
    case CMD_FLATTEN: if (!pc_doc_flatten(d)) { panels_set_mask_target(0); commit(d, "Flatten Image"); REC0(cmd); } return;
    case CMD_ADJ_BRIGHTNESS_CONTRAST: case CMD_ADJ_LEVELS: case CMD_ADJ_CURVES: case CMD_ADJ_COLOR_BALANCE: case CMD_ADJ_HUE_SAT:
    case CMD_ADJ_CHANNEL_MIXER: case CMD_ADJ_INVERT: case CMD_ADJ_THRESHOLD: case CMD_ADJ_POSTERIZE:
        new_adjustment_layer(d, cmd);
        return;
    case CMD_ADJ_EDIT: edit_adjustment_layer(d); return;
    /* ---- Type ---- */
    case CMD_TYPE_EDIT: {
        PcText t;
        if (!l || l->kind != PC_LAYER_TEXT) return;
        t = l->text;
        if (dlg_type(d, &t, 0)) { l->text = t; pc_layer_rerender_text(l, d->s.w, d->s.h); commit(d, "Edit Type Layer"); }
        return;
    }
    case CMD_TYPE_RASTERIZE: l->kind = PC_LAYER_PIXEL; commit(d, "Rasterize Type"); REC0(cmd); return;
    case CMD_TYPE_AA_NONE: case CMD_TYPE_AA_CRISP: case CMD_TYPE_AA_STRONG: case CMD_TYPE_AA_SMOOTH:
        l->text.antialias = cmd - CMD_TYPE_AA_NONE;
        pc_layer_rerender_text(l, d->s.w, d->s.h);
        commit(d, "Anti-Alias");
        return;
    case CMD_TYPE_HORIZONTAL: case CMD_TYPE_VERTICAL:
        l->text.vertical = cmd == CMD_TYPE_VERTICAL;
        pc_layer_rerender_text(l, d->s.w, d->s.h);
        commit(d, "Change Text Orientation");
        return;
    /* ---- Select ---- */
    case CMD_SELECT_ALL: pc_sel_all(d); commit(d, "Select All"); REC0(cmd); return;
    case CMD_DESELECT: remember_selection(d); pc_sel_clear(d); commit(d, "Deselect"); REC0(cmd); return;
    case CMD_RESELECT:
        if (g_last_sel) { pc_buf_unref(d->s.sel); d->s.sel = pc_buf_clone(g_last_sel); commit(d, "Reselect"); }
        return;
    case CMD_INVERSE: pc_sel_inverse(d); commit(d, "Inverse"); REC0(cmd); return;
    case CMD_COLOR_RANGE: {
        struct { int fuzz; u32 color; } p = { 40, 0 };
        p.color = g_app.fg;
        if (!play_params(&p, sizeof p) && !dlg_color_range(d, &p.fuzz, &p.color)) return;
        if (!pc_sel_color_range(d, p.color, p.fuzz, PC_SEL_REPLACE)) { commit(d, "Color Range"); REC(cmd, p); }
        return;
    }
    case CMD_FEATHER: {
        float r = 5;
        if (!play_params(&r, sizeof r) && !dlg_feather(&r)) return;
        if (!pc_sel_feather(d, r)) { commit(d, "Feather"); REC(cmd, r); }
        return;
    }
    case CMD_SEL_BORDER: case CMD_SEL_SMOOTH: case CMD_SEL_EXPAND: case CMD_SEL_CONTRACT: {
        static const char *const titles[4] = { "Border Selection", "Smooth Selection", "Expand Selection", "Contract Selection" };
        static const char *const labels[4] = { "Width:", "Sample Radius:", "Expand By:", "Contract By:" };
        static const int how[4] = { 0, 1, 2, 3 };
        int k = cmd - CMD_SEL_BORDER, px = k == 0 ? 10 : k == 1 ? 2 : 1;
        if (!play_params(&px, sizeof px) && !dlg_modify_sel(titles[k], labels[k], &px)) return;
        if (!pc_sel_modify(d, how[k], px)) { commit(d, titles[k] + 0); REC(cmd, px); }
        return;
    }
    case CMD_GROW: if (!pc_sel_grow(d, g_app.topt[T_WAND].tolerance, 0)) { commit(d, "Grow"); REC0(cmd); } return;
    case CMD_SIMILAR: if (!pc_sel_grow(d, g_app.topt[T_WAND].tolerance, 1)) { commit(d, "Similar"); REC0(cmd); } return;
    case CMD_SAVE_SELECTION: {
        char name[32];
        pc_snprintf(name, sizeof name, "Alpha %d", d->s.nchannels + 1);
        if (!dlg_text_input("Save Selection", "Channel name:", name, sizeof name)) return;
        if (!pc_sel_save_channel(d, name)) { commit(d, "Save Selection"); REC0(cmd); }
        else app_error("Could not save the selection: the document has the maximum number of channels.");
        return;
    }
    case CMD_LOAD_SELECTION: {
        int ch = d->s.nchannels - 1;
        if (v && v->edit_channel >= 100) ch = v->edit_channel - 100;
        if (!pc_sel_load_channel(d, ch, 0, PC_SEL_REPLACE)) { commit(d, "Load Selection"); REC0(cmd); }
        return;
    }
    case CMD_QUICK_MASK:
        g_app.quickmask = !g_app.quickmask;
        d->s.quickmask = g_app.quickmask;
        if (g_app.quickmask && !d->s.sel) { PcBuf *s = pc_sel_get(d, 1); (void)s; }
        if (!g_app.quickmask && d->s.sel) {
            size_t i, n = (size_t)d->s.w * d->s.h;
            for (i = 0; i < n; i++) if (d->s.sel->px[i]) break;
            if (i == n) pc_sel_clear(d);
        }
        commit(d, g_app.quickmask ? "Quick Mask" : "Clear Quick Mask");
        app_refresh(R_TOOLS);
        return;
    /* ---- Filter ---- */
    case CMD_LAST_FILTER:
        if (g_last_filter >= 0) {
            if (!layer_editable(d, "Filter")) return;
            busy(1);
            if (!pc_filter_apply(d, g_last_filter, &g_last_filter_params)) commit(d, pc_filter_name(g_last_filter));
            busy(0);
        }
        return;
    /* ---- View ---- */
    case CMD_ZOOM_IN: case CMD_ZOOM_OUT: {
        int i;
        if (!v) return;
        if (cmd == CMD_ZOOM_IN) { for (i = 0; i < g_zoom_nsteps; i++) if (g_zoom_steps[i] > v->zoom + 1e-6) break; }
        else { for (i = g_zoom_nsteps - 1; i >= 0; i--) if (g_zoom_steps[i] < v->zoom - 1e-6) break; }
        if (i >= 0 && i < g_zoom_nsteps) view_set_zoom(v, g_zoom_steps[i], -1, -1);
        return;
    }
    case CMD_FIT_SCREEN: if (v) view_zoom_fit(v, 0); return;
    case CMD_ACTUAL_PIXELS: if (v) view_zoom_fit(v, 1); return;
    case CMD_PRINT_SIZE: {
        HDC dc = GetDC(NULL);
        double sdpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(NULL, dc);
        if (v) view_set_zoom(v, sdpi / PC_MAX(1.0, d->s.dpi), -1, -1);
        return;
    }
    case CMD_SHOW_EDGES: if (v) { v->show_edges = !v->show_edges; view_invalidate(v, NULL); } return;
    case CMD_SHOW_GRID: if (v) { v->show_grid = !v->show_grid; view_invalidate(v, NULL); } return;
    case CMD_SHOW_RULERS: if (v) { v->show_rulers = !v->show_rulers; view_clamp_scroll(v); view_invalidate(v, NULL); } return;
    /* ---- palette commands ---- */
    case CMD_HISTORY_SNAPSHOT: {
        char name[48];
        int k = 1, i;
        for (i = 0; i < d->nhist; i++) if (d->hist[i].snapshot) k++;
        pc_snprintf(name, sizeof name, "Snapshot %d", k);
        pc_hist_snapshot(d, name);
        app_refresh(R_HISTORY);
        return;
    }
    case CMD_HISTORY_NEWDOC: {
        PcDoc *nd = (PcDoc *)calloc(1, sizeof *nd);
        if (!nd) return;
        pc_state_copy(&nd->s, &d->s);
        nd->hist_limit = d->hist_limit;
        pc_hist_reset(nd, "Duplicate State");
        pc_snprintf(nd->title, sizeof nd->title, "%s", d->hist[d->hist_cur].label);
        nd->dirty = 1;
        app_add_doc(nd, NULL);
        return;
    }
    case CMD_HISTORY_DELETE:
        /* Deleting a state discards it and every later one (linear history). */
        if (d->hist_cur > 0) {
            int k = d->hist_cur, i;
            pc_hist_goto(d, k - 1);
            for (i = d->nhist - 1; i >= k; i--) {
                if (d->hist[i].snapshot) continue;
                if (d->hist[i].state) { pc_state_release(d->hist[i].state); free(d->hist[i].state); }
                memmove(&d->hist[i], &d->hist[i + 1], sizeof(PcHistEntry) * (d->nhist - i - 1));
                d->nhist--;
            }
            if (d->history_source >= d->nhist) d->history_source = 0;
            app_refresh(R_ALL);
        }
        return;
    case CMD_PATH_FILL: case CMD_PATH_STROKE: case CMD_PATH_TO_SEL: {
        float *xy; u8 *st; int n;
        path_to_polys(d, &xy, &st, &n);
        if (n >= 2) {
            if (cmd == CMD_PATH_TO_SEL) { if (!pc_sel_polygons(d, xy, st, n, PC_SEL_REPLACE, 1, 0)) commit(d, "Make Selection"); }
            else if (cmd == CMD_PATH_FILL) {
                PcBuf *old = d->s.sel ? pc_buf_ref(d->s.sel) : NULL;
                u8 *m = (u8 *)pc_alloc_checked((size_t)d->s.w, (size_t)d->s.h, 1, 1);
                if (m && layer_editable(d, "Fill Path")) {
                    PcBuf *tmp = pc_buf_new(d->s.w, d->s.h, 1, 0);
                    pc_raster_polygons(m, d->s.w, d->s.h, xy, st, n, 1);
                    if (tmp) {
                        memcpy(tmp->px, m, (size_t)d->s.w * d->s.h);
                        d->s.sel = tmp;
                        pc_fill_color(d, g_app.fg, 100, PC_BLEND_NORMAL, 0);
                        d->s.sel = old; old = NULL;
                        pc_buf_unref(tmp);
                        commit(d, "Fill Path");
                    }
                }
                pc_buf_unref(old);
                free(m);
            } else if (layer_editable(d, "Stroke Path")) {
                /* Stroke with the current painting tool's brush (PS5 Stroke Path). */
                int t = brush_tool_for_stroke();
                PcPaintOpts po;
                memset(&po, 0, sizeof po);
                po.tool = t; po.brush = g_app.topt[T_BRUSH + (t == PC_PAINT_PENCIL ? 2 : t == PC_PAINT_AIRBRUSH ? 1 : 0)].brush;
                if (po.brush.spacing <= 0) po.brush.spacing = 25;
                if (po.brush.roundness <= 0) po.brush.roundness = 100;
                po.opacity = 100; po.color = g_app.fg; po.bg = g_app.bg;
                {
                    /* Close sub-paths for stroking. */
                    PcPath *p = &d->s.paths[d->s.active_path];
                    float *xy2 = (float *)malloc(sizeof(float) * 2 * (size_t)(n + p->n + 4));
                    u8 *st2 = (u8 *)malloc((size_t)(n + p->n + 4));
                    int m = 0, k, s0 = 0;
                    if (xy2 && st2) {
                        for (k = 0; k < n; k++) {
                            if (st[k] && k > 0) { int ps = 0, q; for (q = 0; q < p->n; q++) if (p->pts[q].start && p->pts[q].closed) ps = 1; if (ps) { xy2[m * 2] = xy[s0 * 2]; xy2[m * 2 + 1] = xy[s0 * 2 + 1]; st2[m++] = 0; } }
                            if (st[k]) s0 = k;
                            xy2[m * 2] = xy[k * 2]; xy2[m * 2 + 1] = xy[k * 2 + 1]; st2[m++] = st[k];
                        }
                        if (p->n && p->pts[0].closed) { xy2[m * 2] = xy[s0 * 2]; xy2[m * 2 + 1] = xy[s0 * 2 + 1]; st2[m++] = 0; }
                        if (!pc_paint_polyline(d, &po, xy2, st2, m)) commit(d, "Stroke Path");
                    }
                    free(xy2); free(st2);
                }
            }
        }
        free(xy); free(st);
        return;
    }
    case CMD_SEL_TO_PATH: selection_to_path(d); commit(d, "Make Work Path"); return;
    case CMD_PATH_SAVE: {
        PcPath *p = &d->s.paths[d->s.active_path];
        char name[32];
        if (!strcmp(p->name, "Work Path")) pc_snprintf(name, sizeof name, "Path %d", d->s.npaths);
        else pc_strlcpy(name, p->name, sizeof name);
        if (!dlg_text_input(!strcmp(p->name, "Work Path") ? "Save Path" : "Rename Path", "Name:", name, sizeof name)) return;
        pc_strlcpy(p->name, name, sizeof p->name);
        commit(d, "Save Path");
        return;
    }
    case CMD_PATH_DELETE: {
        int k = d->s.active_path;
        pc_path_free_pts(&d->s.paths[k]);
        memmove(&d->s.paths[k], &d->s.paths[k + 1], sizeof(PcPath) * (d->s.npaths - k - 1));
        d->s.npaths--;
        d->s.active_path = -1;
        commit(d, "Delete Path");
        return;
    }
    case CMD_CHANNEL_NEW: {
        char name[32];
        PcChannel *ch;
        if (d->s.nchannels >= PC_MAX_CHANNELS) return;
        pc_snprintf(name, sizeof name, "Alpha %d", d->s.nchannels + 1);
        if (!dlg_text_input("New Channel", "Name:", name, sizeof name)) return;
        ch = &d->s.channels[d->s.nchannels];
        memset(ch, 0, sizeof *ch);
        ch->buf = pc_buf_new(d->s.w, d->s.h, 1, 1);
        if (!ch->buf) return;
        pc_strlcpy(ch->name, name, sizeof ch->name);
        ch->color = 0xFF0000; ch->visible = 1;
        d->s.nchannels++;
        if (v) v->edit_channel = 100 + d->s.nchannels - 1;
        commit(d, "New Channel");
        return;
    }
    case CMD_CHANNEL_DELETE: case CMD_CHANNEL_DUPLICATE: {
        int k = v->edit_channel - 100;
        if (k < 0 || k >= d->s.nchannels) return;
        if (cmd == CMD_CHANNEL_DELETE) {
            pc_buf_unref(d->s.channels[k].buf);
            memmove(&d->s.channels[k], &d->s.channels[k + 1], sizeof(PcChannel) * (d->s.nchannels - k - 1));
            d->s.nchannels--;
            v->edit_channel = -1;
            commit(d, "Delete Channel");
        } else if (d->s.nchannels < PC_MAX_CHANNELS) {
            PcChannel *nc = &d->s.channels[d->s.nchannels];
            *nc = d->s.channels[k];
            nc->buf = pc_buf_clone(d->s.channels[k].buf);
            if (!nc->buf) return;
            pc_snprintf(nc->name, sizeof nc->name, "%.24s copy", d->s.channels[k].name);
            d->s.nchannels++;
            commit(d, "Duplicate Channel");
        }
        return;
    }
    case CMD_COMMIT: if (v) tool_commit(v); return;
    case CMD_CANCEL: if (v) tool_cancel(v); return;
    }
}

int brush_tool_for_stroke(void)
{
    switch (g_app.tool) {
    case T_PENCIL: return PC_PAINT_PENCIL;
    case T_AIRBRUSH: return PC_PAINT_AIRBRUSH;
    case T_ERASER: return PC_PAINT_ERASER;
    default: return PC_PAINT_BRUSH;
    }
}
