/*
 * Settings persistence in HKCU\Software\PhotoCraft Classic\Settings.
 * The installer never writes there, so upgrades keep the user's
 * preferences, workspace and tool options.
 */
#include "app.h"

static HKEY open_key(const char *sub, int create)
{
    char path[160];
    HKEY k = NULL;
    pc_snprintf(path, sizeof path, "%s\\%s", APP_REGKEY, sub);
    if (create) {
        DWORD disp;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp) != ERROR_SUCCESS) return NULL;
    } else if (RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) != ERROR_SUCCESS) {
        return NULL;
    }
    return k;
}

int reg_get_int(const char *sub, const char *name, int def)
{
    HKEY k = open_key(sub, 0);
    DWORD v = 0, sz = sizeof v, type = 0;
    if (!k) return def;
    if (RegQueryValueExA(k, name, NULL, &type, (BYTE *)&v, &sz) != ERROR_SUCCESS || type != REG_DWORD) { RegCloseKey(k); return def; }
    RegCloseKey(k);
    return (int)v;
}

void reg_set_int(const char *sub, const char *name, int v)
{
    HKEY k = open_key(sub, 1);
    DWORD d = (DWORD)v;
    if (!k) return;
    RegSetValueExA(k, name, 0, REG_DWORD, (const BYTE *)&d, sizeof d);
    RegCloseKey(k);
}

int reg_get_str(const char *sub, const char *name, char *out, int n)
{
    HKEY k = open_key(sub, 0);
    DWORD sz = (DWORD)n, type = 0;
    if (n > 0) out[0] = 0;
    if (!k) return 0;
    if (RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz) != ERROR_SUCCESS || type != REG_SZ) { RegCloseKey(k); if (n > 0) out[0] = 0; return 0; }
    RegCloseKey(k);
    if (n > 0) out[n - 1] = 0;
    return 1;
}

void reg_set_str(const char *sub, const char *name, const char *v)
{
    HKEY k = open_key(sub, 1);
    if (!k) return;
    RegSetValueExA(k, name, 0, REG_SZ, (const BYTE *)v, (DWORD)strlen(v) + 1);
    RegCloseKey(k);
}

int reg_get_bin(const char *sub, const char *name, void *out, int n)
{
    HKEY k = open_key(sub, 0);
    DWORD sz = (DWORD)n, type = 0;
    if (!k) return 0;
    if (RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz) != ERROR_SUCCESS || type != REG_BINARY || sz != (DWORD)n) { RegCloseKey(k); return 0; }
    RegCloseKey(k);
    return 1;
}

void reg_set_bin(const char *sub, const char *name, const void *v, int n)
{
    HKEY k = open_key(sub, 1);
    if (!k) return;
    RegSetValueExA(k, name, 0, REG_BINARY, (const BYTE *)v, (DWORD)n);
    RegCloseKey(k);
}

/* ---------------- save options per format (the 1998 output rule) ---------------- */

/* Bump when PcSaveOpts / PcQuantOpts change layout so stale blobs are ignored. */
#define SAVEOPTS_VERSION 1

int save_opts_kind(const PcDoc *d)
{
    if (d->s.mode == PC_MODE_GRAY) return 2;
    return pc_doc_has_transparency(d) ? 1 : 0;
}

static void sanitize_quant(PcQuantOpts *q)
{
    q->palette = PC_CLAMP(q->palette, 0, PC_PAL_COUNT - 1);
    q->colors = PC_CLAMP(q->colors, 2, 256);
    q->dither = PC_CLAMP(q->dither, 0, PC_DITHER_COUNT - 1);
    q->transparency = q->transparency != 0;
    q->colorkey = q->colorkey != 0;
    q->full_alpha = q->full_alpha != 0;
    q->key_rgb &= 0xFFFFFF;
}

static void save_opts_name(char *out, size_t n, int fmt, int kind)
{
    static const char *const kinds[3] = { "", " Transparent", " Gray" };
    pc_snprintf(out, n, "Save %s%s", pc_format_ext(fmt), kinds[PC_CLAMP(kind, 0, 2)]);
}

