/*
 * Command table: every menu item, shortcut and recordable action is one
 * entry here.  X(id, menu label, shortcut text, virtual key, modifiers)
 * modifiers: 1 Ctrl, 2 Shift, 4 Alt.  vk 0 = no direct key binding.
 */
#ifndef CMDS_H
#define CMDS_H

#define MOD_C 1
#define MOD_S 2
#define MOD_A 4
#define VKC(ch) (ch)

#define PC_COMMANDS(X) \
    X(CMD_NONE, "", "", 0, 0) \
    /* File */ \
    X(CMD_NEW, "&New...", "Ctrl+N", 'N', MOD_C) \
    X(CMD_OPEN, "&Open...", "Ctrl+O", 'O', MOD_C) \
    X(CMD_CLOSE, "&Close", "Ctrl+W", 'W', MOD_C) \
    X(CMD_CLOSE_ALL, "Close A&ll", "Alt+Ctrl+W", 'W', MOD_C | MOD_A) \
    X(CMD_SAVE, "&Save", "Ctrl+S", 'S', MOD_C) \
    X(CMD_SAVE_AS, "Save &As...", "Shift+Ctrl+S", 'S', MOD_C | MOD_S) \
    X(CMD_SAVE_COPY, "Save a Cop&y...", "Alt+Ctrl+S", 'S', MOD_C | MOD_A) \
    X(CMD_REVERT, "Re&vert", "F12", VK_F12, 0) \
    X(CMD_PAGE_SETUP, "Page Set&up...", "Shift+Ctrl+P", 'P', MOD_C | MOD_S) \
    X(CMD_PRINT, "&Print...", "Ctrl+P", 'P', MOD_C) \
    X(CMD_EXIT, "E&xit", "Ctrl+Q", 'Q', MOD_C) \
    X(CMD_RECENT_0, "1", "", 0, 0) X(CMD_RECENT_1, "2", "", 0, 0) X(CMD_RECENT_2, "3", "", 0, 0) X(CMD_RECENT_3, "4", "", 0, 0) \
    X(CMD_RECENT_4, "5", "", 0, 0) X(CMD_RECENT_5, "6", "", 0, 0) X(CMD_RECENT_6, "7", "", 0, 0) X(CMD_RECENT_7, "8", "", 0, 0) \
    /* Edit */ \
    X(CMD_UNDO, "&Undo", "Ctrl+Z", 'Z', MOD_C) \
    X(CMD_STEP_FORWARD, "Step For&ward", "Shift+Ctrl+Z", 'Z', MOD_C | MOD_S) \
    X(CMD_STEP_BACKWARD, "Step Bac&kward", "Alt+Ctrl+Z", 'Z', MOD_C | MOD_A) \
    X(CMD_CUT, "Cu&t", "Ctrl+X", 'X', MOD_C) \
    X(CMD_COPY, "&Copy", "Ctrl+C", 'C', MOD_C) \
    X(CMD_COPY_MERGED, "Copy &Merged", "Shift+Ctrl+C", 'C', MOD_C | MOD_S) \
    X(CMD_PASTE, "&Paste", "Ctrl+V", 'V', MOD_C) \
    X(CMD_PASTE_INTO, "Paste &Into", "Shift+Ctrl+V", 'V', MOD_C | MOD_S) \
    X(CMD_CLEAR, "Cl&ear", "Del", VK_DELETE, 0) \
    X(CMD_FILL, "&Fill...", "Shift+F5", VK_F5, MOD_S) \
    X(CMD_STROKE, "&Stroke...", "", 0, 0) \
    X(CMD_FREE_TRANSFORM, "Free T&ransform", "Ctrl+T", 'T', MOD_C) \
    X(CMD_TRANSFORM_NUMERIC, "&Numeric...", "Shift+Ctrl+T", 'T', MOD_C | MOD_S) \
    X(CMD_LAYER_ROT180, "Rotate &180\xB0", "", 0, 0) \
    X(CMD_LAYER_ROT90CW, "Rotate &90\xB0 CW", "", 0, 0) \
    X(CMD_LAYER_ROT90CCW, "Rotate 9&0\xB0 CCW", "", 0, 0) \
    X(CMD_LAYER_FLIPH, "Flip &Horizontal", "", 0, 0) \
    X(CMD_LAYER_FLIPV, "Flip &Vertical", "", 0, 0) \
    X(CMD_DEFINE_PATTERN, "Define Patter&n", "", 0, 0) \
    X(CMD_PURGE_UNDO, "&Undo", "", 0, 0) \
    X(CMD_PURGE_CLIPBOARD, "&Clipboard", "", 0, 0) \
    X(CMD_PURGE_HISTORIES, "&Histories", "", 0, 0) \
    X(CMD_PURGE_ALL, "&All", "", 0, 0) \
    X(CMD_PREFERENCES, "&General...", "Ctrl+K", 'K', MOD_C) \
    /* Image */ \
    X(CMD_MODE_GRAY, "&Grayscale", "", 0, 0) \
    X(CMD_MODE_RGB, "&RGB Color", "", 0, 0) \
    X(CMD_LEVELS, "&Levels...", "Ctrl+L", 'L', MOD_C) \
    X(CMD_AUTO_LEVELS, "&Auto Levels", "Shift+Ctrl+L", 'L', MOD_C | MOD_S) \
    X(CMD_CURVES, "&Curves...", "Ctrl+M", 'M', MOD_C) \
    X(CMD_COLOR_BALANCE, "Color &Balance...", "Ctrl+B", 'B', MOD_C) \
    X(CMD_BRIGHTNESS_CONTRAST, "Brightness/C&ontrast...", "", 0, 0) \
    X(CMD_HUE_SAT, "&Hue/Saturation...", "Ctrl+U", 'U', MOD_C) \
    X(CMD_DESATURATE, "&Desaturate", "Shift+Ctrl+U", 'U', MOD_C | MOD_S) \
    X(CMD_CHANNEL_MIXER, "Channel &Mixer...", "", 0, 0) \
    X(CMD_INVERT, "&Invert", "Ctrl+I", 'I', MOD_C) \
    X(CMD_EQUALIZE, "&Equalize", "", 0, 0) \
    X(CMD_THRESHOLD, "&Threshold...", "", 0, 0) \
    X(CMD_POSTERIZE, "&Posterize...", "", 0, 0) \
    X(CMD_DUPLICATE, "D&uplicate...", "", 0, 0) \
    X(CMD_IMAGE_SIZE, "&Image Size...", "Alt+Ctrl+I", 'I', MOD_C | MOD_A) \
    X(CMD_CANVAS_SIZE, "Can&vas Size...", "Alt+Ctrl+C", 'C', MOD_C | MOD_A) \
    X(CMD_ROT180, "&180\xB0", "", 0, 0) \
    X(CMD_ROT90CW, "&90\xB0 CW", "", 0, 0) \
    X(CMD_ROT90CCW, "9&0\xB0 CCW", "", 0, 0) \
    X(CMD_ROT_ARBITRARY, "&Arbitrary...", "", 0, 0) \
    X(CMD_FLIP_CANVAS_H, "Flip &Horizontal", "", 0, 0) \
    X(CMD_FLIP_CANVAS_V, "Flip &Vertical", "", 0, 0) \
    X(CMD_CROP, "&Crop", "", 0, 0) \
    X(CMD_TRIM, "T&rim", "", 0, 0) \
    X(CMD_HISTOGRAM, "Histo&gram...", "", 0, 0) \
    /* Layer */ \
    X(CMD_NEW_LAYER, "&Layer...", "Shift+Ctrl+N", 'N', MOD_C | MOD_S) \
    X(CMD_LAYER_VIA_COPY, "Layer via &Copy", "Ctrl+J", 'J', MOD_C) \
    X(CMD_LAYER_VIA_CUT, "Layer via Cu&t", "Shift+Ctrl+J", 'J', MOD_C | MOD_S) \
    X(CMD_LAYER_FROM_BG, "Layer &from Background...", "", 0, 0) \
    X(CMD_DUPLICATE_LAYER, "D&uplicate Layer...", "", 0, 0) \
    X(CMD_DELETE_LAYER, "&Delete Layer", "", 0, 0) \
    X(CMD_LAYER_PROPS, "Layer &Properties...", "", 0, 0) \
    X(CMD_LAYER_STYLE, "&Blending Options...", "", 0, 0) \
    X(CMD_STYLE_CLEAR, "&Clear Layer Style", "", 0, 0) \
    X(CMD_MASK_REVEAL_ALL, "&Reveal All", "", 0, 0) \
    X(CMD_MASK_HIDE_ALL, "&Hide All", "", 0, 0) \
    X(CMD_MASK_REVEAL_SEL, "Re&veal Selection", "", 0, 0) \
    X(CMD_MASK_HIDE_SEL, "H&ide Selection", "", 0, 0) \
    X(CMD_MASK_APPLY, "&Apply", "", 0, 0) \
    X(CMD_MASK_DELETE, "&Delete", "", 0, 0) \
    X(CMD_MASK_TOGGLE, "Dis&able", "", 0, 0) \
    X(CMD_CLIP_GROUP, "&Group with Previous", "Alt+Ctrl+G", 'G', MOD_C | MOD_A) \
    X(CMD_CLIP_UNGROUP, "U&ngroup", "Shift+Ctrl+G", 'G', MOD_C | MOD_S) \
    X(CMD_BRING_FRONT, "Bring to &Front", "Shift+Ctrl+]", 0xDD, MOD_C | MOD_S) \
    X(CMD_BRING_FORWARD, "Bring For&ward", "Ctrl+]", 0xDD, MOD_C) \
    X(CMD_SEND_BACKWARD, "Send Back&ward", "Ctrl+[", 0xDB, MOD_C) \
    X(CMD_SEND_BACK, "Send to &Back", "Shift+Ctrl+[", 0xDB, MOD_C | MOD_S) \
    X(CMD_MERGE_DOWN, "&Merge Down", "Ctrl+E", 'E', MOD_C) \
    X(CMD_MERGE_VISIBLE, "Merge &Visible", "Shift+Ctrl+E", 'E', MOD_C | MOD_S) \
    X(CMD_FLATTEN, "&Flatten Image", "", 0, 0) \
    /* Type */ \
    X(CMD_TYPE_EDIT, "&Edit Text...", "", 0, 0) \
    X(CMD_TYPE_RASTERIZE, "&Rasterize Type Layer", "", 0, 0) \
    X(CMD_TYPE_AA_NONE, "&None", "", 0, 0) \
    X(CMD_TYPE_AA_CRISP, "&Crisp", "", 0, 0) \
    X(CMD_TYPE_AA_STRONG, "&Strong", "", 0, 0) \
    X(CMD_TYPE_AA_SMOOTH, "S&mooth", "", 0, 0) \
    X(CMD_TYPE_HORIZONTAL, "&Horizontal", "", 0, 0) \
    X(CMD_TYPE_VERTICAL, "&Vertical", "", 0, 0) \
    /* Select */ \
    X(CMD_SELECT_ALL, "&All", "Ctrl+A", 'A', MOD_C) \
    X(CMD_DESELECT, "&Deselect", "Ctrl+D", 'D', MOD_C) \
    X(CMD_RESELECT, "&Reselect", "Shift+Ctrl+D", 'D', MOD_C | MOD_S) \
    X(CMD_INVERSE, "&Inverse", "Shift+Ctrl+I", 'I', MOD_C | MOD_S) \
    X(CMD_COLOR_RANGE, "&Color Range...", "", 0, 0) \
    X(CMD_FEATHER, "&Feather...", "Alt+Ctrl+D", 'D', MOD_C | MOD_A) \
    X(CMD_SEL_BORDER, "&Border...", "", 0, 0) \
    X(CMD_SEL_SMOOTH, "&Smooth...", "", 0, 0) \
    X(CMD_SEL_EXPAND, "&Expand...", "", 0, 0) \
    X(CMD_SEL_CONTRACT, "&Contract...", "", 0, 0) \
    X(CMD_GROW, "&Grow", "", 0, 0) \
    X(CMD_SIMILAR, "Si&milar", "", 0, 0) \
    X(CMD_LOAD_SELECTION, "&Load Selection...", "", 0, 0) \
    X(CMD_SAVE_SELECTION, "&Save Selection...", "", 0, 0) \
    X(CMD_QUICK_MASK, "Edit in &Quick Mask Mode", "Q", 0, 0) \
    /* Filter */ \
    X(CMD_LAST_FILTER, "&Last Filter", "Ctrl+F", 'F', MOD_C) \
    X(CMD_F_BLUR, "&Blur", "", 0, 0) X(CMD_F_BLUR_MORE, "Blur &More", "", 0, 0) \
    X(CMD_F_GAUSSIAN, "&Gaussian Blur...", "", 0, 0) X(CMD_F_MOTION, "M&otion Blur...", "", 0, 0) \
    X(CMD_F_RADIAL, "&Radial Blur...", "", 0, 0) \
    X(CMD_F_SHARPEN, "&Sharpen", "", 0, 0) X(CMD_F_SHARPEN_MORE, "Sharpen &More", "", 0, 0) \
    X(CMD_F_SHARPEN_EDGES, "Sharpen &Edges", "", 0, 0) X(CMD_F_UNSHARP, "&Unsharp Mask...", "", 0, 0) \
    X(CMD_F_NOISE, "&Add Noise...", "", 0, 0) X(CMD_F_DESPECKLE, "&Despeckle", "", 0, 0) \
    X(CMD_F_DUST, "D&ust & Scratches...", "", 0, 0) X(CMD_F_MEDIAN, "&Median...", "", 0, 0) \
    X(CMD_F_EMBOSS, "&Emboss...", "", 0, 0) X(CMD_F_FIND_EDGES, "&Find Edges", "", 0, 0) \
    X(CMD_F_SOLARIZE, "&Solarize", "", 0, 0) X(CMD_F_DIFFUSE, "&Diffuse...", "", 0, 0) \
    X(CMD_F_WIND, "&Wind...", "", 0, 0) X(CMD_F_TRACE, "&Trace Contour...", "", 0, 0) \
    X(CMD_F_MOSAIC, "&Mosaic...", "", 0, 0) \
    X(CMD_F_TWIRL, "&Twirl...", "", 0, 0) X(CMD_F_PINCH, "&Pinch...", "", 0, 0) \
    X(CMD_F_SPHERIZE, "&Spherize...", "", 0, 0) X(CMD_F_RIPPLE, "&Ripple...", "", 0, 0) \
    X(CMD_F_CLOUDS, "&Clouds", "", 0, 0) X(CMD_F_DIFF_CLOUDS, "&Difference Clouds", "", 0, 0) \
    X(CMD_F_OFFSET, "&Offset...", "", 0, 0) X(CMD_F_HIGH_PASS, "&High Pass...", "", 0, 0) \
    X(CMD_F_MAXIMUM, "Ma&ximum...", "", 0, 0) X(CMD_F_MINIMUM, "Mi&nimum...", "", 0, 0) \
    X(CMD_F_CUSTOM, "&Custom...", "", 0, 0) \
    /* View */ \
    X(CMD_ZOOM_IN, "Zoom &In", "Ctrl++", 0xBB, MOD_C) \
    X(CMD_ZOOM_OUT, "Zoom &Out", "Ctrl+-", 0xBD, MOD_C) \
    X(CMD_FIT_SCREEN, "&Fit on Screen", "Ctrl+0", '0', MOD_C) \
    X(CMD_ACTUAL_PIXELS, "&Actual Pixels", "Ctrl+1", '1', MOD_C) \
    X(CMD_PRINT_SIZE, "Print Si&ze", "", 0, 0) \
    X(CMD_SCREEN_STANDARD, "&Standard Screen Mode", "", 0, 0) \
    X(CMD_SCREEN_FULL_MENU, "Full Screen Mode with &Menu Bar", "", 0, 0) \
    X(CMD_SCREEN_FULL, "&Full Screen Mode", "", 0, 0) \
    X(CMD_SHOW_EDGES, "Selection &Edges", "Ctrl+H", 'H', MOD_C) \
    X(CMD_SHOW_GRID, "&Grid", "Ctrl+'", 0xDE, MOD_C) \
    X(CMD_SHOW_RULERS, "&Rulers", "Ctrl+R", 'R', MOD_C) \
    /* Window */ \
    X(CMD_CASCADE, "&Cascade", "", 0, 0) \
    X(CMD_TILE_H, "Tile &Horizontally", "", 0, 0) \
    X(CMD_TILE_V, "Tile &Vertically", "", 0, 0) \
    X(CMD_FLOAT_WINDOW, "&Float in Window", "", 0, 0) \
    X(CMD_FLOAT_ALL, "Float &All in Windows", "", 0, 0) \
    X(CMD_CONSOLIDATE, "Consolidate All to &Tabs", "", 0, 0) \
    X(CMD_WS_ESSENTIALS, "&Essentials (Default)", "", 0, 0) \
    X(CMD_WS_RESET, "&Reset Essentials", "", 0, 0) \
    X(CMD_WS_SAVE, "&Save Workspace", "", 0, 0) \
    X(CMD_WS_LOAD, "&Load Saved Workspace", "", 0, 0) \
    X(CMD_WS_CLASSIC98, "&1998 Palettes (Floating)", "", 0, 0) \
    X(CMD_WIN_ACTIONS, "&Actions", "Alt+F9", VK_F9, MOD_A) \
    X(CMD_WIN_BRUSHES, "&Brushes", "F5", VK_F5, 0) \
    X(CMD_WIN_CHANNELS, "&Channels", "", 0, 0) \
    X(CMD_WIN_COLOR, "C&olor", "F6", VK_F6, 0) \
    X(CMD_WIN_HISTORY, "&History", "", 0, 0) \
    X(CMD_WIN_INFO, "&Info", "F8", VK_F8, 0) \
    X(CMD_WIN_LAYERS, "&Layers", "F7", VK_F7, 0) \
    X(CMD_WIN_NAVIGATOR, "&Navigator", "", 0, 0) \
    X(CMD_WIN_PATHS, "&Paths", "", 0, 0) \
    X(CMD_WIN_SWATCHES, "&Swatches", "", 0, 0) \
    X(CMD_WIN_OPTIONS, "Op&tions", "", 0, 0) \
    X(CMD_WIN_TOOLS, "&Tools", "", 0, 0) \
    X(CMD_WIN_DOC_0, "1", "", 0, 0) X(CMD_WIN_DOC_1, "2", "", 0, 0) X(CMD_WIN_DOC_2, "3", "", 0, 0) \
    X(CMD_WIN_DOC_3, "4", "", 0, 0) X(CMD_WIN_DOC_4, "5", "", 0, 0) X(CMD_WIN_DOC_5, "6", "", 0, 0) \
    X(CMD_WIN_DOC_6, "7", "", 0, 0) X(CMD_WIN_DOC_7, "8", "", 0, 0) X(CMD_WIN_DOC_8, "9", "", 0, 0) \
    /* Help */ \
    X(CMD_HELP, "PhotoCraft Classic &Help...", "F1", VK_F1, 0) \
    X(CMD_SHORTCUTS, "&Keyboard Shortcuts...", "", 0, 0) \
    X(CMD_RELEASE_NOTES, "&Release Notes...", "", 0, 0) \
    X(CMD_CHECK_UPDATES, "Check for &Updates...", "", 0, 0) \
    X(CMD_AUTO_UPDATES, "Check for Updates at &Startup", "", 0, 0) \
    X(CMD_ABOUT, "&About PhotoCraft Classic...", "", 0, 0) \
    /* not in menus: keyboard-only and palette commands */ \
    X(CMD_DEFAULT_COLORS, "Default Colors", "D", 0, 0) \
    X(CMD_SWAP_COLORS, "Switch Colors", "X", 0, 0) \
    X(CMD_CYCLE_SCREEN, "Screen Mode", "F", 0, 0) \
    X(CMD_TOGGLE_ALL_PANELS, "Show/Hide Panels", "Tab", 0, 0) \
    X(CMD_TOGGLE_PANELS, "Show/Hide Palettes", "Shift+Tab", 0, 0) \
    X(CMD_HISTORY_SNAPSHOT, "New Snapshot", "", 0, 0) \
    X(CMD_HISTORY_NEWDOC, "New Document", "", 0, 0) \
    X(CMD_HISTORY_DELETE, "Delete", "", 0, 0) \
    X(CMD_NEW_LAYER_QUICK, "New Layer", "", 0, 0) \
    X(CMD_ADD_MASK_QUICK, "Add Layer Mask", "", 0, 0) \
    X(CMD_PATH_FILL, "Fill Path", "", 0, 0) \
    X(CMD_PATH_STROKE, "Stroke Path", "", 0, 0) \
    X(CMD_PATH_TO_SEL, "Make Selection", "", 0, 0) \
    X(CMD_SEL_TO_PATH, "Make Work Path", "", 0, 0) \
    X(CMD_PATH_SAVE, "Save Path...", "", 0, 0) \
    X(CMD_PATH_DELETE, "Delete Path", "", 0, 0) \
    X(CMD_CHANNEL_NEW, "New Channel...", "", 0, 0) \
    X(CMD_CHANNEL_DELETE, "Delete Channel", "", 0, 0) \
    X(CMD_CHANNEL_DUPLICATE, "Duplicate Channel...", "", 0, 0) \
    X(CMD_LAYER_STYLE_SHADOW, "&Drop Shadow...", "", 0, 0) \
    X(CMD_LAYER_STYLE_INNER, "&Inner Shadow...", "", 0, 0) \
    X(CMD_LAYER_STYLE_GLOW, "&Outer Glow...", "", 0, 0) \
    X(CMD_LAYER_STYLE_BEVEL, "&Bevel and Emboss...", "", 0, 0) \
    X(CMD_COMMIT, "Commit", "Enter", 0, 0) \
    X(CMD_CANCEL, "Cancel", "Esc", 0, 0) \
    /* appended (not next to Grayscale/RGB) so recorded Actions keep their command ids */ \
    X(CMD_MODE_INDEXED, "&Indexed Color...", "", 0, 0)

#define X(id, label, sc, vk, mods) id,
enum { PC_COMMANDS(X) CMD_COUNT };
#undef X

typedef struct CmdInfo { const char *label, *shortcut; int vk, mods; } CmdInfo;
extern const CmdInfo g_cmd_info[CMD_COUNT];

/* Menu model (menu.c). */
typedef struct MenuItem {
    int depth;           /* 0 = top-level menu title, 1 = item, 2 = submenu item */
    int cmd;             /* CMD_* or 0 for a submenu title / separator */
    const char *label;   /* NULL: use the command's label; "-" = separator */
} MenuItem;

#endif
