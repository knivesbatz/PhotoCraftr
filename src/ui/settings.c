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