void settings_load_save_opts(int fmt, int kind, PcSaveOpts *o)
{
    char name[48];
    PcSaveOpts tmp;
    pc_save_opts_default(o);
    o->jpeg_quality = g_app.prefs.jpeg_quality;
    /* Defaults (the dialog proposes these the first time): BMP/TIFF 24-bit,
     * Targa 24 (32 with transparency), PNG 24 (32 and full alpha with
     * transparency), GIF Adaptive 256 with Diffusion, gray documents 8-bit. */
    o->depth = pc_format_default_depth(fmt, kind == 2, kind == 1);
    if (fmt == PC_FMT_PNG && kind == 1) o->q.full_alpha = 1;
    if (fmt == PC_FMT_PSD) o->depth = 0;
    save_opts_name(name, sizeof name, fmt, kind);
    if (reg_get_int("Settings", "SaveOptsVersion", 0) != SAVEOPTS_VERSION) return;
    if (!reg_get_bin("Settings", name, &tmp, sizeof tmp)) return;
    if (!pc_format_depth_ok(fmt, tmp.depth)) return;
    sanitize_quant(&tmp.q);
    tmp.green6 = tmp.green6 != 0;
    tmp.interlace = tmp.interlace != 0;
    tmp.tiff_compress = PC_CLAMP(tmp.tiff_compress, 0, 2);
    tmp.tga_rle = tmp.tga_rle != 0;
    tmp.jpeg_quality = PC_CLAMP(tmp.jpeg_quality, 0, 12);
    *o = tmp;
}

void settings_store_save_opts(int fmt, int kind, const PcSaveOpts *o)
{
    char name[48];
    save_opts_name(name, sizeof name, fmt, kind);
    reg_set_int("Settings", "SaveOptsVersion", SAVEOPTS_VERSION);
    reg_set_bin("Settings", name, o, sizeof *o);
    if (fmt == PC_FMT_JPEG) g_app.prefs.jpeg_quality = o->jpeg_quality;
}

void settings_load_indexed(PcQuantOpts *q)
{
    PcQuantOpts tmp;
    pc_quant_defaults(q);
    if (reg_get_int("Settings", "SaveOptsVersion", 0) != SAVEOPTS_VERSION) return;
    if (!reg_get_bin("Settings", "Indexed Color", &tmp, sizeof tmp)) return;
    sanitize_quant(&tmp);
    *q = tmp;
}

void settings_store_indexed(const PcQuantOpts *q)
{
    reg_set_int("Settings", "SaveOptsVersion", SAVEOPTS_VERSION);
    reg_set_bin("Settings", "Indexed Color", q, sizeof *q);
}

/* Bump when ToolOpts changes layout so stale blobs are ignored. */
#define TOOLOPTS_VERSION 3

