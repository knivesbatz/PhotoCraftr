/* PhotoCraft Classic - interface icon and cursor art.
 *
 * Original pixel art drawn for this project (MIT OR Apache-2.0, see
 * res/icons/README.md).  The art itself lives in icons_data.c as readable
 * ASCII grids; tools/render_icons.py renders it to docs/images/icon-sheet.png.
 *
 * Pure C data: no windows.h, so the portable code and the tests can use it.
 */
#ifndef PC_UI_ICONS_H
#define PC_UI_ICONS_H

/* One icon: w*h characters, row-major, from the 16-colour palette below.
 *   '.' transparent   'k' black    'm' maroon   'G' green    'o' olive
 *   'n' navy          'p' purple   't' teal     'g' silver   'd' gray
 *   'R' red           'L' lime     'Y' yellow   'B' blue     'F' fuchsia
 *   'C' aqua          'w' white
 * (the Windows 95 16-colour VGA palette, so icons survive 16-colour mode). */
typedef struct {
    const char *name;
    int w, h;
    const char *px;
} PcIconDef;

/* One 32x32 cursor.  px chars: '.' transparent, 'k' black, 'w' white,
 * 'x' inverted screen.  (hotx, hoty) is the hotspot. */
typedef struct {
    const char *name;
    int w, h, hotx, hoty;
    const char *px;
} PcCursorDef;