void settings_load(void)
{
    Prefs *p = &g_app.prefs;
    int i, ver;
    p->history_states = reg_get_int("Settings", "HistoryStates", 20);
    p->gradient_captions = reg_get_int("Settings", "GradientCaptions", 0);
    p->precise_cursors = reg_get_int("Settings", "PreciseCursors", 0);
    p->brush_size_cursor = reg_get_int("Settings", "BrushSizeCursor", 1);
    p->interpolation = reg_get_int("Settings", "Interpolation", PC_RESAMPLE_BICUBIC);
    p->units = reg_get_int("Settings", "Units", 0);
    p->checker_size = reg_get_int("Settings", "CheckerSize", 1);
    p->export_clipboard = reg_get_int("Settings", "ExportClipboard", 1);
    p->beep_when_done = reg_get_int("Settings", "BeepWhenDone", 0);
    p->jpeg_quality = reg_get_int("Settings", "JpegQuality", 8);
    p->grid_spacing = reg_get_int("Settings", "GridSpacing", 32);
    p->grid_color = (u32)reg_get_int("Settings", "GridColor", 0x808080);
    p->tab_docs = reg_get_int("Settings", "TabDocuments", 1);
    p->classic_file_dialogs = reg_get_int("Settings", "ClassicFileDialogs", 0);
    p->history_states = PC_CLAMP(p->history_states, 1, 100);
    g_app.fg = (u32)reg_get_int("Settings", "Foreground", 0x000000) & 0xFFFFFF;
    g_app.bg = (u32)reg_get_int("Settings", "Background", 0xFFFFFF) & 0xFFFFFF;
    g_app.tool = PC_CLAMP(reg_get_int("Settings", "Tool", T_MOVE), 0, T_COUNT - 1);
    reg_get_str("Settings", "LastDir", g_app.last_dir, sizeof g_app.last_dir);
    g_app.nrecent = 0;
    for (i = 0; i < MAX_RECENT; i++) {
        char name[16];
        pc_snprintf(name, sizeof name, "Recent%d", i);
        if (reg_get_str("Settings", name, g_app.recent[g_app.nrecent], 260) && g_app.recent[g_app.nrecent][0]) g_app.nrecent++;
    }
    ver = reg_get_int("Settings", "ToolOptsVersion", 0);
    if (ver == TOOLOPTS_VERSION) {
        ToolOpts tmp[T_COUNT];
        if (reg_get_bin("Settings", "ToolOpts", tmp, sizeof tmp)) {
            for (i = 0; i < T_COUNT; i++) {
                tmp[i].brush.diameter = PC_CLAMP(tmp[i].brush.diameter, 1, 999);
                tmp[i].opacity = PC_CLAMP(tmp[i].opacity, 1, 100);
                tmp[i].blend = PC_CLAMP(tmp[i].blend, 0, PC_BLEND_COUNT - 1);
            }
            memcpy(g_app.topt, tmp, sizeof tmp);
        }
        reg_get_bin("Settings", "GroupTools", g_app.group_tool, sizeof g_app.group_tool);
        for (i = 0; i < 32; i++) if (g_app.group_tool[i] < 0 || g_app.group_tool[i] >= T_COUNT || g_tool_info[g_app.group_tool[i]].group != i) g_app.group_tool[i] = -1;
    }
    for (i = 0; i < T_COUNT; i++) if (g_app.group_tool[g_tool_info[i].group] < 0) g_app.group_tool[g_tool_info[i].group] = i;
    g_app.group_tool[g_tool_info[g_app.tool].group] = g_app.tool;
}

void settings_save(void)
{
    Prefs *p = &g_app.prefs;
    int i;
    reg_set_int("Settings", "HistoryStates", p->history_states);
    reg_set_int("Settings", "GradientCaptions", p->gradient_captions);
    reg_set_int("Settings", "PreciseCursors", p->precise_cursors);
    reg_set_int("Settings", "BrushSizeCursor", p->brush_size_cursor);
    reg_set_int("Settings", "Interpolation", p->interpolation);
    reg_set_int("Settings", "Units", p->units);
    reg_set_int("Settings", "CheckerSize", p->checker_size);
    reg_set_int("Settings", "ExportClipboard", p->export_clipboard);
    reg_set_int("Settings", "BeepWhenDone", p->beep_when_done);
    reg_set_int("Settings", "JpegQuality", p->jpeg_quality);
    reg_set_int("Settings", "GridSpacing", p->grid_spacing);
    reg_set_int("Settings", "GridColor", (int)p->grid_color);
    reg_set_int("Settings", "TabDocuments", p->tab_docs);
    reg_set_int("Settings", "ClassicFileDialogs", p->classic_file_dialogs);
    reg_set_int("Settings", "Foreground", (int)g_app.fg);
    reg_set_int("Settings", "Background", (int)g_app.bg);
    reg_set_int("Settings", "Tool", g_app.tool);
    reg_set_str("Settings", "LastDir", g_app.last_dir);
    for (i = 0; i < MAX_RECENT; i++) {
        char name[16];
        pc_snprintf(name, sizeof name, "Recent%d", i);
        reg_set_str("Settings", name, i < g_app.nrecent ? g_app.recent[i] : "");
    }
    reg_set_int("Settings", "ToolOptsVersion", TOOLOPTS_VERSION);
    reg_set_bin("Settings", "ToolOpts", g_app.topt, sizeof g_app.topt);
    reg_set_bin("Settings", "GroupTools", g_app.group_tool, sizeof g_app.group_tool);
}