enum PcIconId {
    /* Tools */
    PC_ICON_TOOL_MOVE,                   /* 16x16 */
    PC_ICON_TOOL_MARQUEE_RECT,           /* 16x16 */
    PC_ICON_TOOL_MARQUEE_ELLIPSE,        /* 16x16 */
    PC_ICON_TOOL_MARQUEE_ROW,            /* 16x16 */
    PC_ICON_TOOL_MARQUEE_COL,            /* 16x16 */
    PC_ICON_TOOL_LASSO,                  /* 16x16 */
    PC_ICON_TOOL_LASSO_POLY,             /* 16x16 */
    PC_ICON_TOOL_LASSO_MAGNETIC,         /* 16x16 */
    PC_ICON_TOOL_WAND,                   /* 16x16 */
    PC_ICON_TOOL_CROP,                   /* 16x16 */
    PC_ICON_TOOL_EYEDROPPER,             /* 16x16 */
    PC_ICON_TOOL_MEASURE,                /* 16x16 */
    PC_ICON_TOOL_BRUSH,                  /* 16x16 */
    PC_ICON_TOOL_AIRBRUSH,               /* 16x16 */
    PC_ICON_TOOL_PENCIL,                 /* 16x16 */
    PC_ICON_TOOL_STAMP,                  /* 16x16 */
    PC_ICON_TOOL_PATTERN_STAMP,          /* 16x16 */
    PC_ICON_TOOL_HISTORY_BRUSH,          /* 16x16 */
    PC_ICON_TOOL_ERASER,                 /* 16x16 */
    PC_ICON_TOOL_GRADIENT,               /* 16x16 */
    PC_ICON_TOOL_BUCKET,                 /* 16x16 */
    PC_ICON_TOOL_BLUR,                   /* 16x16 */
    PC_ICON_TOOL_SHARPEN,                /* 16x16 */
    PC_ICON_TOOL_SMUDGE,                 /* 16x16 */
    PC_ICON_TOOL_DODGE,                  /* 16x16 */
    PC_ICON_TOOL_BURN,                   /* 16x16 */
    PC_ICON_TOOL_SPONGE,                 /* 16x16 */
    PC_ICON_TOOL_PEN,                    /* 16x16 */
    PC_ICON_TOOL_PEN_FREEFORM,           /* 16x16 */
    PC_ICON_TOOL_PEN_MAGNETIC,           /* 16x16 */
    PC_ICON_TOOL_PEN_ADD,                /* 16x16 */
    PC_ICON_TOOL_PEN_DELETE,             /* 16x16 */
    PC_ICON_TOOL_PEN_CONVERT,            /* 16x16 */
    PC_ICON_TOOL_DIRECT_SELECT,          /* 16x16 */
    PC_ICON_TOOL_PATH_SELECT,            /* 16x16 */
    PC_ICON_TOOL_TYPE,                   /* 16x16 */
    PC_ICON_TOOL_TYPE_VERTICAL,          /* 16x16 */
    PC_ICON_TOOL_TYPE_MASK,              /* 16x16 */
    PC_ICON_TOOL_TYPE_MASK_VERTICAL,     /* 16x16 */
    PC_ICON_TOOL_SHAPE_RECT,             /* 16x16 */
    PC_ICON_TOOL_SHAPE_ELLIPSE,          /* 16x16 */
    PC_ICON_TOOL_SHAPE_LINE,             /* 16x16 */
    PC_ICON_TOOL_HAND,                   /* 16x16 */
    PC_ICON_TOOL_ZOOM,                   /* 16x16 */
    /* Toolbox extras */
    PC_ICON_DEFAULT_COLORS,              /* 10x10 */
    PC_ICON_SWAP_COLORS,                 /* 10x10 */
    PC_ICON_STANDARD_MODE,               /* 16x16 */
    PC_ICON_QUICKMASK_MODE,              /* 16x16 */
    PC_ICON_SCREEN_STANDARD,             /* 16x16 */
    PC_ICON_SCREEN_FULL_MENU,            /* 16x16 */
    PC_ICON_SCREEN_FULL,                 /* 16x16 */
    PC_ICON_FLYOUT,                      /* 4x4 */
    /* Panels and dock */
    PC_ICON_PANEL_MENU,                  /* 10x9 */
    PC_ICON_CHEVRON_LEFT,                /* 8x7 */
    PC_ICON_CHEVRON_RIGHT,               /* 8x7 */
    PC_ICON_PANEL_NAVIGATOR,             /* 16x16 */
    PC_ICON_PANEL_INFO,                  /* 16x16 */
    PC_ICON_PANEL_COLOR,                 /* 16x16 */
    PC_ICON_PANEL_SWATCHES,              /* 16x16 */
    PC_ICON_PANEL_BRUSHES,               /* 16x16 */
    PC_ICON_PANEL_LAYERS,                /* 16x16 */
    PC_ICON_PANEL_CHANNELS,              /* 16x16 */
    PC_ICON_PANEL_PATHS,                 /* 16x16 */
    PC_ICON_PANEL_HISTORY,               /* 16x16 */
    PC_ICON_PANEL_ACTIONS,               /* 16x16 */
    PC_ICON_PANEL_CHARACTER,             /* 16x16 */
    /* Layers panel */
    PC_ICON_EYE,                         /* 13x11 */
    PC_ICON_BRUSH_SMALL,                 /* 13x13 */
    PC_ICON_MASK_SMALL,                  /* 13x13 */
    PC_ICON_LINK,                        /* 7x13 */
    PC_ICON_LOCK_TRANSPARENT,            /* 13x13 */
    PC_ICON_LOCK_PIXELS,                 /* 13x13 */
    PC_ICON_LOCK_POSITION,               /* 13x13 */
    PC_ICON_LOCK_ALL,                    /* 13x13 */
    PC_ICON_TEXT_LAYER,                  /* 16x16 */
    PC_ICON_FX,                          /* 13x13 */
    PC_ICON_NEW_LAYER,                   /* 16x16 */
    PC_ICON_TRASH,                       /* 16x16 */
    PC_ICON_ADD_MASK,                    /* 16x16 */
    PC_ICON_NEW_GROUP,                   /* 16x16 */
    PC_ICON_LAYER_STYLE,                 /* 16x16 */
    PC_ICON_NEW_ADJUSTMENT,              /* 16x16 */
    /* Channels / Paths */
    PC_ICON_LOAD_SELECTION,              /* 16x16 */
    PC_ICON_SAVE_SELECTION,              /* 16x16 */
    PC_ICON_FILL_PATH,                   /* 16x16 */
    PC_ICON_STROKE_PATH,                 /* 16x16 */
    PC_ICON_PATH_TO_SELECTION,           /* 16x16 */
    PC_ICON_SELECTION_TO_PATH,           /* 16x16 */
    /* History / Actions */
    PC_ICON_SNAPSHOT,                    /* 16x16 */
    PC_ICON_NEW_DOC_FROM_STATE,          /* 16x16 */
    PC_ICON_HISTORY_SOURCE,              /* 13x13 */
    PC_ICON_ACTION_STOP,                 /* 16x16 */
    PC_ICON_ACTION_RECORD,               /* 16x16 */
    PC_ICON_ACTION_PLAY,                 /* 16x16 */
    PC_ICON_ACTION_NEW_SET,              /* 16x16 */
    PC_ICON_ACTION_NEW,                  /* 16x16 */
    PC_ICON_CHECK,                       /* 9x9 */
    PC_ICON_DIALOG_TOGGLE,               /* 11x9 */
    /* Navigator */
    PC_ICON_ZOOM_OUT_MOUNTAIN,           /* 12x9 */
    PC_ICON_ZOOM_IN_MOUNTAIN,            /* 16x11 */
    /* Window chrome and message boxes */
    PC_ICON_CAPTION_CLOSE,               /* 8x7 */
    PC_ICON_CAPTION_MIN,                 /* 6x2 */
    PC_ICON_CAPTION_MAX,                 /* 9x9 */
    PC_ICON_CAPTION_RESTORE,             /* 8x9 */
    PC_ICON_ARROW_UP,                    /* 7x4 */
    PC_ICON_ARROW_DOWN,                  /* 7x4 */
    PC_ICON_ARROW_LEFT,                  /* 4x7 */
    PC_ICON_ARROW_RIGHT,                 /* 4x7 */
    PC_ICON_COMBO_ARROW,                 /* 7x4 */
    PC_ICON_MENU_CHECK,                  /* 7x7 */
    PC_ICON_MENU_RADIO,                  /* 6x6 */
    PC_ICON_SUBMENU_ARROW,               /* 4x7 */
    PC_ICON_SIZE_GRIP,                   /* 12x12 */
    PC_ICON_WARNING,                     /* 32x32 */
    PC_ICON_INFO,                        /* 32x32 */
    PC_ICON_QUESTION,                    /* 32x32 */
    PC_ICON_ERROR,                       /* 32x32 */
    /* Open/Save dialog */
    PC_ICON_FOLDER,                      /* 16x16 */
    PC_ICON_FOLDER_OPEN,                 /* 16x16 */
    PC_ICON_FILE,                        /* 16x16 */
    PC_ICON_FILE_IMAGE,                  /* 16x16 */
    PC_ICON_DRIVE_HDD,                   /* 16x16 */
    PC_ICON_DRIVE_FLOPPY,                /* 16x16 */
    PC_ICON_DRIVE_CD,                    /* 16x16 */
    PC_ICON_DRIVE_NET,                   /* 16x16 */
    PC_ICON_UP_FOLDER,                   /* 16x16 */
    PC_ICON_NEW_FOLDER,                  /* 16x16 */
    PC_ICON_VIEW_LIST,                   /* 16x16 */
    PC_ICON_VIEW_DETAILS,                /* 16x16 */
    PC_ICON_COUNT
};

enum PcCursorId {
    PC_CURSOR_CROSSHAIR,
    PC_CURSOR_MOVE,
    PC_CURSOR_HAND,
    PC_CURSOR_GRAB,
    PC_CURSOR_ZOOM_IN,
    PC_CURSOR_ZOOM_OUT,
    PC_CURSOR_ZOOM_NONE,
    PC_CURSOR_EYEDROPPER,
    PC_CURSOR_BUCKET,
    PC_CURSOR_PENCIL,
    PC_CURSOR_BRUSH,
    PC_CURSOR_AIRBRUSH,
    PC_CURSOR_STAMP,
    PC_CURSOR_STAMP_SOURCE,
    PC_CURSOR_ERASER,
    PC_CURSOR_LASSO,
    PC_CURSOR_LASSO_POLY,
    PC_CURSOR_WAND,
    PC_CURSOR_CROP,
    PC_CURSOR_IBEAM,
    PC_CURSOR_IBEAM_VERTICAL,
    PC_CURSOR_PEN,
    PC_CURSOR_PEN_ADD,
    PC_CURSOR_PEN_DELETE,
    PC_CURSOR_PEN_CLOSE,
    PC_CURSOR_DIRECT_SELECT,
    PC_CURSOR_MEASURE,
    PC_CURSOR_SMUDGE,
    PC_CURSOR_BLUR,
    PC_CURSOR_SHARPEN,
    PC_CURSOR_DODGE,
    PC_CURSOR_BURN,
    PC_CURSOR_SPONGE,
    PC_CURSOR_GRADIENT,
    PC_CURSOR_MARQUEE,
    PC_CURSOR_NO,
    PC_CURSOR_ROTATE,
    PC_CURSOR_SCALE_NS,
    PC_CURSOR_SCALE_EW,
    PC_CURSOR_SCALE_NWSE,
    PC_CURSOR_SCALE_NESW,
    PC_CURSOR_MARQUEE_ADD,
    PC_CURSOR_MARQUEE_SUB,
    PC_CURSOR_MARQUEE_INT,
    PC_CURSOR_MOVE_SELECTION,
    PC_CURSOR_COUNT
};

/* Definition of icon/cursor `id`, or NULL when id is out of range. */
const PcIconDef *pc_icon_def(int id);
const PcCursorDef *pc_cursor_def(int id);

/* Palette lookup: returns 0 for '.' (transparent) or an unknown char,
 * otherwise 1 with *rgb = 0xRRGGBB. */
int pc_icon_rgb(char c, unsigned *rgb);

#endif /* PC_UI_ICONS_H */
