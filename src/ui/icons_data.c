/* PhotoCraft Classic - interface icon and cursor art (see icons.h).
 *
 * Every icon and cursor in this file is an original drawing made for
 * PhotoCraft Classic in the style of late-1990s Windows graphics programs:
 * hand-placed pixels, black outlines, Windows 95 16-colour palette, no
 * anti-aliasing.  Licensed MIT OR Apache-2.0 like the rest of the project.
 *
 * Edit the grids directly: one string literal per pixel row.  The C file is
 * the single source of truth; tools/render_icons.py parses it, checks every
 * size, and renders docs/images/icon-sheet.png for review.  PC_ART_CHECK
 * below also rejects a wrongly sized grid at compile time.
 */
#include "icons.h"

#define PC_ART_CHECK(a, w, h) \
    extern char pc_art_check_##a[(sizeof(a) - 1u == (w) * (h) + 0u) ? 1 : -1]

/* ==================================================================== */
/* Tools */
/* ==================================================================== */

/* PC_ICON_TOOL_MOVE (16x16) */
static const char ico_tool_move[] =
    "k..............."
    "kk.............."
    "kkk............."
    "kkkk............"
    "kkkkk..........."
    "kkkkkk.........."
    "kkkkkkk........."
    "kkkkkkkk...k...."
    "kkk.kk....kkk..."
    "kk...kk....k...."
    "k....kk..k.k.k.."
    "......kkkkkkkkk."
    ".........k.k.k.."
    "...........k...."
    "..........kkk..."
    "...........k....";
PC_ART_CHECK(ico_tool_move, 16, 16);

/* PC_ICON_TOOL_MARQUEE_RECT (16x16) */
static const char ico_tool_marquee_rect[] =
    "................"
    "................"
    "................"
    ".kk.kk.kk.kk.kk."
    ".k............k."
    "................"
    ".k............k."
    ".k............k."
    "................"
    ".k............k."
    ".k............k."
    "................"
    ".k............k."
    ".kk.kk.kk.kk.kk."
    "................"
    "................";
PC_ART_CHECK(ico_tool_marquee_rect, 16, 16);

/* PC_ICON_TOOL_MARQUEE_ELLIPSE (16x16) */
static const char ico_tool_marquee_ellipse[] =
    "................"
    "................"
    "................"
    ".....kk..kk....."
    "...k........k..."
    "..k..........k.."
    "................"
    ".k............k."
    ".k............k."
    ".k............k."
    "................"
    "..k..........k.."
    "...k........k..."
    ".....kk..kk....."
    "................"
    "................";
PC_ART_CHECK(ico_tool_marquee_ellipse, 16, 16);

/* PC_ICON_TOOL_MARQUEE_ROW (16x16) */
static const char ico_tool_marquee_row[] =
    "................"
    "................"
    "................"
    "d.d.d.d.d.d.d.d."
    "................"
    "................"
    "kk.kk.kk.kk.kk.k"
    "k..............k"
    "kk.kk.kk.kk.kk.k"
    "................"
    "................"
    "d.d.d.d.d.d.d.d."
    "................"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_marquee_row, 16, 16);

/* PC_ICON_TOOL_MARQUEE_COL (16x16) */
static const char ico_tool_marquee_col[] =
    "...d..kkk..d...."
    "......k.k......."
    "...d.......d...."
    "......k.k......."
    "...d..k.k..d...."
    "................"
    "...d..k.k..d...."
    "......k.k......."
    "...d.......d...."
    "......k.k......."
    "...d..k.k..d...."
    "................"
    "...d..k.k..d...."
    "......k.k......."
    "...d.......d...."
    "......kkk.......";
PC_ART_CHECK(ico_tool_marquee_col, 16, 16);

/* PC_ICON_TOOL_LASSO (16x16) */
static const char ico_tool_lasso[] =
    "................"
    "......kkkkk....."
    "....kk.....kk..."
    "...k.........k.."
    "..k...........k."
    "..k...........k."
    "..k...........k."
    "...k.........k.."
    "....kk.....kk..."
    "....kkkkkkk....."
    "...k.k.........."
    "...k..k........."
    "...k............"
    "....k..........."
    "....kk.........."
    "................";
PC_ART_CHECK(ico_tool_lasso, 16, 16);

/* PC_ICON_TOOL_LASSO_POLY (16x16) */
static const char ico_tool_lasso_poly[] =
    "................"
    "......kkkk......"
    ".....k....kkkk.."
    "....k........k.."
    "...k.........k.."
    "..k...........k."
    "..k...........k."
    "...k.........kk."
    "...k.......kk..."
    "....kkkkkkk....."
    "...k.k.........."
    "...k..k........."
    "...k............"
    "....k..........."
    "....kk.........."
    "................";
PC_ART_CHECK(ico_tool_lasso_poly, 16, 16);

/* PC_ICON_TOOL_LASSO_MAGNETIC (16x16) */
static const char ico_tool_lasso_magnetic[] =
    "................"
    "....kkkkk......."
    "..kk.....kk....."
    ".k.........k...."
    ".k.........k...."
    ".k.........k...."
    "..kk.....kk....."
    "...kkkkkk......."
    "..kk............"
    ".k.k......kkkkk."
    ".k..k....kRRRRRk"
    ".k.......kRkkkRk"
    "..kk.....kRk.kRk"
    ".........kwk.kwk"
    ".........kkk.kkk"
    "................";
PC_ART_CHECK(ico_tool_lasso_magnetic, 16, 16);

/* PC_ICON_TOOL_WAND (16x16) */
static const char ico_tool_wand[] =
    "...........k...."
    "...........k...."
    "..........kYk..."
    "........kkYwYkk."
    "..........kYk..."
    ".......k...k..k."
    ".........k.k...."
    ".......kk......."
    "......kwk.k....."
    ".....kkk........"
    "....kkk........."
    "...kkk.........."
    "..kkk..........."
    ".kkk............"
    ".kk............."
    "................";
PC_ART_CHECK(ico_tool_wand, 16, 16);

/* PC_ICON_TOOL_CROP (16x16) */
static const char ico_tool_crop[] =
    "...kk..........."
    "...kk..........."
    "...kk..........."
    "kkkkkkkkkkkkk..."
    "kkkkkkkkkkkkk..."
    "...kk......kk..."
    "...kk......kk..."
    "...kk......kk..."
    "...kk......kk..."
    "...kk......kk..."
    "...kk......kk..."
    "...kkkkkkkkkkkkk"
    "...kkkkkkkkkkkkk"
    "...........kk..."
    "...........kk..."
    "...........kk...";
PC_ART_CHECK(ico_tool_crop, 16, 16);

/* PC_ICON_TOOL_EYEDROPPER (16x16) */
static const char ico_tool_eyedropper[] =
    "................"
    "...........kkk.."
    "..........kkkkk."
    "..........kkkkk."
    "........k.kkkkk."
    ".........kkkkk.."
    "........kwkkk.k."
    ".......kwwwk...."
    "......kwwwk.k..."
    ".....kwwwk......"
    "....kwwwk......."
    "...kwwwk........"
    "...kwwk........."
    "..kkkk.........."
    ".k.............."
    "................";
PC_ART_CHECK(ico_tool_eyedropper, 16, 16);

/* PC_ICON_TOOL_MEASURE (16x16) */
static const char ico_tool_measure[] =
    ".............k.."
    "............kYk."
    "...........kYYkk"
    "..........kYkYk."
    ".........kYYkk.."
    "........kYYYk..."
    ".......kYYkk...."
    "......kYkYk....."
    ".....kYYkk......"
    "....kYYYk......."
    "...kYYkk........"
    "..kYkYk........."
    ".kYYkk.........."
    "kYYYk..........."
    ".kkk............"
    "..k.............";
PC_ART_CHECK(ico_tool_measure, 16, 16);

/* PC_ICON_TOOL_BRUSH (16x16) */
static const char ico_tool_brush[] =
    "................"
    "..............k."
    ".............kmk"
    "............kmRk"
    "...........kmRk."
    "..........kmRk.."
    ".........kmRk..."
    "........kgwk...."
    ".......kdgwk...."
    "......kkdgk....."
    ".....kkkkk......"
    "....kkkkk......."
    "...kkkkk........"
    "..kkkk.........."
    "..kkk..........."
    ".kk.............";
PC_ART_CHECK(ico_tool_brush, 16, 16);

/* PC_ICON_TOOL_AIRBRUSH (16x16) */
static const char ico_tool_airbrush[] =
    "................"
    "................"
    ".........kkkk..."
    ".........kBBk..."
    ".........kkkk..."
    "..........kk...."
    "k....kkkkkkkkkk."
    "...kkwwwwwwwwwwk"
    ".k.kkddddddddddk"
    ".....kkkkkkkkkkk"
    "k.k.......kk..k."
    "..........kk..k."
    ".k.............k"
    "...............k"
    "..............k."
    "................";
PC_ART_CHECK(ico_tool_airbrush, 16, 16);

/* PC_ICON_TOOL_PENCIL (16x16) */
static const char ico_tool_pencil[] =
    "................"
    "............kk.."
    "...........kRRk."
    "..........kwRmk."
    ".........kwgdk.."
    "........kYYdk..."
    ".......kYYok...."
    "......kYYok....."
    ".....kYYok......"
    "...kkYYok......."
    "...kwYok........"
    "..kwwgk........."
    "..kwgkk........."
    "..kkk..........."
    ".k.............."
    "................";
PC_ART_CHECK(ico_tool_pencil, 16, 16);

/* PC_ICON_TOOL_STAMP (16x16) */
static const char ico_tool_stamp[] =
    "................"
    "......kkkk......"
    ".....kYYYok....."
    ".....kYYook....."
    "......kook......"
    ".......kk......."
    "......kYok......"
    "......kYok......"
    "..kkkkkkkkkkkk.."
    ".kwwwwwwwwwwwwk."
    ".kgggggggggggdk."
    ".kkkkkkkkkkkkkk."
    "..kmmmmmmmmmmk.."
    "..kkkkkkkkkkkk.."
    "................"
    "................";
PC_ART_CHECK(ico_tool_stamp, 16, 16);

/* PC_ICON_TOOL_PATTERN_STAMP (16x16) */
static const char ico_tool_pattern_stamp[] =
    "......kkkk......"
    ".....kYYYok....."
    ".....kYYook....."
    "......kook......"
    ".......kk......."
    "......kYok......"
    "..kkkkkkkkkkkk.."
    ".kwwwwwwwwwwwwk."
    ".kgggggggggggdk."
    ".kkkkkkkkkkkkkk."
    "..kmmmmmmmmmmk.."
    "..kkkkkkkkkkkk.."
    "................"
    ".kk..kk..kk..kk."
    "...kk..kk..kk..."
    ".kk..kk..kk..kk.";
PC_ART_CHECK(ico_tool_pattern_stamp, 16, 16);

/* PC_ICON_TOOL_HISTORY_BRUSH (16x16) */
static const char ico_tool_history_brush[] =
    "..kkk..........."
    ".k...k........k."
    "k.....k......kmk"
    "k...kkkkk...kmRk"
    "k....kkk...kmRk."
    ".k....k...kmRk.."
    "..........kgk..."
    ".........kdgk..."
    "........kkdk...."
    ".......kkkk....."
    "......kkkk......"
    ".....kkkk......."
    "....kkkk........"
    "....kkk........."
    "...kk..........."
    "................";
PC_ART_CHECK(ico_tool_history_brush, 16, 16);

/* PC_ICON_TOOL_ERASER (16x16) */
static const char ico_tool_eraser[] =
    "................"
    "................"
    "................"
    "......kkkkkkkkk."
    ".....kwwwwwwwwkk"
    "....kwwwwwwwwkdk"
    "...kwwwwwwwwkddk"
    "..kwwwwwwwwkdddk"
    ".kkkkkkkkkkddddk"
    ".kwwwwwwwwkddddk"
    ".kwwwwwwwwkdddk."
    ".kwwwwwwwwkddk.."
    ".kwwwwwwwwkdk..."
    ".kwwwwwwwwkk...."
    ".kkkkkkkkkk....."
    "................";
PC_ART_CHECK(ico_tool_eraser, 16, 16);

/* PC_ICON_TOOL_GRADIENT (16x16) */
static const char ico_tool_gradient[] =
    "................"
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "kkkkdkddgdgggwgk"
    "kkkdkddgdggwgwwk"
    "kkkkdkddgdgggwgk"
    "kkkdkddgdggwgwwk"
    "kkkkdkddgdgggwgk"
    "kkkdkddgdggwgwwk"
    "kkkkdkddgdgggwgk"
    "kkkdkddgdggwgwwk"
    "kkkkkkkkkkkkkkkk"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_gradient, 16, 16);

/* PC_ICON_TOOL_BUCKET (16x16) */
static const char ico_tool_bucket[] =
    ".........kkk...."
    "........k...k..."
    ".......kkk...k.."
    "......kwwwk..k.."
    ".....kwwwwwk.k.."
    "....kkwwwwwwkk.."
    "...kBBkwwwwwdk.."
    "..kBBBBkwwwwdk.."
    "..kBkBBkwwwddk.."
    "..kBkkBkwwddk..."
    "..kBk.kkkddk...."
    "..kBk...kkk....."
    "..kBk..........."
    "..kBk..........."
    "...k............"
    "................";
PC_ART_CHECK(ico_tool_bucket, 16, 16);

/* PC_ICON_TOOL_BLUR (16x16) */
static const char ico_tool_blur[] =
    "................"
    ".......kk......."
    ".......kk......."
    "......kwwk......"
    "......kwwk......"
    ".....kwwwdk....."
    ".....kwwwdk....."
    "....kwwwwwdk...."
    "...kwwwwwwwdk..."
    "...kwwwwwwwdk..."
    "..kwwwwwwwwwdk.."
    "..kwwwwwwwwwdk.."
    "..kwwwwwwwwddk.."
    "...kwwwwwwddk..."
    "....kkdddddk...."
    "......kkkk......";
PC_ART_CHECK(ico_tool_blur, 16, 16);

/* PC_ICON_TOOL_SHARPEN (16x16) */
static const char ico_tool_sharpen[] =
    "................"
    ".......kk......."
    ".......kk......."
    "......kwdk......"
    "......kwdk......"
    ".....kwwddk....."
    ".....kwwddk....."
    "....kwwwdddk...."
    "....kwwwdddk...."
    "...kwwwwddddk..."
    "...kwwwwddddk..."
    "..kwwwwwdddddk.."
    "..kwwwwwdddddk.."
    ".kwwwwwwddddddk."
    ".kkkkkkkkkkkkkk."
    "................";
PC_ART_CHECK(ico_tool_sharpen, 16, 16);

/* PC_ICON_TOOL_SMUDGE (16x16) */
static const char ico_tool_smudge[] =
    "................"
    "..........kkkk.."
    ".........kwwwwk."
    "........kwkkwwwk"
    ".......kwwwkwwwk"
    "......kwwwkkwwwk"
    ".....kwwwk.kwwk."
    "....kwwwk..kwwk."
    "...kwwwk..kwwk.."
    "..kwwwk...kkk..."
    ".kwwwk.........."
    ".kwwk..........."
    "..kk............"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_smudge, 16, 16);

/* PC_ICON_TOOL_DODGE (16x16) */
static const char ico_tool_dodge[] =
    "................"
    ".........kkkk..."
    "........kkkkkk.."
    ".......kkwkkkkk."
    ".......kwkkkkkk."
    ".......kkkkkkkk."
    ".......kkkkkkkk."
    "........kkkkkk.."
    ".......k.kkkk..."
    "......kk........"
    ".....kk........."
    "....kk.........."
    "...kk..........."
    "..kk............"
    ".kk............."
    "................";
PC_ART_CHECK(ico_tool_dodge, 16, 16);

/* PC_ICON_TOOL_BURN (16x16) */
static const char ico_tool_burn[] =
    "................"
    "....kkkkk......."
    "...kwwwwwkk....."
    "..kwwkkkwwwk...."
    "..kwk...kwwwk..."
    "..kwk...kwwwwk.."
    "..kwwkkkwkwwwk.."
    "...kwwwwkwkwwk.."
    "...kkkkkwwkwwk.."
    "..kwwwwwkwkwwk.."
    "..kkkkkkwwkwk..."
    "..kwwwwwwkwwk..."
    "...kkkkkkwwk...."
    "........kwwk...."
    "........kkkk...."
    "................";
PC_ART_CHECK(ico_tool_burn, 16, 16);

/* PC_ICON_TOOL_SPONGE (16x16) */
static const char ico_tool_sponge[] =
    "................"
    "................"
    "...kkkkkkkkkk..."
    "..kYYYYYYYYYYk.."
    ".kYYoYYYYYoYYok."
    ".kYYYYYoYYYYYok."
    ".kYoYYYYYYYoYok."
    ".kYYYYoYYYYYYok."
    ".kYYYYYYYoYYYok."
    ".kYYoYYYYYYYYok."
    ".kYYYYYYYoYYook."
    "..kYYYoYYYYYok.."
    "...kkkkkkkkkk..."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_sponge, 16, 16);

/* PC_ICON_TOOL_PEN (16x16) */
static const char ico_tool_pen[] =
    "................"
    "....kkkkkkk....."
    "....kkkkkkk....."
    "...kwwwkdddk...."
    "...kwwwkdddk...."
    "...kwwk.kddk...."
    "...kwwwkdddk...."
    "....kwwkddk....."
    "....kwwkddk....."
    ".....kwkdk......"
    ".....kwkdk......"
    "......kkk......."
    ".......k........"
    ".......k........"
    "................"
    "................";
PC_ART_CHECK(ico_tool_pen, 16, 16);

/* PC_ICON_TOOL_PEN_FREEFORM (16x16) */
static const char ico_tool_pen_freeform[] =
    "..kkkkkkk......."
    "..kkkkkkk......."
    ".kwwwkdddk......"
    ".kwwwkdddk......"
    ".kwwk.kddk......"
    ".kwwwkdddk......"
    "..kwwkddk......."
    "..kwwkddk......."
    "...kwkdk........"
    "...kwkdk........"
    "....kkk........."
    ".....k.....kk..."
    ".....k....k..k.."
    "......k..k....kk"
    ".......kk......."
    "................";
PC_ART_CHECK(ico_tool_pen_freeform, 16, 16);

/* PC_ICON_TOOL_PEN_MAGNETIC (16x16) */
static const char ico_tool_pen_magnetic[] =
    "..kkkkkkk......."
    "..kkkkkkk......."
    ".kwwwkdddk......"
    ".kwwwkdddk......"
    ".kwwk.kddk......"
    ".kwwwkdddk......"
    "..kwwkddk......."
    "..kwwkddk......."
    "...kwkdk........"
    "...kwkdk..kkkkk."
    "....kkk..kRRRRRk"
    ".....k...kRkkkRk"
    ".....k...kRk.kRk"
    ".........kwk.kwk"
    ".........kkk.kkk"
    "................";
PC_ART_CHECK(ico_tool_pen_magnetic, 16, 16);

/* PC_ICON_TOOL_PEN_ADD (16x16) */
static const char ico_tool_pen_add[] =
    "..kkkkkkk......."
    "..kkkkkkk......."
    ".kwwwkdddk......"
    ".kwwwkdddk......"
    ".kwwk.kddk......"
    ".kwwwkdddk......"
    "..kwwkddk......."
    "..kwwkddk......."
    "...kwkdk........"
    "...kwkdk........"
    "....kkk.....k..."
    ".....k......k..."
    ".....k....kkkkk."
    "............k..."
    "............k..."
    "................";
PC_ART_CHECK(ico_tool_pen_add, 16, 16);

/* PC_ICON_TOOL_PEN_DELETE (16x16) */
static const char ico_tool_pen_delete[] =
    "..kkkkkkk......."
    "..kkkkkkk......."
    ".kwwwkdddk......"
    ".kwwwkdddk......"
    ".kwwk.kddk......"
    ".kwwwkdddk......"
    "..kwwkddk......."
    "..kwwkddk......."
    "...kwkdk........"
    "...kwkdk........"
    "....kkk........."
    ".....k.........."
    ".....k....kkkkk."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_pen_delete, 16, 16);

/* PC_ICON_TOOL_PEN_CONVERT (16x16) */
static const char ico_tool_pen_convert[] =
    "................"
    "................"
    ".......kkk......"
    ".......kwk......"
    ".......kkk......"
    "......k...k....."
    "......k...k....."
    ".....k.....k...."
    ".....k.....k...."
    "....k.......k..."
    "....k.......k..."
    "...k.........k.."
    "...k.........k.."
    "..k...........k."
    "..k...........k."
    "................";
PC_ART_CHECK(ico_tool_pen_convert, 16, 16);

/* PC_ICON_TOOL_DIRECT_SELECT (16x16) */
static const char ico_tool_direct_select[] =
    "....k..........."
    "....kk.........."
    "....kwk........."
    "....kwwk........"
    "....kwwwk......."
    "....kwwwwk......"
    "....kwwwwwk....."
    "....kwwwwwwk...."
    "....kwwwwwwwk..."
    "....kwwwwkkkkk.."
    "....kwwkwwk....."
    "....kwk.kwwk...."
    "....kk...kwwk..."
    "....k.....kwwk.."
    "...........kk..."
    "................";
PC_ART_CHECK(ico_tool_direct_select, 16, 16);

/* PC_ICON_TOOL_PATH_SELECT (16x16) */
static const char ico_tool_path_select[] =
    "....k..........."
    "....kk.........."
    "....kkk........."
    "....kkkk........"
    "....kkkkk......."
    "....kkkkkk......"
    "....kkkkkkk....."
    "....kkkkkkkk...."
    "....kkkkkkkkk..."
    "....kkkkkkkkkk.."
    "....kkkkkkk....."
    "....kkk.kkkk...."
    "....kk...kkkk..."
    "....k.....kkkk.."
    "...........kk..."
    "................";
PC_ART_CHECK(ico_tool_path_select, 16, 16);

/* PC_ICON_TOOL_TYPE (16x16) */
static const char ico_tool_type[] =
    "................"
    "..kkkkkkkkkkkk.."
    "..kk...kk...kk.."
    "..k....kk....k.."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".......kk......."
    ".....kkkkkk....."
    "................"
    "................";
PC_ART_CHECK(ico_tool_type, 16, 16);

/* PC_ICON_TOOL_TYPE_VERTICAL (16x16) */
static const char ico_tool_type_vertical[] =
    "................"
    "kkkkkkkkkk......"
    "kk..kk..kk...k.."
    "k...kk...k...k.."
    "....kk.......k.."
    "....kk.......k.."
    "....kk.......k.."
    "....kk.......k.."
    "....kk.......k.."
    "....kk.......k.."
    "..kkkkkk.....k.."
    "...........kkkkk"
    "............kkk."
    ".............k.."
    "................"
    "................";
PC_ART_CHECK(ico_tool_type_vertical, 16, 16);

/* PC_ICON_TOOL_TYPE_MASK (16x16) */
static const char ico_tool_type_mask[] =
    "................"
    ".k.k.k.k.k.k.k.."
    "..............k."
    ".k.............."
    "..k.k.....k.k.k."
    "......k........."
    ".........k......"
    "......k........."
    ".........k......"
    "......k........."
    ".........k......"
    "......k........."
    ".........k......"
    "......k........."
    ".......k.k......"
    "................";
PC_ART_CHECK(ico_tool_type_mask, 16, 16);

/* PC_ICON_TOOL_TYPE_MASK_VERTICAL (16x16) */
static const char ico_tool_type_mask_vertical[] =
    "................"
    "k.k.k.k.k.k....."
    "................"
    "k.k.....k.k..k.."
    "....k.k......k.."
    ".............k.."
    "....k.k......k.."
    ".............k.."
    "....k.k......k.."
    ".............k.."
    "....k.k......k.."
    "...........kkkkk"
    "....k.k.....kkk."
    ".............k.."
    "................"
    "................";
PC_ART_CHECK(ico_tool_type_mask_vertical, 16, 16);

/* PC_ICON_TOOL_SHAPE_RECT (16x16) */
static const char ico_tool_shape_rect[] =
    "................"
    "................"
    "................"
    ".kkkkkkkkkkkkkk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kkkkkkkkkkkkkk."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_tool_shape_rect, 16, 16);

/* PC_ICON_TOOL_SHAPE_ELLIPSE (16x16) */
static const char ico_tool_shape_ellipse[] =
    "................"
    "................"
    ".....kkkkkk....."
    "...kkddddddkk..."
    "..kddddddddddk.."
    "..kddddddddddk.."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    ".kddddddddddddk."
    "..kddddddddddk.."
    "..kddddddddddk.."
    "...kkddddddkk..."
    ".....kkkkkk....."
    "................"
    "................";
PC_ART_CHECK(ico_tool_shape_ellipse, 16, 16);

/* PC_ICON_TOOL_SHAPE_LINE (16x16) */
static const char ico_tool_shape_line[] =
    "................"
    "................"
    "............kkk."
    "...........kkkk."
    "..........kkkk.."
    ".........kkk...."
    "........kkk....."
    ".......kkk......"
    "......kkk......."
    ".....kkk........"
    "....kkk........."
    "..kkkk.........."
    ".kkkk..........."
    ".kkk............"
    "................"
    "................";
PC_ART_CHECK(ico_tool_shape_line, 16, 16);

/* PC_ICON_TOOL_HAND (16x16) */
static const char ico_tool_hand[] =
    ".......kk......."
    "......kwwkkk...."
    "....kkkwwkwwk..."
    "...kwwkwwkwwkkk."
    "...kwwkwwkwwkwwk"
    "...kwwkwwkwwkwwk"
    "...kwwkwwkwwkwwk"
    "kk.kwwkwwkwwkwwk"
    "kwkkwwwwwwwwwwwk"
    "kwwkwwwwwwwwwwwk"
    ".kwwwwwwwwwwwwwk"
    "..kwwwwwwwwwwwk."
    "...kwwwwwwwwwk.."
    "....kwwwwwwwk..."
    "....kwwwwwwwk..."
    "....kkkkkkkkk...";
PC_ART_CHECK(ico_tool_hand, 16, 16);

/* PC_ICON_TOOL_ZOOM (16x16) */
static const char ico_tool_zoom[] =
    "................"
    "....kkk........."
    "..kkwwwkk......."
    ".kwwwwwwwk......"
    ".kwwwwwwwk......"
    "kwwwwwwwwwk....."
    "kwwwwwwwwwk....."
    "kwwwwwwwwwk....."
    ".kwwwwwwwk......"
    ".kwwwwwwwkk....."
    "..kkwwwkkkkk...."
    "....kkk..kkkk..."
    "..........kkkk.."
    "...........kkkk."
    "............kkk."
    "................";
PC_ART_CHECK(ico_tool_zoom, 16, 16);

/* ==================================================================== */
/* Toolbox extras */
/* ==================================================================== */

/* PC_ICON_DEFAULT_COLORS (10x10) */
static const char ico_default_colors[] =
    "kkkkkk...."
    "kkkkkk...."
    "kkkkkk...."
    "kkkkkk...."
    "kkkkkkkkkk"
    "kkkkkkwwwk"
    "....kwwwwk"
    "....kwwwwk"
    "....kwwwwk"
    "....kkkkkk";
PC_ART_CHECK(ico_default_colors, 10, 10);

/* PC_ICON_SWAP_COLORS (10x10) */
static const char ico_swap_colors[] =
    "..k......."
    ".kk......."
    "kkkkkk...."
    ".kk...k..."
    "..k....k.."
    ".......k.."
    ".......k.."
    ".....kkkkk"
    "......kkk."
    ".......k..";
PC_ART_CHECK(ico_swap_colors, 10, 10);

/* PC_ICON_STANDARD_MODE (16x16) */
static const char ico_standard_mode[] =
    "................"
    ".kkkkkkkkkkkkkk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwkkkkwwwwk."
    ".kwwwkddddkwwwk."
    ".kwwkddddddkwwk."
    ".kwwkddddddkwwk."
    ".kwwkddddddkwwk."
    ".kwwkddddddkwwk."
    ".kwwwkddddkwwwk."
    ".kwwwwkkkkwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kkkkkkkkkkkkkk."
    "................";
PC_ART_CHECK(ico_standard_mode, 16, 16);

/* PC_ICON_QUICKMASK_MODE (16x16) */
static const char ico_quickmask_mode[] =
    "................"
    ".kkkkkkkkkkkkkk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwkkkkwwwwk."
    ".kwwwkwRwRkwwwk."
    ".kwwkwRwRwRkwwk."
    ".kwwkRwRwRwkwwk."
    ".kwwkwRwRwRkwwk."
    ".kwwkRwRwRwkwwk."
    ".kwwwkRwRwkwwwk."
    ".kwwwwkkkkwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kkkkkkkkkkkkkk."
    "................";
PC_ART_CHECK(ico_quickmask_mode, 16, 16);

/* PC_ICON_SCREEN_STANDARD (16x16) */
static const char ico_screen_standard[] =
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "knnnnnnnnnnnnnnk"
    "kkkkkkkkkkkkkkkk"
    "kddddddddddddddk"
    "kddkkkkkkkkkdddk"
    "kddkddddddddkddk"
    "kddkwwwwwwwwkddk"
    "kddkwwwwwwwwkddk"
    "kddkwwwwwwwwkddk"
    "kddkwwwwwwwwkddk"
    "kddkkkkkkkkkkddk"
    "kddddddddddddddk"
    "kkkkkkkkkkkkkkkk"
    "................";
PC_ART_CHECK(ico_screen_standard, 16, 16);

/* PC_ICON_SCREEN_FULL_MENU (16x16) */
static const char ico_screen_full_menu[] =
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "kwkkwkkwkkwwwwwk"
    "kkkkkkkkkkkkkkkk"
    "kddddddddddddddk"
    "kddddddddddddddk"
    "kddwwwwwwwwwwddk"
    "kddwwwwwwwwwwddk"
    "kddwwwwwwwwwwddk"
    "kddwwwwwwwwwwddk"
    "kddwwwwwwwwwwddk"
    "kddddddddddddddk"
    "kddddddddddddddk"
    "kkkkkkkkkkkkkkkk"
    "................";
PC_ART_CHECK(ico_screen_full_menu, 16, 16);

/* PC_ICON_SCREEN_FULL (16x16) */
static const char ico_screen_full[] =
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkwwwwwwwwwwkkk"
    "kkkwwwwwwwwwwkkk"
    "kkkwwwwwwwwwwkkk"
    "kkkwwwwwwwwwwkkk"
    "kkkwwwwwwwwwwkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "kkkkkkkkkkkkkkkk"
    "................";
PC_ART_CHECK(ico_screen_full, 16, 16);

/* PC_ICON_FLYOUT (4x4) */
static const char ico_flyout[] =
    "...k"
    "..kk"
    ".kkk"
    "kkkk";
PC_ART_CHECK(ico_flyout, 4, 4);

/* ==================================================================== */
/* Panels and dock */
/* ==================================================================== */

/* PC_ICON_PANEL_MENU (10x9) */
static const char ico_panel_menu[] =
    "......kkkk"
    ".........."
    "......kkkk"
    "kkkkk....."
    ".kkk..kkkk"
    "..k......."
    "......kkkk"
    ".........."
    "......kkkk";
PC_ART_CHECK(ico_panel_menu, 10, 9);

/* PC_ICON_CHEVRON_LEFT (8x7) */
static const char ico_chevron_left[] =
    "...k...k"
    "..kk..kk"
    ".kk..kk."
    "kk..kk.."
    ".kk..kk."
    "..kk..kk"
    "...k...k";
PC_ART_CHECK(ico_chevron_left, 8, 7);

/* PC_ICON_CHEVRON_RIGHT (8x7) */
static const char ico_chevron_right[] =
    "k...k..."
    "kk..kk.."
    ".kk..kk."
    "..kk..kk"
    ".kk..kk."
    "kk..kk.."
    "k...k...";
PC_ART_CHECK(ico_chevron_right, 8, 7);

/* PC_ICON_PANEL_NAVIGATOR (16x16) */
static const char ico_panel_navigator[] =
    "................"
    "kkkkkkkkkkkkkkkk"
    "kCCCCCCCCCCCCCCk"
    "kCCCCCCCCCCYYCCk"
    "kCCRRRRRRRCYYCCk"
    "kCCRCCCCCRCCCCCk"
    "kCCRCCkCCRCCCCCk"
    "kCCRCkGkCRCkCCCk"
    "kCCRkGGGkRkGkCCk"
    "kCCRRRRRRRGGGkCk"
    "kCkGGGGGGGGGGGkk"
    "kGGGGGGGGGGGGGGk"
    "kGGGGGGGGGGGGGGk"
    "kkkkkkkkkkkkkkkk"
    "................"
    "................";
PC_ART_CHECK(ico_panel_navigator, 16, 16);

/* PC_ICON_PANEL_INFO (16x16) */
static const char ico_panel_info[] =
    "................"
    ".kkkkkkkkkkkkkk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwnnnwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwwnnwwwwwk."
    ".kwwwwnnnnwwwwk."
    ".kwwwwwwwwwwwwk."
    ".kkkkkkkkkkkkkk."
    "................"
    "................";
PC_ART_CHECK(ico_panel_info, 16, 16);

/* PC_ICON_PANEL_COLOR (16x16) */
static const char ico_panel_color[] =
    "................"
    ".kkkkkkkkkkkkkk."
    ".kRRRRRRRRRRRRk."
    ".kkkkkkkkkkkkkk."
    ".....k.........."
    "....kkk........."
    ".kkkkkkkkkkkkkk."
    ".kLLLLLLLLLLLLk."
    ".kkkkkkkkkkkkkk."
    ".........k......"
    "........kkk....."
    ".kkkkkkkkkkkkkk."
    ".kBBBBBBBBBBBBk."
    ".kkkkkkkkkkkkkk."
    "...k............"
    "..kkk...........";
PC_ART_CHECK(ico_panel_color, 16, 16);

/* PC_ICON_PANEL_SWATCHES (16x16) */
static const char ico_panel_swatches[] =
    "................"
    "kkkkkkkkkkkkkkkk"
    "kRRRkYYYkLLLkCCk"
    "kRRRkYYYkLLLkCCk"
    "kRRRkYYYkLLLkCCk"
    "kkkkkkkkkkkkkkkk"
    "kBBBkFFFkwwwkddk"
    "kBBBkFFFkwwwkddk"
    "kBBBkFFFkwwwkddk"
    "kkkkkkkkkkkkkkkk"
    "kmmmkoooknnnkkkk"
    "kmmmkoooknnnkkkk"
    "kmmmkoooknnnkkkk"
    "kkkkkkkkkkkkkkkk"
    "................"
    "................";
PC_ART_CHECK(ico_panel_swatches, 16, 16);

/* PC_ICON_PANEL_BRUSHES (16x16) */
static const char ico_panel_brushes[] =
    "................"
    "................"
    "............kkk."
    ".......kk..kkkkk"
    "...kk.kkkk.kkkkk"
    "kk.kk.kkkk.kkkkk"
    "kk.....kk...kkk."
    "................"
    "................"
    "...kkk.........."
    "..kkkkk.....kk.."
    ".kk...kk...kkkk."
    "kk.....kk.kk..kk"
    "........kkk....."
    ".........k......"
    "................";
PC_ART_CHECK(ico_panel_brushes, 16, 16);

/* PC_ICON_PANEL_LAYERS (16x16) */
static const char ico_panel_layers[] =
    "................"
    "................"
    "......kkkkkkkkk."
    "......kdddddddk."
    "....kkkkkkkkkdk."
    "....kgggggggkdk."
    "..kkkkkkkkkgkdk."
    "..kwwwwwwwkgkdk."
    "..kwwwwwwwkgkkk."
    "..kwwwwwwwkgk..."
    "..kwwwwwwwkkk..."
    "..kwwwwwwwk....."
    "..kkkkkkkkk....."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_panel_layers, 16, 16);

/* PC_ICON_PANEL_CHANNELS (16x16) */
static const char ico_panel_channels[] =
    "................"
    "......kkkkk....."
    ".....kRRRRRk...."
    "....kRRRRRRRk..."
    "....kRRRRRRRk..."
    "....kRRRRRRRk..."
    "...kYYYYFFFFk..."
    "..kLYYYwwFFFFk.."
    ".kLLLYwwwwFFBBk."
    ".kLLLLwwwwFBBBk."
    ".kLLLLCCCCBBBBk."
    ".kLLLLCCCCBBBBk."
    ".kLLLLCCCCBBBBk."
    "..kLLLLCCBBBBk.."
    "...kkkkkkkkkk..."
    "................";
PC_ART_CHECK(ico_panel_channels, 16, 16);

/* PC_ICON_PANEL_PATHS (16x16) */
static const char ico_panel_paths[] =
    "................"
    "............kkk."
    "..........kkkwk."
    "........kk..kkk."
    "........k......."
    ".......kk......."
    "......kkk....k.."
    "......kwkkkkkkk."
    "......kkk....k.."
    "........k......."
    ".......kk......."
    ".......k........"
    ".kkk..kk........"
    ".kwkkk.........."
    ".kkk............"
    "................";
PC_ART_CHECK(ico_panel_paths, 16, 16);

/* PC_ICON_PANEL_HISTORY (16x16) */
static const char ico_panel_history[] =
    "................"
    ".....kkkkkk....."
    "...kkwwwwwwkk..."
    "..kwwwwkwwwwwk.."
    "..kwwwwkwwwwwk.."
    ".kwwwwwkwwwwwwk."
    ".kwwwwwkwwwwwwk."
    ".kwwwwwkkkkkwwk."
    ".kwwwwwwwwwwwwk."
    ".kwwwwwwwwwwwwk."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "...kkwwwwwwkk..."
    ".....kkkkkk....."
    "................"
    "................";
PC_ART_CHECK(ico_panel_history, 16, 16);

/* PC_ICON_PANEL_ACTIONS (16x16) */
static const char ico_panel_actions[] =
    "................"
    "................"
    ".k.............."
    ".kk...kkkkkkkkk."
    ".kkk............"
    ".kk...kkkkkkkkk."
    ".k.............."
    "................"
    "......kkkkkkkkk."
    "................"
    "......kkkkkkkkk."
    "................"
    "......kkkkkkkkk."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_panel_actions, 16, 16);

/* PC_ICON_PANEL_CHARACTER (16x16) */
static const char ico_panel_character[] =
    "................"
    "................"
    "................"
    "....kk.........."
    "....kk.........."
    "...k.kk........."
    "...k.kk...kkk..."
    "..k...kk.....kk."
    "..kkkkkk..kkkkk."
    ".k.....kk.k..kk."
    ".k.....kkkk..kk."
    "kkk...kkkk.kk.kk"
    "................"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_panel_character, 16, 16);

/* ==================================================================== */
/* Layers panel */
/* ==================================================================== */

/* PC_ICON_EYE (13x11) */
static const char ico_eye[] =
    "............."
    "....kkkkk...."
    "..kkwwwwwkk.."
    ".kwwwkkkwwwk."
    "kwwwkkwkkwwwk"
    "kwwwkkkkkwwwk"
    ".kwwwkkkwwwk."
    "..kkwwwwwkk.."
    "....kkkkk...."
    "............."
    ".............";
PC_ART_CHECK(ico_eye, 13, 11);

/* PC_ICON_BRUSH_SMALL (13x13) */
static const char ico_brush_small[] =
    "..........kk."
    ".........kRk."
    "........kRk.."
    ".......kRk..."
    "......kgk...."
    ".....kdgk...."
    "....kkdk....."
    "...kkkk......"
    "..kkkk......."
    "..kkk........"
    ".kk.........."
    ".k..........."
    ".............";
PC_ART_CHECK(ico_brush_small, 13, 13);

/* PC_ICON_MASK_SMALL (13x13) */
static const char ico_mask_small[] =
    "............."
    "............."
    "kkkkkkkkkkkkk"
    "kdddddddddddk"
    "kddddwwwddddk"
    "kdddwwwwwdddk"
    "kdddwwwwwdddk"
    "kdddwwwwwdddk"
    "kddddwwwddddk"
    "kdddddddddddk"
    "kkkkkkkkkkkkk"
    "............."
    ".............";
PC_ART_CHECK(ico_mask_small, 13, 13);

/* PC_ICON_LINK (7x13) */
static const char ico_link[] =
    "..kkk.."
    ".k...k."
    ".k...k."
    ".k...k."
    ".k.k.k."
    ".k.k.k."
    "..kkk.."
    ".k.k.k."
    ".k.k.k."
    ".k...k."
    ".k...k."
    ".k...k."
    "..kkk..";
PC_ART_CHECK(ico_link, 7, 13);

/* PC_ICON_LOCK_TRANSPARENT (13x13) */
static const char ico_lock_transparent[] =
    "............."
    "kkkkkkkkkkkkk"
    "kwwddwwddwwdk"
    "kwwddwwddwwdk"
    "kddwwddwwddwk"
    "kddwwddwwddwk"
    "kwwddwwddwwdk"
    "kwwddwwddwwdk"
    "kddwwddwwddwk"
    "kddwwddwwddwk"
    "kwwddwwddwwdk"
    "kkkkkkkkkkkkk"
    ".............";
PC_ART_CHECK(ico_lock_transparent, 13, 13);

/* PC_ICON_LOCK_POSITION (13x13) */
static const char ico_lock_position[] =
    "......k......"
    ".....kkk....."
    "....kkkkk...."
    "......k......"
    "..k...k...k.."
    ".kk...k...kk."
    "kkkkkkkkkkkkk"
    ".kk...k...kk."
    "..k...k...k.."
    "......k......"
    "....kkkkk...."
    ".....kkk....."
    "......k......";
PC_ART_CHECK(ico_lock_position, 13, 13);

/* PC_ICON_LOCK_ALL (13x13) */
static const char ico_lock_all[] =
    "....kkkkk...."
    "...kk...kk..."
    "...k.....k..."
    "...k.....k..."
    "...k.....k..."
    ".kkkkkkkkkkk."
    ".kYYYYYYYYok."
    ".kYYYkkkYYok."
    ".kYYYkkkYYok."
    ".kYYYYkYYYok."
    ".kYYYYkYYYok."
    ".koooooooook."
    ".kkkkkkkkkkk.";
PC_ART_CHECK(ico_lock_all, 13, 13);

/* PC_ICON_TEXT_LAYER (16x16) */
static const char ico_text_layer[] =
    "kkkkkkkkkkkkkkkk"
    "kwwwwwwwwwwwwwwk"
    "kwkkkkkkkkkkkkwk"
    "kwkkwwwkkwwwkkwk"
    "kwkwwwwkkwwwwkwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwwwkkwwwwwwk"
    "kwwwwkkkkkkwwwwk"
    "kwwwwwwwwwwwwwwk"
    "kwwwwwwwwwwwwwwk"
    "kkkkkkkkkkkkkkkk";
PC_ART_CHECK(ico_text_layer, 16, 16);

/* PC_ICON_FX (13x13) */
static const char ico_fx[] =
    "............."
    ".......kkk..."
    "......k...k.."
    "......k......"
    ".....kk......"
    "...kkkkkk...."
    ".....kk......"
    ".....kk......"
    "....kk......."
    "....kk......."
    "....kk......."
    ".k.kk........"
    "..kk.........";
PC_ART_CHECK(ico_fx, 13, 13);

/* PC_ICON_NEW_LAYER (16x16) */
static const char ico_new_layer[] =
    "..kkkkkkkk......"
    "..kwwwwwwkk....."
    "..kwwwwwwkwk...."
    "..kwwwwwwkwwk..."
    "..kwwwwwwkkkkk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kkkkkkkkkkkk.."
    "................";
PC_ART_CHECK(ico_new_layer, 16, 16);

/* PC_ICON_TRASH (16x16) */
static const char ico_trash[] =
    ".....kkkkkk....."
    "..kkkk....kkkk.."
    ".kwwwwwwwwwwwdk."
    ".kkkkkkkkkkkkkk."
    "..kwwwwwwwwwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwkwwkwwkwdk.."
    "..kwwwwwwwwwdk.."
    "...kkkkkkkkkk..."
    "................"
    "................";
PC_ART_CHECK(ico_trash, 16, 16);

/* PC_ICON_ADD_MASK (16x16) */
static const char ico_add_mask[] =
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "kddddddddddddddk"
    "kddddddddddddddk"
    "kddddkkkkkkddddk"
    "kdddkwwwwwwkdddk"
    "kdddkwwwwwwkdddk"
    "kdddkwwwwwwkdddk"
    "kdddkwwwwwwkdddk"
    "kddddkkkkkkddddk"
    "kddddddddddddddk"
    "kddddddddddddddk"
    "kkkkkkkkkkkkkkkk"
    "................"
    "................";
PC_ART_CHECK(ico_add_mask, 16, 16);

/* PC_ICON_NEW_GROUP (16x16) */
static const char ico_new_group[] =
    "................"
    "..kkkkk........."
    ".kwwwwwk........"
    ".kwYYYYwkkkkkkk."
    ".kwYYYYYwwwwwwk."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kooooooooooook."
    ".kkkkkkkkkkkkkk."
    "................"
    "................";
PC_ART_CHECK(ico_new_group, 16, 16);

/* PC_ICON_LAYER_STYLE (16x16) */
static const char ico_layer_style[] =
    "................"
    ".......kkk......"
    "......k...k....."
    "......k........."
    ".....kk........."
    "...kkkkkk......."
    ".....kk........."
    ".....kk..kk..kk."
    "....kk....kkkk.."
    "....kk.....kk..."
    "....kk....kkkk.."
    ".k.kk....kk..kk."
    "..kk............"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_layer_style, 16, 16);

/* PC_ICON_NEW_ADJUSTMENT (16x16) */
static const char ico_new_adjustment[] =
    "................"
    "................"
    "......kkkk......"
    "....kkkkwwkk...."
    "...kkkkkwwwwk..."
    "...kkkkkwwwwk..."
    "..kkkkkkwwwwwk.."
    "..kkkkkkwwwwwk.."
    "..kkkkkkwwwwwk.."
    "..kkkkkkwwwwwk.."
    "...kkkkkwwwwk..."
    "...kkkkkwwwwk..."
    "....kkkkwwkk...."
    "......kkkk......"
    "................"
    "................";
PC_ART_CHECK(ico_new_adjustment, 16, 16);

/* ==================================================================== */
/* Channels / Paths */
/* ==================================================================== */

/* PC_ICON_LOAD_SELECTION (16x16) */
static const char ico_load_selection[] =
    "................"
    "................"
    "......kk.k......"
    "....kk....k....."
    "............k..."
    "...k........k..."
    "..k............."
    ".............k.."
    "..k..........k.."
    "..k............."
    "............k..."
    "...k........k..."
    "....k.....k....."
    "......kk.k......"
    "................"
    "................";
PC_ART_CHECK(ico_load_selection, 16, 16);

/* PC_ICON_FILL_PATH (16x16) */
static const char ico_fill_path[] =
    "................"
    "................"
    "......kkkk......"
    "....kkddddkk...."
    "...kddddddddk..."
    "...kddddddddk..."
    "..kddddddddddk.."
    "..kddddddddddk.."
    "..kddddddddddk.."
    "..kddddddddddk.."
    "...kddddddddk..."
    "...kddddddddk..."
    "....kkddddkk...."
    "......kkkk......"
    "................"
    "................";
PC_ART_CHECK(ico_fill_path, 16, 16);

/* PC_ICON_STROKE_PATH (16x16) */
static const char ico_stroke_path[] =
    "................"
    "................"
    "......kkkk......"
    "....kkkkkkkk...."
    "...kkk....kkk..."
    "...kk......kk..."
    "..kk........kk.."
    "..kk........kk.."
    "..kk........kk.."
    "..kk........kk.."
    "...kk......kk..."
    "...kkk....kkk..."
    "....kkkkkkkk...."
    "......kkkk......"
    "................"
    "................";
PC_ART_CHECK(ico_stroke_path, 16, 16);

/* PC_ICON_PATH_TO_SELECTION (16x16) */
static const char ico_path_to_selection[] =
    "................"
    ".......kkk......"
    "......kkwk......"
    "....kk.kkkk....."
    "............k..."
    "...k........k..."
    "..k............."
    ".kkk........kkk."
    ".kwk........kwk."
    ".kkk........kkk."
    "............k..."
    "...k........k..."
    "....k..kkkk....."
    "......kkwk......"
    ".......kkk......"
    "................";
PC_ART_CHECK(ico_path_to_selection, 16, 16);

/* PC_ICON_SELECTION_TO_PATH (16x16) */
static const char ico_selection_to_path[] =
    "................"
    ".......kkk......"
    "......kkwk......"
    "....kk.kkkkk...."
    "...k........k..."
    "...k........k..."
    "..k..........k.."
    ".kkk........kkk."
    ".kwk........kwk."
    ".kkk........kkk."
    "...k........k..."
    "...k........k..."
    "....kk.kkkkk...."
    "......kkwk......"
    ".......kkk......"
    "................";
PC_ART_CHECK(ico_selection_to_path, 16, 16);

/* ==================================================================== */
/* History / Actions */
/* ==================================================================== */

/* PC_ICON_SNAPSHOT (16x16) */
static const char ico_snapshot[] =
    "................"
    "................"
    "................"
    "....kkkk....kk.."
    ".kkkwwwwkkkkkkk."
    ".kwwwwwwwwwwwdk."
    ".kwwwwkkkkwwwdk."
    ".kwwwkdddwkwwdk."
    ".kwwkddkkdwkwdk."
    ".kwwkdkwkddkwdk."
    ".kwwkdkkkddkwdk."
    ".kwwwkddddkwwdk."
    ".kwwwwkkkkwwwdk."
    ".kddddddddddddk."
    ".kkkkkkkkkkkkkk."
    "................";
PC_ART_CHECK(ico_snapshot, 16, 16);

/* PC_ICON_NEW_DOC_FROM_STATE (16x16) */
static const char ico_new_doc_from_state[] =
    "..kkkkkkkk......"
    "..kwwwwwwkk....."
    "..kwwwwwwkwk...."
    "..kwwwwwwkwwk..."
    "..kwwwwwwkkkkk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwkwwwwk.."
    "..kwwwwwkwwwwk.."
    "..kwwwkkkkkwwk.."
    "..kwwwwwkwwwwk.."
    "..kwwwwwkwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kwwwwwwwwwwk.."
    "..kkkkkkkkkkkk.."
    "................";
PC_ART_CHECK(ico_new_doc_from_state, 16, 16);

/* PC_ICON_HISTORY_SOURCE (13x13) */
static const char ico_history_source[] =
    ".kkk.......k."
    "k...k.....kRk"
    "k..kkk...kRk."
    "k...k...kRk.."
    ".k.....kgk..."
    "......kdgk..."
    ".....kkdk...."
    "....kkkk....."
    "...kkkk......"
    "...kkk......."
    "..kk........."
    ".k..........."
    ".............";
PC_ART_CHECK(ico_history_source, 13, 13);

/* PC_ICON_ACTION_STOP (16x16) */
static const char ico_action_stop[] =
    "................"
    "................"
    "................"
    "................"
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "....kkkkkkkk...."
    "................"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_action_stop, 16, 16);

/* PC_ICON_ACTION_RECORD (16x16) */
static const char ico_action_record[] =
    "................"
    "................"
    "................"
    "......kkkk......"
    "....kkRRRRkk...."
    "....kwwRRRRk...."
    "...kRwRRRRRRk..."
    "...kRRRRRRRRk..."
    "...kRRRRRRRRk..."
    "...kRRRRRRRRk..."
    "....kRRRRRRk...."
    "....kkRRRRkk...."
    "......kkkk......"
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_action_record, 16, 16);

/* PC_ICON_ACTION_PLAY (16x16) */
static const char ico_action_play[] =
    "................"
    "................"
    "................"
    ".....k.........."
    ".....kk........."
    ".....kkk........"
    ".....kkkk......."
    ".....kkkkk......"
    ".....kkkkkk....."
    ".....kkkkk......"
    ".....kkkk......."
    ".....kkk........"
    ".....kk........."
    ".....k.........."
    "................"
    "................";
PC_ART_CHECK(ico_action_play, 16, 16);

/* PC_ICON_CHECK (9x9) */
static const char ico_check[] =
    "........k"
    ".......kk"
    "......kkk"
    "k....kkk."
    "kk..kkk.."
    "kkkkkk..."
    ".kkkk...."
    "..kk....."
    "...k.....";
PC_ART_CHECK(ico_check, 9, 9);

/* PC_ICON_DIALOG_TOGGLE (11x9) */
static const char ico_dialog_toggle[] =
    "kkkkkkkkkkk"
    "knnnnnnnnnk"
    "kkkkkkkkkkk"
    "kwwwwwwwwwk"
    "kwkkkkkkkwk"
    "kwwwwwwwwwk"
    "kwwwwkkkkwk"
    "kwwwwwwwwwk"
    "kkkkkkkkkkk";
PC_ART_CHECK(ico_dialog_toggle, 11, 9);

/* ==================================================================== */
/* Navigator */
/* ==================================================================== */

/* PC_ICON_ZOOM_OUT_MOUNTAIN (12x9) */
static const char ico_zoom_out_mountain[] =
    "............"
    "............"
    "............"
    "....k......."
    "...kwk......"
    "..kdddk.k..."
    ".kdddddkdk.."
    "kdddddddddk."
    "kkkkkkkkkkk.";
PC_ART_CHECK(ico_zoom_out_mountain, 12, 9);

/* PC_ICON_ZOOM_IN_MOUNTAIN (16x11) */
static const char ico_zoom_in_mountain[] =
    "................"
    ".....k.........."
    "....kwk........."
    "...kwwwk........"
    "..kddwdk..k....."
    "..kddddkkwwk...."
    ".kdddddddddk...."
    ".kddddddddddkk.."
    "kddddddddddddddk"
    "kddddddddddddddk"
    "kkkkkkkkkkkkkkkk";
PC_ART_CHECK(ico_zoom_in_mountain, 16, 11);

/* ==================================================================== */
/* Window chrome and message boxes */
/* ==================================================================== */

/* PC_ICON_CAPTION_CLOSE (8x7) */
static const char ico_caption_close[] =
    "kk....kk"
    ".kk..kk."
    "..kkkk.."
    "...kk..."
    "..kkkk.."
    ".kk..kk."
    "kk....kk";
PC_ART_CHECK(ico_caption_close, 8, 7);

/* PC_ICON_CAPTION_MIN (6x2) */
static const char ico_caption_min[] =
    "kkkkkk"
    "kkkkkk";
PC_ART_CHECK(ico_caption_min, 6, 2);

/* PC_ICON_CAPTION_MAX (9x9) */
static const char ico_caption_max[] =
    "kkkkkkkkk"
    "kkkkkkkkk"
    "k.......k"
    "k.......k"
    "k.......k"
    "k.......k"
    "k.......k"
    "k.......k"
    "kkkkkkkkk";
PC_ART_CHECK(ico_caption_max, 9, 9);

/* PC_ICON_CAPTION_RESTORE (8x9) */
static const char ico_caption_restore[] =
    "..kkkkkk"
    "..kkkkkk"
    "..k....k"
    "kkkkkk.k"
    "kkkkkk.k"
    "k....kkk"
    "k....k.."
    "k....k.."
    "kkkkkk..";
PC_ART_CHECK(ico_caption_restore, 8, 9);

/* PC_ICON_ARROW_UP (7x4) */
static const char ico_arrow_up[] =
    "...k..."
    "..kkk.."
    ".kkkkk."
    "kkkkkkk";
PC_ART_CHECK(ico_arrow_up, 7, 4);

/* PC_ICON_ARROW_DOWN (7x4) */
static const char ico_arrow_down[] =
    "kkkkkkk"
    ".kkkkk."
    "..kkk.."
    "...k...";
PC_ART_CHECK(ico_arrow_down, 7, 4);

/* PC_ICON_ARROW_LEFT (4x7) */
static const char ico_arrow_left[] =
    "...k"
    "..kk"
    ".kkk"
    "kkkk"
    ".kkk"
    "..kk"
    "...k";
PC_ART_CHECK(ico_arrow_left, 4, 7);

/* PC_ICON_ARROW_RIGHT (4x7) */
static const char ico_arrow_right[] =
    "k..."
    "kk.."
    "kkk."
    "kkkk"
    "kkk."
    "kk.."
    "k...";
PC_ART_CHECK(ico_arrow_right, 4, 7);

/* PC_ICON_MENU_CHECK (7x7) */
static const char ico_menu_check[] =
    "......k"
    ".....kk"
    "k...kkk"
    "kk.kkk."
    "kkkkk.."
    ".kkk..."
    "..k....";
PC_ART_CHECK(ico_menu_check, 7, 7);

/* PC_ICON_MENU_RADIO (6x6) */
static const char ico_menu_radio[] =
    ".kkkk."
    "kkkkkk"
    "kkkkkk"
    "kkkkkk"
    "kkkkkk"
    ".kkkk.";
PC_ART_CHECK(ico_menu_radio, 6, 6);

/* PC_ICON_SIZE_GRIP (12x12) */
static const char ico_size_grip[] =
    "...........w"
    "..........wd"
    ".........wdd"
    "........wdd."
    ".......wdd.w"
    "......wdd.wd"
    ".....wdd.wdd"
    "....wdd.wdd."
    "...wdd.wdd.w"
    "..wdd.wdd.wd"
    ".wdd.wdd.wdd"
    "wdd.wdd.wdd.";
PC_ART_CHECK(ico_size_grip, 12, 12);

/* PC_ICON_WARNING (32x32) */
static const char ico_warning[] =
    "................................"
    "................................"
    "...............kk..............."
    "...............kk..............."
    "..............kYok.............."
    "..............kYok.............."
    ".............kYYYok............."
    ".............kYYYok............."
    "............kYYYYYok............"
    "............kYYkkkok............"
    "...........kYYkkkkkok..........."
    "...........kYYkkkkkok..........."
    "..........kYYYkkkkkYok.........."
    "..........kYYYkkkkkYok.........."
    ".........kYYYYYkkkYYYok........."
    "........kYYYYYYkkkYYYYok........"
    "........kYYYYYYkkkYYYYok........"
    ".......kYYYYYYYkkkYYYYYok......."
    ".......kYYYYYYYYkYYYYYYok......."
    "......kYYYYYYYYYkYYYYYYYok......"
    "......kYYYYYYYYYYYYYYYYYok......"
    ".....kYYYYYYYYYYYYYYYYYYYok....."
    ".....kYYYYYYYYYkkkYYYYYYYok....."
    "....kYYYYYYYYYkkkkkYYYYYYYok...."
    "....kYYYYYYYYYkkkkkYYYYYYYok...."
    "...kYYYYYYYYYYYkkkYYYYYYYYYok..."
    "...kYYYYYYYYYYYYYYYYYYYYYYYok..."
    "..kYYYYYYYYYYYYYoooooooooooook.."
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk.."
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(ico_warning, 32, 32);

/* PC_ICON_INFO (32x32) */
static const char ico_info[] =
    "................................"
    "............kkkkkkkk............"
    ".........kkkBBBBBBBBkkk........."
    "........kBBBBBBBBBBBBBBk........"
    "......kkBBBBBBBBBBBBBBBBkk......"
    ".....kBBBBBBBBBBBBBBBBBBBBk....."
    "....kBBBBBBBBBBBBBBBBBBBBBBk...."
    "....kBBBBBBBBBBwwwBBBBBBBBBk...."
    "...kBBBBBBBBBBwwwwwBBBBBBBnnk..."
    "..kBBBBBBBBBBBwwwwwBBBBBBBnnnk.."
    "..kBBBBBBBBBBBBwwwBBBBBBBBBnnk.."
    "..kBBBBBBBBBBBBBBBBBBBBBBBBnnk.."
    ".kBBBBBBBBBBBBBBBBBBBBBBBBBnnnk."
    ".kBBBBBBBBBBBwwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBBnnk."
    ".kBBBBBBBBBBBBwwwwBBBBBBBBBnnnk."
    "..kBBBBBBBBBBBwwwwBBBBBBBBBnnk.."
    "..kBBBBBBBBBBwwwwwwwBBBBBBBnnk.."
    "..kBBBBBBBBBBwwwwwwwBBBBBBnnnk.."
    "...kBBBBBBBBBBBBBBBBBBBBBnnnk..."
    "....kBBBBBBBBBBBBBBBBBBBnnnk...."
    "....kBBBBBBBBBBBBBBBBBBnnnnk...."
    ".....kBBnnBBBBBBBBBBBBnnnnk....."
    "......kknnnnnBBBBBBnnnnnkk......"
    "........knnnnnnnnnnnnnnk........"
    ".........kkknnnnnnnnkkk........."
    "............kkkkkkkk............"
    "................................";
PC_ART_CHECK(ico_info, 32, 32);

/* PC_ICON_QUESTION (32x32) */
static const char ico_question[] =
    "................................"
    "............kkkkkkkk............"
    ".........kkkwwwwwwwgkkk........."
    ".......kkwwwwwwwwwwwwwgkk......."
    ".....kkwwwwwwBBBBBwwwwwwgkk....."
    "....kwwwwwwwBBBBBBBnwwwwwwgk...."
    "...kwwwwwwwBBBnwwBBBnwwwwwwgk..."
    "...kwwwwwwwBBBnwwBBBnwwwwwwgk..."
    "..kwwwwwwwwwnnwwBBBBnwwwwwwwgk.."
    "..kwwwwwwwwwwwwBBBBnwwwwwwwwgk.."
    ".kwwwwwwwwwwwwBBBnwwwwwwwwwwwgk."
    ".kwwwwwwwwwwwwBBBnwwwwwwwwwwwgk."
    ".kwwwwwwwwwwwwBBBnwwwwwwwwwwwgk."
    ".kwwwwwwwwwwwwwnnnwwwwwwwwwwwgk."
    ".kwwwwwwwwwwwwwwwwwwwwwwwwwwwgk."
    ".kwwwwwwwwwwwwBBBnwwwwwwwwwwwgk."
    "..kwwwwwwwwwwwBBBnwwwwwwwwwwgk.."
    "..kwwwwwwwwwwwBBBnwwwwwwwwwwgk.."
    "...kwwwwwwwwwwwnnnwwwwwwwwwgk..."
    "...kwwwwwwwwwwwwwwwwwwwwwwwgk..."
    "....kwwwwwwwwwwwwwwwwwwwwggk...."
    ".....kwwwwwwwwwwwwwwwwwggkk....."
    "......kwwwwwwwwwwwwwgggkk......."
    "......kwwwwgggggggggkkk........."
    "......kwwggkkkkkkkkk............"
    "......kwwkk....................."
    "......kwk......................."
    ".....kwk........................"
    ".....kk........................."
    ".....k.........................."
    "................................"
    "................................";
PC_ART_CHECK(ico_question, 32, 32);

/* PC_ICON_ERROR (32x32) */
static const char ico_error[] =
    "................................"
    "............kkkkkkkk............"
    ".........kkkRRRRRRRRkkk........."
    "........kRRRRRRRRRRRRRRk........"
    "......kkRRRRRRRRRRRRRRRRkk......"
    ".....kRRRRRRRRRRRRRRRRRRRRk....."
    "....kRRRRRRRRRRRRRRRRRRRRRRk...."
    "....kRRRRRRRRRRRRRRRRRRRRRRk...."
    "...kRRRRRRRRRRRRRRRRRRRRRRmmk..."
    "..kRRRRRRRRRRRRRRRRRRRRRRRmmmk.."
    "..kRRRRRRRRRRRRRRRRRRRRRRRRmmk.."
    "..kRRRRRRRRwwRRRRRRwwRRRRRRmmk.."
    ".kRRRRRRRRRwwwRRRRwwwRRRRRRmmmk."
    ".kRRRRRRRRRRwwwRRwwwRRRRRRRRmmk."
    ".kRRRRRRRRRRRwwwwwwRRRRRRRRRmmk."
    ".kRRRRRRRRRRRRwwwwRRRRRRRRRRmmk."
    ".kRRRRRRRRRRRRwwwwRRRRRRRRRRmmk."
    ".kRRRRRRRRRRRwwwwwwRRRRRRRRRmmk."
    ".kRRRRRRRRRRwwwRRwwwRRRRRRRRmmk."
    ".kRRRRRRRRRwwwRRRRwwwRRRRRRmmmk."
    "..kRRRRRRRRwwRRRRRRwwRRRRRRmmk.."
    "..kRRRRRRRRRRRRRRRRRRRRRRRRmmk.."
    "..kRRRRRRRRRRRRRRRRRRRRRRRmmmk.."
    "...kRRRRRRRRRRRRRRRRRRRRRmmmk..."
    "....kRRRRRRRRRRRRRRRRRRRmmmk...."
    "....kRRRRRRRRRRRRRRRRRRmmmmk...."
    ".....kRRmmRRRRRRRRRRRRmmmmk....."
    "......kkmmmmmRRRRRRmmmmmkk......"
    "........kmmmmmmmmmmmmmmk........"
    ".........kkkmmmmmmmmkkk........."
    "............kkkkkkkk............"
    "................................";
PC_ART_CHECK(ico_error, 32, 32);

/* ==================================================================== */
/* Open/Save dialog */
/* ==================================================================== */

/* PC_ICON_FOLDER_OPEN (16x16) */
static const char ico_folder_open[] =
    "................"
    "..kkkkk........."
    ".kwwwwwk........"
    ".kwYYYYwkkkkkk.."
    ".kwYYYYYYYYYYok."
    ".kwYYYkkkkkkkkkk"
    ".kwYYkwwwwwwwwwk"
    ".kwYkwYYYYYYYYk."
    ".kwYkwYYYYYYYYk."
    ".kwkwYYYYYYYYk.."
    ".kwkwYYYYYYYok.."
    ".kkwYYYYYYYYk..."
    ".kkoooooooook..."
    ".kkkkkkkkkkk...."
    "................"
    "................";
PC_ART_CHECK(ico_folder_open, 16, 16);

/* PC_ICON_FILE_IMAGE (16x16) */
static const char ico_file_image[] =
    "..kkkkkkkk......"
    "..kwwwwwwkk....."
    "..kwwwwwwkwk...."
    "..kwwwwwwkwwk..."
    "..kwwwwwwkkkkk.."
    "..kwwwwwwwwwwk.."
    "..kwkkkkkkkwwk.."
    "..kwkCCCYCkwwk.."
    "..kwkCCCCCkwwk.."
    "..kwkCkCCCkwwk.."
    "..kwkkGkCkkwwk.."
    "..kwkGGGkGkwwk.."
    "..kwkkkkkkkwwk.."
    "..kwwwwwwwwwwk.."
    "..kkkkkkkkkkkk.."
    "................";
PC_ART_CHECK(ico_file_image, 16, 16);

/* PC_ICON_DRIVE_HDD (16x16) */
static const char ico_drive_hdd[] =
    "................"
    "................"
    "................"
    "................"
    "....kkkkkkkkkkk."
    "...kwwwwwwwwwkk."
    "..kwwwwwwwwwkdk."
    ".kkkkkkkkkkkddk."
    ".kwwwwwwwwwkddk."
    ".kwggggggggkddk."
    ".kwgLLggddgkdk.."
    ".kwggggggggkk..."
    ".kkkkkkkkkkk...."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_drive_hdd, 16, 16);

/* PC_ICON_DRIVE_FLOPPY (16x16) */
static const char ico_drive_floppy[] =
    "................"
    ".kkkkkkkkkkkkk.."
    ".knkggggggkknnk."
    ".knkggkkggkknnk."
    ".knkggkkggkknnk."
    ".knkggggggkknnk."
    ".knnkkkkkkknnnk."
    ".knnnnnnnnnnnnk."
    ".knnkkkkkkkknnk."
    ".knkwwwwwwwwknk."
    ".knkwkkkkkkwknk."
    ".knkwwwwwwwwknk."
    ".knkwkkkkkkwknk."
    ".knkwwwwwwwwknk."
    ".kkkkkkkkkkkkkk."
    "................";
PC_ART_CHECK(ico_drive_floppy, 16, 16);

/* PC_ICON_DRIVE_CD (16x16) */
static const char ico_drive_cd[] =
    "................"
    ".....kkkkkk....."
    "....kCCCwwwk...."
    "...kCCCCwwwwk..."
    "..kCCCCwwwwwwk.."
    ".kCCCCddddwwwwk."
    ".kCCCdgkkgdwwwk."
    ".kCCwdk..kdwwwk."
    ".kwwwdk..kdwFFk."
    ".kwwwdgkkgdFFFk."
    ".kwwwwddddFFFFk."
    "..kwwwwwwFFFFk.."
    "...kwwwwFFFFk..."
    "....kwwwFFFk...."
    ".....kkkkkk....."
    "................";
PC_ART_CHECK(ico_drive_cd, 16, 16);

/* PC_ICON_DRIVE_NET (16x16) */
static const char ico_drive_net[] =
    "................"
    "....kkkkkkkkkkk."
    "...kwwwwwwwwwkk."
    "..kwwwwwwwwwkdk."
    ".kkkkkkkkkkkddk."
    ".kwwwwwwwwwkddk."
    ".kwggggggggkddk."
    ".kwgLLggddgkdk.."
    ".kwggggggggkk..."
    ".kkkkkkkkkkk...."
    ".....kGGk......."
    ".....kGGk......."
    "kkkkkkGGkkkkkkkk"
    "GGGGGGGGGGGGGGGG"
    "kkkkkkkkkkkkkkkk"
    "................";
PC_ART_CHECK(ico_drive_net, 16, 16);

/* PC_ICON_UP_FOLDER (16x16) */
static const char ico_up_folder[] =
    "................"
    "..kkkkk........."
    ".kwwwwwk........"
    ".kwYYYYwkkkkkkk."
    ".kwYYYYYwwwwwwk."
    ".kwYYYYYkYYYYok."
    ".kwYYYYkkkYYYok."
    ".kwYYYkkkkkYYok."
    ".kwYYkkkkkkkYok."
    ".kwYYYYkkkYYYok."
    ".kwYYYYkkkYYYok."
    ".kwYYYYkkkYYYok."
    ".kooooooooooook."
    ".kkkkkkkkkkkkkk."
    "................"
    "................";
PC_ART_CHECK(ico_up_folder, 16, 16);

/* PC_ICON_NEW_FOLDER (16x16) */
static const char ico_new_folder[] =
    "............k..."
    "..kkkkk...k.k.k."
    ".kwwwwwk...kYk.."
    ".kwYYYYwkkkYwYkk"
    ".kwYYYYYwwwkYkk."
    ".kwYYYYYYYkYkok."
    ".kwYYYYYYYYYkok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kwYYYYYYYYYYok."
    ".kooooooooooook."
    ".kkkkkkkkkkkkkk."
    "................"
    "................";
PC_ART_CHECK(ico_new_folder, 16, 16);

/* PC_ICON_VIEW_LIST (16x16) */
static const char ico_view_list[] =
    "................"
    "................"
    "kkkk.kkk.kkkk.kk"
    "kYYk.....kYYk..."
    "kkkk.....kkkk..."
    "................"
    "kkkk.kkk.kkkk.kk"
    "kYYk.....kYYk..."
    "kkkk.....kkkk..."
    "................"
    "kkkk.kkk.kkkk.kk"
    "kYYk.....kYYk..."
    "kkkk.....kkkk..."
    "................"
    "................"
    "................";
PC_ART_CHECK(ico_view_list, 16, 16);

/* PC_ICON_VIEW_DETAILS (16x16) */
static const char ico_view_details[] =
    "................"
    "................"
    "kkkkkkkkkkkkkkkk"
    "................"
    "kkk.kkkkk.kk.kkk"
    "kYk............."
    "kkk............."
    "................"
    "kkk.kkkkk.kk.kkk"
    "kYk............."
    "kkk............."
    "................"
    "kkk.kkkkk.kk.kkk"
    "kYk............."
    "kkk............."
    "................";
PC_ART_CHECK(ico_view_details, 16, 16);

/* ==================================================================== */
/* Cursors (32x32): black glyph with a white halo, x = inverted screen */
/* ==================================================================== */

/* PC_CURSOR_CROSSHAIR, hotspot (15,15) */
static const char cur_crosshair[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "..............www..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "......wwwwwwwwwwwwwwwwwww......."
    "......wkkkkkkkwxwkkkkkkkw......."
    "......wwwwwwwwwwwwwwwwwww......."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_crosshair, 32, 32);

/* PC_CURSOR_MOVE, hotspot (1,1) */
static const char cur_move[] =
    "www............................."
    "wkww............................"
    "wkkww..........................."
    "wkkkww.........................."
    "wkkkkww........................."
    "wkkkkkww........................"
    "wkkkkkkww......................."
    "wkkkkkkkww......................"
    "wkkkkkkkkww....................."
    "wkkkkkkkkkww...................."
    "wkkkkkkkkkkw...................."
    "wkkkkkkwwwww..www..............."
    "wkkkwkkkw....wwkww.............."
    "wkkwwkkkww...wkkkw.............."
    "wkwwwwkkkw.wwwwkwwww............"
    "www..wkkkwwwkwwkwwkww..........."
    ".....wwkkwwkkkkkkkkkw..........."
    "......wwwwwwkwwkwwkww..........."
    "...........wwwwkwwww............"
    ".............wkkkw.............."
    ".............wwkww.............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_move, 32, 32);

/* PC_CURSOR_HAND, hotspot (9,9) */
static const char cur_hand[] =
    ".......wwww....................."
    "......wwkkwwww.................."
    "....wwwkwwkkkww................."
    "...wwkkkwwkwwkwww..............."
    "...wkwwkwwkwwkkkww.............."
    "...wkwwkwwkwwkwwkw.............."
    "...wkwwkwwkwwkwwkw.............."
    "wwwwkwwkwwkwwkwwkw.............."
    "wkkwkwwkwwkwwkwwkw.............."
    "wkwkkwwwwwwwwwwwkw.............."
    "wkwwkwwwwwwwwwwwkw.............."
    "wwkwwwwwwwwwwwwwkw.............."
    ".wwkwwwwwwwwwwwkww.............."
    "..wwkwwwwwwwwwkww..............."
    "...wwkwwwwwwwkww................"
    "....wkwwwwwwwkw................."
    "....wkkkkkkkkkw................."
    "....wwwwwwwwwww................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_hand, 32, 32);

/* PC_CURSOR_GRAB, hotspot (9,9) */
static const char cur_grab[] =
    "................................"
    "................................"
    "................................"
    "....wwwwwwwwww.................."
    "...wwkkwkkwkkwww................"
    "...wkwwkwwkwwkkww..............."
    ".wwwkwwkwwkwwkwkww.............."
    "wwkkkwwwwwwwwkwwkw.............."
    "wkwwkwwwwwwwwwwwkw.............."
    "wkwwwwwwwwwwwwwwkw.............."
    "wwkwwwwwwwwwwwwwkw.............."
    ".wwkwwwwwwwwwwwkww.............."
    "..wwkwwwwwwwwwkww..............."
    "...wwkwwwwwwwkww................"
    "....wkwwwwwwwkw................."
    "....wkkkkkkkkkw................."
    "....wwwwwwwwwww................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_grab, 32, 32);

/* PC_CURSOR_ZOOM_IN, hotspot (7,7) */
static const char cur_zoom_in[] =
    "....wwwwwww....................."
    "..wwwkkkkkwww..................."
    ".wwkkwwwwwkkww.................."
    ".wkwwwwwwwwwkw.................."
    "wwkwwwwkwwwwkww................."
    "wkwwwwwkwwwwwkw................."
    "wkwwwwwkwwwwwkw................."
    "wkwwkkkkkkkwwkw................."
    "wkwwwwwkwwwwwkw................."
    "wkwwwwwkwwwwwkw................."
    "wwkwwwwkwwwwkww................."
    ".wkwwwwwwwwwkww................."
    ".wwkkwwwwwkkkkww................"
    "..wwwkkkkkwwkkkww..............."
    "....wwwwwwwwwkkkww.............."
    "............wwkkkww............."
    ".............wwkkkww............"
    "..............wwkkkw............"
    "...............wwkkw............"
    "................wwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_zoom_in, 32, 32);

/* PC_CURSOR_ZOOM_OUT, hotspot (7,7) */
static const char cur_zoom_out[] =
    "....wwwwwww....................."
    "..wwwkkkkkwww..................."
    ".wwkkwwwwwkkww.................."
    ".wkwwwwwwwwwkw.................."
    "wwkwwwwwwwwwkww................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wkwwkkkkkkkwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wwkwwwwwwwwwkww................."
    ".wkwwwwwwwwwkww................."
    ".wwkkwwwwwkkkkww................"
    "..wwwkkkkkwwkkkww..............."
    "....wwwwwwwwwkkkww.............."
    "............wwkkkww............."
    ".............wwkkkww............"
    "..............wwkkkw............"
    "...............wwkkw............"
    "................wwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_zoom_out, 32, 32);

/* PC_CURSOR_ZOOM_NONE, hotspot (7,7) */
static const char cur_zoom_none[] =
    "....wwwwwww....................."
    "..wwwkkkkkwww..................."
    ".wwkkwwwwwkkww.................."
    ".wkwwwwwwwwwkw.................."
    "wwkwwwwwwwwwkww................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wkwwwwwwwwwwwkw................."
    "wwkwwwwwwwwwkww................."
    ".wkwwwwwwwwwkww................."
    ".wwkkwwwwwkkkkww................"
    "..wwwkkkkkwwkkkww..............."
    "....wwwwwwwwwkkkww.............."
    "............wwkkkww............."
    ".............wwkkkww............"
    "..............wwkkkw............"
    "...............wwkkw............"
    "................wwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_zoom_none, 32, 32);

/* PC_CURSOR_EYEDROPPER, hotspot (2,15) */
static const char cur_eyedropper[] =
    "................................"
    "...........wwwww................"
    "..........wwkkkww..............."
    "..........wkkkkkw..............."
    "........wwwkkkkkw..............."
    "........wkwkkkkkw..............."
    "........wwkkkkkww..............."
    ".......wwkwkkkwkw..............."
    "......wwkwwwkwwww..............."
    ".....wwkwwwkwkw................."
    "....wwkwwwkwwww................."
    "...wwkwwwkww...................."
    "...wkwwwkww....................."
    "..wwkwwkww......................"
    ".wwkkkkww......................."
    ".wkwwwww........................"
    ".www............................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_eyedropper, 32, 32);

/* PC_CURSOR_BUCKET, hotspot (4,15) */
static const char cur_bucket[] =
    ".........wwwww.................."
    "........wwkkkww................."
    ".......wwkwwwkww................"
    "......wwkkkwwwkw................"
    ".....wwkwwwkwwkw................"
    "....wwkwwwwwkwkw................"
    "...wwkkwwwwwwkkw................"
    "..wwkkkkwwwwwwkw................"
    "..wkkkkkkwwwwwkw................"
    "..wkkkkkkwwwwwkw................"
    "..wkkkkkkwwwwkww................"
    "..wkkkwkkkwwkww................."
    "..wkkkwwwkkkww.................."
    "..wkkkw.wwwww..................."
    "..wkkkw........................."
    "..wwkww........................."
    "...www.........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_bucket, 32, 32);

/* PC_CURSOR_PENCIL, hotspot (2,15) */
static const char cur_pencil[] =
    "................................"
    "............wwww................"
    "...........wwkkww..............."
    "..........wwkkkkw..............."
    ".........wwkwkkkw..............."
    "........wwkwwwkww..............."
    ".......wwkwwwkww................"
    "......wwkwwwkww................."
    ".....wwkwwwkww.................."
    "...wwwkwwwkww..................."
    "...wkkwwwkww...................."
    "..wwkwwwkww....................."
    "..wkwwwkww......................"
    "..wkwwkkw......................."
    ".wwkkkwww......................."
    ".wkwwww........................."
    ".www............................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_pencil, 32, 32);

/* PC_CURSOR_BRUSH, hotspot (2,16) */
static const char cur_brush[] =
    "................................"
    "..............www..............."
    ".............wwkww.............."
    "............wwkkkw.............."
    "...........wwkkkkw.............."
    "..........wwkkkkww.............."
    ".........wwkkkkww..............."
    "........wwkkkkww................"
    ".......wwkwwkww................."
    "......wwkwwwkw.................."
    ".....wwkkwwkww.................."
    "....wwkkkkkww..................."
    "...wwkkkkkww...................."
    "..wwkkkkkww....................."
    "..wkkkkwww......................"
    ".wwkkkww........................"
    ".wkkwww........................."
    ".wwww..........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_brush, 32, 32);

/* PC_CURSOR_AIRBRUSH, hotspot (2,9) */
static const char cur_airbrush[] =
    "................................"
    "................................"
    ".........wwwwww................."
    ".........wkkkkw................."
    ".........wkkkkw................."
    ".........wkkkkw................."
    "www..wwwwwwkkwwww..............."
    "wkwwwwkkkkkkkkkkww.............."
    "wwwwkkwwwwwwwwwwkw.............."
    ".wkwkkwwwwwwwwwwkw.............."
    "wwwwwwkkkkkkkkkkkw.............."
    "wkwkwwwwwwwkkwwkww.............."
    "wwwww.....wkkwwkww.............."
    ".wkw......wwwwwwkw.............."
    ".www..........wwkw.............."
    "..............wkww.............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_airbrush, 32, 32);

/* PC_CURSOR_STAMP, hotspot (8,14) */
static const char cur_stamp[] =
    "................................"
    "......wwwwww...................."
    ".....wwkkkkww..................."
    ".....wkwwwwkw..................."
    ".....wkwwwwkw..................."
    ".....wwkwwkww..................."
    "......wwkkww...................."
    "......wkwwkw...................."
    "..wwwwwkwwkwwwww................"
    ".wwkkkkkkkkkkkkww..............."
    ".wkwwwwwwwwwwwwkw..............."
    ".wkwwwwwwwwwwwwkw..............."
    ".wkkkkkkkkkkkkkkw..............."
    ".wwkkkkkkkkkkkkww..............."
    "..wkkkkkkkkkkkkw................"
    "..wwwwwwwwwwwwww................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_stamp, 32, 32);

/* PC_CURSOR_STAMP_SOURCE, hotspot (15,15) */
static const char cur_stamp_source[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "..............www..............."
    "..............wkw..............."
    "............wwwkwww............."
    "..........wwwkkkkkwww..........."
    ".........wwkkwwkwwkkww.........."
    "........wwkwwwwkwwwwkww........."
    "........wkww..wkw..wwkw........."
    ".......wwkw...wkw...wkww........"
    ".......wkww...wkw...wwkw........"
    ".....wwwkwwwwwwwwwwwwwkwww......"
    ".....wkkkkkkkkwxwkkkkkkkkw......"
    ".....wwwkwwwwwwwwwwwwwkwww......"
    ".......wkww...wkw...wwkw........"
    ".......wwkw...wkw...wkww........"
    "........wkww..wkw..wwkw........."
    "........wwkwwwwkwwwwkww........."
    ".........wwkkwwkwwkkww.........."
    "..........wwwkkkkkwww..........."
    "............wwwkwww............."
    "..............wkw..............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_stamp_source, 32, 32);

/* PC_CURSOR_ERASER, hotspot (2,15) */
static const char cur_eraser[] =
    "................................"
    "................................"
    "................................"
    "......wwwwwwwwwww..............."
    ".....wwkkkkkkkkkww.............."
    "....wwkwwwwwwwwkkw.............."
    "...wwkwwwwwwwwkwkw.............."
    "..wwkwwwwwwwwkwwkw.............."
    ".wwkwwwwwwwwkwwwkw.............."
    ".wkkkkkkkkkkwwwwkw.............."
    ".wkwwwwwwwwkwwwwkw.............."
    ".wkwwwwwwwwkwwwkww.............."
    ".wkwwwwwwwwkwwkww..............."
    ".wkwwwwwwwwkwkww................"
    ".wkwwwwwwwwkkww................."
    ".wkkkkkkkkkkww.................."
    ".wwwwwwwwwwww..................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_eraser, 32, 32);

/* PC_CURSOR_LASSO, hotspot (6,15) */
static const char cur_lasso[] =
    "................................"
    "......wwwwwww..................."
    "....wwwkkkkkwww................."
    "...wwkkwwwwwkkww................"
    "..wwkwww...wwwkww..............."
    "..wkww.......wwkw..............."
    "..wkw.........wkw..............."
    "..wkww.......wwkw..............."
    "..wwkwww...wwwkww..............."
    "...wwkkwwwwwkkww................"
    "...wwkkkkkkkwww................."
    "...wkwkwwwwww..................."
    "...wkwwkw......................."
    "...wkwwww......................."
    "...wwkww........................"
    "....wkkw........................"
    "....wwww........................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_lasso, 32, 32);

/* PC_CURSOR_LASSO_POLY, hotspot (6,15) */
static const char cur_lasso_poly[] =
    "................................"
    "......wwwwww...................."
    ".....wwkkkkwwwww................"
    "....wwkwwwwkkkkw................"
    "...wwkww..wwwwkw................"
    "..wwkww......wkww..............."
    "..wkww.......wwkw..............."
    "..wkww.......wwkw..............."
    "..wwkw.....wwwkkw..............."
    "...wkwwwwwwwkkwww..............."
    "...wwkkkkkkkwww................."
    "...wkwkwwwwww..................."
    "...wkwwkw......................."
    "...wkwwww......................."
    "...wwkww........................"
    "....wkkw........................"
    "....wwww........................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_lasso_poly, 32, 32);

/* PC_CURSOR_WAND, hotspot (12,4) */
static const char cur_wand[] =
    "...........www.................."
    "...........wkw.................."
    "..........wwkww................."
    "........wwwkwkwww..............."
    "........wkkwwwkkw..............."
    ".......wwwwkwkwww..............."
    ".......wkwwwkwwkw..............."
    ".......wwwkwkwwww..............."
    "......wwkkwwww.................."
    ".....wwkwkwkw..................."
    "....wwkkkwwww..................."
    "...wwkkkww......................"
    "..wwkkkww......................."
    ".wwkkkww........................"
    ".wkkkww........................."
    ".wkkww.........................."
    ".wwww..........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_wand, 32, 32);

/* PC_CURSOR_CROP, hotspot (4,4) */
static const char cur_crop[] =
    "...wwww........................."
    "...wkkw........................."
    "...wkkw........................."
    "wwwwkkwwwwwwwww................."
    "wkkkkkkkkkkkkkw................."
    "wkkkkkkkkkkkkkw................."
    "wwwwkkwwwwwwkkw................."
    "...wkkw....wkkw................."
    "...wkkw....wkkw................."
    "...wkkw....wkkw................."
    "...wkkw....wkkw................."
    "...wkkwwwwwwkkwwww.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wwwwwwwwwkkwwww.............."
    "...........wkkw................."
    "...........wkkw................."
    "...........wwww................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_crop, 32, 32);

/* PC_CURSOR_IBEAM, hotspot (15,15) */
static const char cur_ibeam[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "...........wwwwwwwww............"
    "...........wkkkwkkkw............"
    "...........wwwwkwwww............"
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "...........wwwwkwwww............"
    "...........wkkkwkkkw............"
    "...........wwwwwwwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_ibeam, 32, 32);

/* PC_CURSOR_IBEAM_VERTICAL, hotspot (15,15) */
static const char cur_ibeam_vertical[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    ".......www............www......."
    ".......wkw............wkw......."
    ".......wkw............wkw......."
    ".......wkwwwwwwwwwwwwwwkw......."
    ".......wwkkkkkkkkkkkkkkww......."
    ".......wkwwwwwwwwwwwwwwkw......."
    ".......wkw............wkw......."
    ".......wkw............wkw......."
    ".......www............www......."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_ibeam_vertical, 32, 32);

/* PC_CURSOR_PEN, hotspot (8,14) */
static const char cur_pen[] =
    "................................"
    "....wwwwwwwww..................."
    "....wkkkkkkkw..................."
    "...wwkkkkkkkww.................."
    "...wkwwwkwwwkw.................."
    "...wkwwwkwwwkw.................."
    "...wkwwkwkwwkw.................."
    "...wkwwwkwwwkw.................."
    "...wwkwwkwwkww.................."
    "....wkwwkwwkw..................."
    "....wwkwkwkww..................."
    ".....wkwkwkw...................."
    ".....wwkkkww...................."
    "......wwkww....................."
    ".......wkw......................"
    ".......www......................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_pen, 32, 32);

/* PC_CURSOR_PEN_ADD, hotspot (6,13) */
static const char cur_pen_add[] =
    "..wwwwwwwww....................."
    "..wkkkkkkkw....................."
    ".wwkkkkkkkww...................."
    ".wkwwwkwwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wkwwkwkwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wwkwwkwwkww...................."
    "..wkwwkwwkw....................."
    "..wwkwkwkww....................."
    "...wkwkwkw..www................."
    "...wwkkkww..wkw................."
    "....wwkww.wwwkwww..............."
    ".....wkw..wkkkkkw..............."
    ".....www..wwwkwww..............."
    "............wkw................."
    "............www................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_pen_add, 32, 32);

/* PC_CURSOR_PEN_DELETE, hotspot (6,13) */
static const char cur_pen_delete[] =
    "..wwwwwwwww....................."
    "..wkkkkkkkw....................."
    ".wwkkkkkkkww...................."
    ".wkwwwkwwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wkwwkwkwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wwkwwkwwkww...................."
    "..wkwwkwwkw....................."
    "..wwkwkwkww....................."
    "...wkwkwkw......................"
    "...wwkkkww......................"
    "....wwkww.wwwwwww..............."
    ".....wkw..wkkkkkw..............."
    ".....www..wwwwwww..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_pen_delete, 32, 32);

/* PC_CURSOR_PEN_CLOSE, hotspot (6,13) */
static const char cur_pen_close[] =
    "..wwwwwwwww....................."
    "..wkkkkkkkw....................."
    ".wwkkkkkkkww...................."
    ".wkwwwkwwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wkwwkwkwwkw...................."
    ".wkwwwkwwwkw...................."
    ".wwkwwkwwkww...................."
    "..wkwwkwwkw....................."
    "..wwkwkwkww....................."
    "...wkwkwkw.wwwww................"
    "...wwkkkwwwwkkkww..............."
    "....wwkww.wkwwwkw..............."
    ".....wkw..wkw.wkw..............."
    ".....www..wkwwwkw..............."
    "..........wwkkkww..............."
    "...........wwwww................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_pen_close, 32, 32);

/* PC_CURSOR_DIRECT_SELECT, hotspot (5,1) */
static const char cur_direct_select[] =
    "....www........................."
    "....wkww........................"
    "....wkkww......................."
    "....wkwkww......................"
    "....wkwwkww....................."
    "....wkwwwkww...................."
    "....wkwwwwkww..................."
    "....wkwwwwwkww.................."
    "....wkwwwwwwkww................."
    "....wkwwwwwwwkww................"
    "....wkwwwwkkkkkw................"
    "....wkwwkwwkwwww................"
    "....wkwkwkwwkww................."
    "....wkkwwwkwwkww................"
    "....wkww.wwkwwkw................"
    "....www...wwkkww................"
    "...........wwww................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_direct_select, 32, 32);

/* PC_CURSOR_MEASURE, hotspot (1,14) */
static const char cur_measure[] =
    ".............www................"
    "............wwkww..............."
    "...........wwkwkww.............."
    "..........wwkwwkkw.............."
    ".........wwkwkwkww.............."
    "........wwkwwkkww..............."
    ".......wwkwwwkww................"
    "......wwkwwkkww................."
    ".....wwkwkwkww.................."
    "....wwkwwkkww..................."
    "...wwkwwwkww...................."
    "..wwkwwkkww....................."
    ".wwkwkwkww......................"
    "wwkwwkkww......................."
    "wkwwwkww........................"
    "wwkkkww........................."
    ".wwkww.........................."
    "..www..........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_measure, 32, 32);

/* PC_CURSOR_SMUDGE, hotspot (2,12) */
static const char cur_smudge[] =
    "................................"
    "..........wwwwww................"
    ".........wwkkkkww..............."
    "........wwkwwwwkww.............."
    ".......wwkwkkwwwkw.............."
    "......wwkwwwkwwwkw.............."
    ".....wwkwwwkkwwwkw.............."
    "....wwkwwwkwkwwkww.............."
    "...wwkwwwkwwkwwkw..............."
    "..wwkwwwkwwkwwkww..............."
    ".wwkwwwkwwwkkkww................"
    ".wkwwwkww.wwwww................."
    ".wkwwkww........................"
    ".wwkkww........................."
    "..wwww.........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_smudge, 32, 32);

/* PC_CURSOR_BLUR, hotspot (8,11) */
static const char cur_blur[] =
    "................................"
    ".......wwww....................."
    ".......wkkw....................."
    "......wwkkww...................."
    "......wkwwkw...................."
    ".....wwkwwkww..................."
    ".....wkwwwwkw..................."
    "....wwkwwwwkww.................."
    "...wwkwwwwwwkww................."
    "...wkwwwwwwwwkw................."
    "..wwkwwwwwwwwkww................"
    "..wkwwwwwwwwwwkw................"
    "..wkwwwwwwwwwwkw................"
    "..wkwwwwwwwwwwkw................"
    "..wwkwwwwwwwwkww................"
    "...wwkkwwwwwkww................."
    "....wwwkkkkwww.................."
    "......wwwwww...................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_blur, 32, 32);

/* PC_CURSOR_SHARPEN, hotspot (8,2) */
static const char cur_sharpen[] =
    "................................"
    ".......wwww....................."
    ".......wkkw....................."
    "......wwkkww...................."
    "......wkwwkw...................."
    ".....wwkwwkww..................."
    ".....wkwwwwkw..................."
    "....wwkwwwwkww.................."
    "....wkwwwwwwkw.................."
    "...wwkwwwwwwkww................."
    "...wkwwwwwwwwkw................."
    "..wwkwwwwwwwwkww................"
    "..wkwwwwwwwwwwkw................"
    ".wwkwwwwwwwwwwkww..............."
    ".wkwwwwwwwwwwwwkw..............."
    ".wkkkkkkkkkkkkkkw..............."
    ".wwwwwwwwwwwwwwww..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_sharpen, 32, 32);

/* PC_CURSOR_DODGE, hotspot (12,6) */
static const char cur_dodge[] =
    "................................"
    ".........wwwwww................."
    "........wwkkkkww................"
    ".......wwkkkkkkww..............."
    ".......wkkwkkkkkw..............."
    ".......wkwkkkkkkw..............."
    ".......wkkkkkkkkw..............."
    ".......wkkkkkkkkw..............."
    ".......wwkkkkkkww..............."
    "......wwkwkkkkww................"
    ".....wwkkwwwwww................."
    "....wwkkww......................"
    "...wwkkww......................."
    "..wwkkww........................"
    ".wwkkww........................."
    ".wkkww.........................."
    ".wwww..........................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_dodge, 32, 32);

/* PC_CURSOR_BURN, hotspot (7,5) */
static const char cur_burn[] =
    "................................"
    "....wwwwwww....................."
    "...wwkkkkkwww..................."
    "..wwkwwwwwkkww.................."
    "..wkwwkkkwwwkww................."
    "..wkwkwwwkwwwkww................"
    "..wkwkwwwkwwwwkw................"
    "..wkwwkkkwkwwwkw................"
    "..wwkwwwwkwkwwkw................"
    "..wwkkkkkwwkwwkw................"
    "..wkwwwwwkwkwwkw................"
    "..wkkkkkkwwkwkww................"
    "..wkwwwwwwkwwkw................."
    "..wwkkkkkkwwkww................."
    "...wwwwwwkwwkw.................."
    "........wkkkkw.................."
    "........wwwwww.................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_burn, 32, 32);

/* PC_CURSOR_SPONGE, hotspot (8,8) */
static const char cur_sponge[] =
    "................................"
    "................................"
    "...wwwwwwwwwwww................."
    "..wwkkkkkkkkkkww................"
    ".wwkwwwwwwwwwwkww..............."
    ".wkwwkwwwwwkwwkkw..............."
    ".wkwwwwwkwwwwwkkw..............."
    ".wkwkwwwwwwwkwkkw..............."
    ".wkwwwwkwwwwwwkkw..............."
    ".wkwwwwwwwkwwwkkw..............."
    ".wkwwkwwwwwwwwkkw..............."
    ".wkwwwwwwwkwwkkkw..............."
    ".wwkwwwkwwwwwkkww..............."
    "..wwkkkkkkkkkkww................"
    "...wwwwwwwwwwww................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_sponge, 32, 32);

/* PC_CURSOR_GRADIENT, hotspot (15,15) */
static const char cur_gradient[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "..............www..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    ".......wwwwwwwwkwwwwwwww........"
    ".......wkkkkkkkxkkkkkkkw........"
    ".......wwwwwwwwkwwwwwwww........"
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_gradient, 32, 32);

/* PC_CURSOR_MARQUEE, hotspot (10,10) */
static const char cur_marquee[] =
    "................................"
    "................................"
    "................................"
    ".........www...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    "...wwwwwwwkwwwwwww.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wwwwwwwkwwwwwww.............."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........www...................."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_marquee, 32, 32);

/* PC_CURSOR_NO, hotspot (15,15) */
static const char cur_no[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "...........wwwwwwwww............"
    ".........wwwkkkkkkkwww.........."
    "........wwkkkkkkkkkkkww........."
    ".......wwkkkkkkkkkkkkkww........"
    "......wwkkkkkwwwwwkkkkkww......."
    "......wkkkkkww...wwwkkkkw......."
    ".....wwkkkkkkww....wwkkkww......"
    ".....wkkkkwkkkww....wkkkkw......"
    ".....wkkkwwwkkkww...wwkkkw......"
    ".....wkkkw.wwkkkww...wkkkw......"
    ".....wkkkw..wwkkkww..wkkkw......"
    ".....wkkkw...wwkkkww.wkkkw......"
    ".....wkkkww...wwkkkwwwkkkw......"
    ".....wkkkkw....wwkkkwkkkkw......"
    ".....wwkkkww....wwkkkkkkww......"
    "......wkkkkwww...wwkkkkkw......."
    "......wwkkkkkwwwwwkkkkkww......."
    ".......wwkkkkkkkkkkkkkww........"
    "........wwkkkkkkkkkkkww........."
    ".........wwwkkkkkkkwww.........."
    "...........wwwwwwwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_no, 32, 32);

/* PC_CURSOR_ROTATE, hotspot (15,15) */
static const char cur_rotate[] =
    "................................"
    "................................"
    "................................"
    "................................"
    ".................www............"
    ".................wkww..........."
    ".................wkkww.........."
    ".............wwwwwkkkww........."
    "...........wwwkkkkkkkkw........."
    "..........wwkkkwwwkkkww........."
    ".........wwkkwww.wkkww.........."
    "........wwkkww...wkww..........."
    "........wkkww....www............"
    ".......wwkww...................."
    ".......wkkw....................."
    ".......wkww....................."
    ".......wkw......................"
    "....wwwwkwwww..................."
    "....wkkkkkkkw..................."
    "....wwkkkkkww..................."
    ".....wwkkkww...................."
    "......wwkww....................."
    ".......www......................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_rotate, 32, 32);

/* PC_CURSOR_SCALE_NS, hotspot (15,15) */
static const char cur_scale_ns[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "..............www..............."
    ".............wwkww.............."
    "............wwkkkww............."
    "...........wwkkkkkww............"
    "...........wkkkkkkkw............"
    "...........wwwwkwwww............"
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "..............wkw..............."
    "...........wwwwkwwww............"
    "...........wkkkkkkkw............"
    "...........wwkkkkkww............"
    "............wwkkkww............."
    ".............wwkww.............."
    "..............www..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_scale_ns, 32, 32);

/* PC_CURSOR_SCALE_EW, hotspot (15,15) */
static const char cur_scale_ew[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "..........www.....www..........."
    ".........wwkw.....wkww.........."
    "........wwkkw.....wkkww........."
    ".......wwkkkwwwwwwwkkkww........"
    ".......wkkkkkkkkkkkkkkkw........"
    ".......wwkkkwwwwwwwkkkww........"
    "........wwkkw.....wkkww........."
    ".........wwkw.....wkww.........."
    "..........www.....www..........."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_scale_ew, 32, 32);

/* PC_CURSOR_SCALE_NWSE, hotspot (15,15) */
static const char cur_scale_nwse[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "........wwwwwwww................"
    "........wkkkkkkw................"
    "........wkkkkkww................"
    "........wkkkkww................."
    "........wkkkkww................."
    "........wkkwwkww................"
    "........wkwwwwkww..............."
    "........www..wwkww..www........."
    "..............wwkwwwwkw........."
    "...............wwkwwkkw........."
    "................wwkkkkw........."
    "................wwkkkkw........."
    "...............wwkkkkkw........."
    "...............wkkkkkkw........."
    "...............wwwwwwww........."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_scale_nwse, 32, 32);

/* PC_CURSOR_SCALE_NESW, hotspot (16,15) */
static const char cur_scale_nesw[] =
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................wwwwwwww........"
    "................wkkkkkkw........"
    "................wwkkkkkw........"
    ".................wwkkkkw........"
    ".................wwkkkkw........"
    "................wwkwwkkw........"
    "...............wwkwwwwkw........"
    ".........www..wwkww..www........"
    ".........wkwwwwkww.............."
    ".........wkkwwkww..............."
    ".........wkkkkww................"
    ".........wkkkkww................"
    ".........wkkkkkww..............."
    ".........wkkkkkkw..............."
    ".........wwwwwwww..............."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_scale_nesw, 32, 32);

/* PC_CURSOR_MARQUEE_ADD, hotspot (10,10) */
static const char cur_marquee_add[] =
    "................................"
    "................................"
    "................................"
    ".........www...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    "...wwwwwwwkwwwwwww.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wwwwwwwkwwwwwww.............."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw......www..........."
    ".........www......wkw..........."
    "................wwwkwww........."
    "................wkkkkkw........."
    "................wwwkwww........."
    "..................wkw..........."
    "..................www..........."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_marquee_add, 32, 32);

/* PC_CURSOR_MARQUEE_SUB, hotspot (10,10) */
static const char cur_marquee_sub[] =
    "................................"
    "................................"
    "................................"
    ".........www...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    "...wwwwwwwkwwwwwww.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wwwwwwwkwwwwwww.............."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........www...................."
    "................wwwwwww........."
    "................wkkkkkw........."
    "................wwwwwww........."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_marquee_sub, 32, 32);

/* PC_CURSOR_MARQUEE_INT, hotspot (10,10) */
static const char cur_marquee_int[] =
    "................................"
    "................................"
    "................................"
    ".........www...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    "...wwwwwwwkwwwwwww.............."
    "...wkkkkkkkkkkkkkw.............."
    "...wwwwwwwkwwwwwww.............."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw...................."
    ".........wkw....www.www........."
    ".........www....wkwwwkw........."
    "................wwkwkww........."
    ".................wwkww.........."
    "................wwkwkww........."
    "................wkwwwkw........."
    "................www.www........."
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_marquee_int, 32, 32);

/* PC_CURSOR_MOVE_SELECTION, hotspot (1,1) */
static const char cur_move_selection[] =
    "www............................."
    "wkww............................"
    "wkkww..........................."
    "wkkkww.........................."
    "wkkkkww........................."
    "wkkkkkww........................"
    "wkkkkkkww......................."
    "wkkkkkkkww......................"
    "wkkkkkkkkww....................."
    "wkkkkkkkkkww...................."
    "wkkkkkkkkkkw...................."
    "wkkkkkkwwwww...................."
    "wkkkwkkkw.wwwwwwwwww............"
    "wkkwwkkkwwwkkwkkwkkww..........."
    "wkwwwwkkkwwkwwwwwwwkw..........."
    "www..wkkkwwww.....www..........."
    ".....wwkkwwkw.....wkw..........."
    "......wwwwwkw.....wkw..........."
    "..........wwwwwwwwwww..........."
    "..........wkkwkkwkkw............"
    "..........wwwwwwwwww............"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................"
    "................................";
PC_ART_CHECK(cur_move_selection, 32, 32);

static const PcIconDef g_icons[PC_ICON_COUNT] = {
    [PC_ICON_TOOL_MOVE] = { "tool_move", 16, 16, ico_tool_move },
    [PC_ICON_TOOL_MARQUEE_RECT] = { "tool_marquee_rect", 16, 16, ico_tool_marquee_rect },
    [PC_ICON_TOOL_MARQUEE_ELLIPSE] = { "tool_marquee_ellipse", 16, 16, ico_tool_marquee_ellipse },
    [PC_ICON_TOOL_MARQUEE_ROW] = { "tool_marquee_row", 16, 16, ico_tool_marquee_row },
    [PC_ICON_TOOL_MARQUEE_COL] = { "tool_marquee_col", 16, 16, ico_tool_marquee_col },
    [PC_ICON_TOOL_LASSO] = { "tool_lasso", 16, 16, ico_tool_lasso },
    [PC_ICON_TOOL_LASSO_POLY] = { "tool_lasso_poly", 16, 16, ico_tool_lasso_poly },
    [PC_ICON_TOOL_LASSO_MAGNETIC] = { "tool_lasso_magnetic", 16, 16, ico_tool_lasso_magnetic },
    [PC_ICON_TOOL_WAND] = { "tool_wand", 16, 16, ico_tool_wand },
    [PC_ICON_TOOL_CROP] = { "tool_crop", 16, 16, ico_tool_crop },
    [PC_ICON_TOOL_EYEDROPPER] = { "tool_eyedropper", 16, 16, ico_tool_eyedropper },
    [PC_ICON_TOOL_MEASURE] = { "tool_measure", 16, 16, ico_tool_measure },
    [PC_ICON_TOOL_BRUSH] = { "tool_brush", 16, 16, ico_tool_brush },
    [PC_ICON_TOOL_AIRBRUSH] = { "tool_airbrush", 16, 16, ico_tool_airbrush },
    [PC_ICON_TOOL_PENCIL] = { "tool_pencil", 16, 16, ico_tool_pencil },
    [PC_ICON_TOOL_STAMP] = { "tool_stamp", 16, 16, ico_tool_stamp },
    [PC_ICON_TOOL_PATTERN_STAMP] = { "tool_pattern_stamp", 16, 16, ico_tool_pattern_stamp },
    [PC_ICON_TOOL_HISTORY_BRUSH] = { "tool_history_brush", 16, 16, ico_tool_history_brush },
    [PC_ICON_TOOL_ERASER] = { "tool_eraser", 16, 16, ico_tool_eraser },
    [PC_ICON_TOOL_GRADIENT] = { "tool_gradient", 16, 16, ico_tool_gradient },
    [PC_ICON_TOOL_BUCKET] = { "tool_bucket", 16, 16, ico_tool_bucket },
    [PC_ICON_TOOL_BLUR] = { "tool_blur", 16, 16, ico_tool_blur },
    [PC_ICON_TOOL_SHARPEN] = { "tool_sharpen", 16, 16, ico_tool_sharpen },
    [PC_ICON_TOOL_SMUDGE] = { "tool_smudge", 16, 16, ico_tool_smudge },
    [PC_ICON_TOOL_DODGE] = { "tool_dodge", 16, 16, ico_tool_dodge },
    [PC_ICON_TOOL_BURN] = { "tool_burn", 16, 16, ico_tool_burn },
    [PC_ICON_TOOL_SPONGE] = { "tool_sponge", 16, 16, ico_tool_sponge },
    [PC_ICON_TOOL_PEN] = { "tool_pen", 16, 16, ico_tool_pen },
    [PC_ICON_TOOL_PEN_FREEFORM] = { "tool_pen_freeform", 16, 16, ico_tool_pen_freeform },
    [PC_ICON_TOOL_PEN_MAGNETIC] = { "tool_pen_magnetic", 16, 16, ico_tool_pen_magnetic },
    [PC_ICON_TOOL_PEN_ADD] = { "tool_pen_add", 16, 16, ico_tool_pen_add },
    [PC_ICON_TOOL_PEN_DELETE] = { "tool_pen_delete", 16, 16, ico_tool_pen_delete },
    [PC_ICON_TOOL_PEN_CONVERT] = { "tool_pen_convert", 16, 16, ico_tool_pen_convert },
    [PC_ICON_TOOL_DIRECT_SELECT] = { "tool_direct_select", 16, 16, ico_tool_direct_select },
    [PC_ICON_TOOL_PATH_SELECT] = { "tool_path_select", 16, 16, ico_tool_path_select },
    [PC_ICON_TOOL_TYPE] = { "tool_type", 16, 16, ico_tool_type },
    [PC_ICON_TOOL_TYPE_VERTICAL] = { "tool_type_vertical", 16, 16, ico_tool_type_vertical },
    [PC_ICON_TOOL_TYPE_MASK] = { "tool_type_mask", 16, 16, ico_tool_type_mask },
    [PC_ICON_TOOL_TYPE_MASK_VERTICAL] = { "tool_type_mask_vertical", 16, 16, ico_tool_type_mask_vertical },
    [PC_ICON_TOOL_SHAPE_RECT] = { "tool_shape_rect", 16, 16, ico_tool_shape_rect },
    [PC_ICON_TOOL_SHAPE_ELLIPSE] = { "tool_shape_ellipse", 16, 16, ico_tool_shape_ellipse },
    [PC_ICON_TOOL_SHAPE_LINE] = { "tool_shape_line", 16, 16, ico_tool_shape_line },
    [PC_ICON_TOOL_HAND] = { "tool_hand", 16, 16, ico_tool_hand },
    [PC_ICON_TOOL_ZOOM] = { "tool_zoom", 16, 16, ico_tool_zoom },
    [PC_ICON_DEFAULT_COLORS] = { "default_colors", 10, 10, ico_default_colors },
    [PC_ICON_SWAP_COLORS] = { "swap_colors", 10, 10, ico_swap_colors },
    [PC_ICON_STANDARD_MODE] = { "standard_mode", 16, 16, ico_standard_mode },
    [PC_ICON_QUICKMASK_MODE] = { "quickmask_mode", 16, 16, ico_quickmask_mode },
    [PC_ICON_SCREEN_STANDARD] = { "screen_standard", 16, 16, ico_screen_standard },
    [PC_ICON_SCREEN_FULL_MENU] = { "screen_full_menu", 16, 16, ico_screen_full_menu },
    [PC_ICON_SCREEN_FULL] = { "screen_full", 16, 16, ico_screen_full },
    [PC_ICON_FLYOUT] = { "flyout", 4, 4, ico_flyout },
    [PC_ICON_PANEL_MENU] = { "panel_menu", 10, 9, ico_panel_menu },
    [PC_ICON_CHEVRON_LEFT] = { "chevron_left", 8, 7, ico_chevron_left },
    [PC_ICON_CHEVRON_RIGHT] = { "chevron_right", 8, 7, ico_chevron_right },
    [PC_ICON_PANEL_NAVIGATOR] = { "panel_navigator", 16, 16, ico_panel_navigator },
    [PC_ICON_PANEL_INFO] = { "panel_info", 16, 16, ico_panel_info },
    [PC_ICON_PANEL_COLOR] = { "panel_color", 16, 16, ico_panel_color },
    [PC_ICON_PANEL_SWATCHES] = { "panel_swatches", 16, 16, ico_panel_swatches },
    [PC_ICON_PANEL_BRUSHES] = { "panel_brushes", 16, 16, ico_panel_brushes },
    [PC_ICON_PANEL_LAYERS] = { "panel_layers", 16, 16, ico_panel_layers },
    [PC_ICON_PANEL_CHANNELS] = { "panel_channels", 16, 16, ico_panel_channels },
    [PC_ICON_PANEL_PATHS] = { "panel_paths", 16, 16, ico_panel_paths },
    [PC_ICON_PANEL_HISTORY] = { "panel_history", 16, 16, ico_panel_history },
    [PC_ICON_PANEL_ACTIONS] = { "panel_actions", 16, 16, ico_panel_actions },
    [PC_ICON_PANEL_CHARACTER] = { "panel_character", 16, 16, ico_panel_character },
    [PC_ICON_EYE] = { "eye", 13, 11, ico_eye },
    [PC_ICON_BRUSH_SMALL] = { "brush_small", 13, 13, ico_brush_small },
    [PC_ICON_MASK_SMALL] = { "mask_small", 13, 13, ico_mask_small },
    [PC_ICON_LINK] = { "link", 7, 13, ico_link },
    [PC_ICON_LOCK_TRANSPARENT] = { "lock_transparent", 13, 13, ico_lock_transparent },
    [PC_ICON_LOCK_PIXELS] = { "lock_pixels", 13, 13, ico_brush_small },
    [PC_ICON_LOCK_POSITION] = { "lock_position", 13, 13, ico_lock_position },
    [PC_ICON_LOCK_ALL] = { "lock_all", 13, 13, ico_lock_all },
    [PC_ICON_TEXT_LAYER] = { "text_layer", 16, 16, ico_text_layer },
    [PC_ICON_FX] = { "fx", 13, 13, ico_fx },
    [PC_ICON_NEW_LAYER] = { "new_layer", 16, 16, ico_new_layer },
    [PC_ICON_TRASH] = { "trash", 16, 16, ico_trash },
    [PC_ICON_ADD_MASK] = { "add_mask", 16, 16, ico_add_mask },
    [PC_ICON_NEW_GROUP] = { "new_group", 16, 16, ico_new_group },
    [PC_ICON_LAYER_STYLE] = { "layer_style", 16, 16, ico_layer_style },
    [PC_ICON_NEW_ADJUSTMENT] = { "new_adjustment", 16, 16, ico_new_adjustment },
    [PC_ICON_LOAD_SELECTION] = { "load_selection", 16, 16, ico_load_selection },
    [PC_ICON_SAVE_SELECTION] = { "save_selection", 16, 16, ico_add_mask },
    [PC_ICON_FILL_PATH] = { "fill_path", 16, 16, ico_fill_path },
    [PC_ICON_STROKE_PATH] = { "stroke_path", 16, 16, ico_stroke_path },
    [PC_ICON_PATH_TO_SELECTION] = { "path_to_selection", 16, 16, ico_path_to_selection },
    [PC_ICON_SELECTION_TO_PATH] = { "selection_to_path", 16, 16, ico_selection_to_path },
    [PC_ICON_SNAPSHOT] = { "snapshot", 16, 16, ico_snapshot },
    [PC_ICON_NEW_DOC_FROM_STATE] = { "new_doc_from_state", 16, 16, ico_new_doc_from_state },
    [PC_ICON_HISTORY_SOURCE] = { "history_source", 13, 13, ico_history_source },
    [PC_ICON_ACTION_STOP] = { "action_stop", 16, 16, ico_action_stop },
    [PC_ICON_ACTION_RECORD] = { "action_record", 16, 16, ico_action_record },
    [PC_ICON_ACTION_PLAY] = { "action_play", 16, 16, ico_action_play },
    [PC_ICON_ACTION_NEW_SET] = { "action_new_set", 16, 16, ico_new_group },
    [PC_ICON_ACTION_NEW] = { "action_new", 16, 16, ico_new_layer },
    [PC_ICON_CHECK] = { "check", 9, 9, ico_check },
    [PC_ICON_DIALOG_TOGGLE] = { "dialog_toggle", 11, 9, ico_dialog_toggle },
    [PC_ICON_ZOOM_OUT_MOUNTAIN] = { "zoom_out_mountain", 12, 9, ico_zoom_out_mountain },
    [PC_ICON_ZOOM_IN_MOUNTAIN] = { "zoom_in_mountain", 16, 11, ico_zoom_in_mountain },
    [PC_ICON_CAPTION_CLOSE] = { "caption_close", 8, 7, ico_caption_close },
    [PC_ICON_CAPTION_MIN] = { "caption_min", 6, 2, ico_caption_min },
    [PC_ICON_CAPTION_MAX] = { "caption_max", 9, 9, ico_caption_max },
    [PC_ICON_CAPTION_RESTORE] = { "caption_restore", 8, 9, ico_caption_restore },
    [PC_ICON_ARROW_UP] = { "arrow_up", 7, 4, ico_arrow_up },
    [PC_ICON_ARROW_DOWN] = { "arrow_down", 7, 4, ico_arrow_down },
    [PC_ICON_ARROW_LEFT] = { "arrow_left", 4, 7, ico_arrow_left },
    [PC_ICON_ARROW_RIGHT] = { "arrow_right", 4, 7, ico_arrow_right },
    [PC_ICON_COMBO_ARROW] = { "combo_arrow", 7, 4, ico_arrow_down },
    [PC_ICON_MENU_CHECK] = { "menu_check", 7, 7, ico_menu_check },
    [PC_ICON_MENU_RADIO] = { "menu_radio", 6, 6, ico_menu_radio },
    [PC_ICON_SUBMENU_ARROW] = { "submenu_arrow", 4, 7, ico_arrow_right },
    [PC_ICON_SIZE_GRIP] = { "size_grip", 12, 12, ico_size_grip },
    [PC_ICON_WARNING] = { "warning", 32, 32, ico_warning },
    [PC_ICON_INFO] = { "info", 32, 32, ico_info },
    [PC_ICON_QUESTION] = { "question", 32, 32, ico_question },
    [PC_ICON_ERROR] = { "error", 32, 32, ico_error },
    [PC_ICON_FOLDER] = { "folder", 16, 16, ico_new_group },
    [PC_ICON_FOLDER_OPEN] = { "folder_open", 16, 16, ico_folder_open },
    [PC_ICON_FILE] = { "file", 16, 16, ico_new_layer },
    [PC_ICON_FILE_IMAGE] = { "file_image", 16, 16, ico_file_image },
    [PC_ICON_DRIVE_HDD] = { "drive_hdd", 16, 16, ico_drive_hdd },
    [PC_ICON_DRIVE_FLOPPY] = { "drive_floppy", 16, 16, ico_drive_floppy },
    [PC_ICON_DRIVE_CD] = { "drive_cd", 16, 16, ico_drive_cd },
    [PC_ICON_DRIVE_NET] = { "drive_net", 16, 16, ico_drive_net },
    [PC_ICON_UP_FOLDER] = { "up_folder", 16, 16, ico_up_folder },
    [PC_ICON_NEW_FOLDER] = { "new_folder", 16, 16, ico_new_folder },
    [PC_ICON_VIEW_LIST] = { "view_list", 16, 16, ico_view_list },
    [PC_ICON_VIEW_DETAILS] = { "view_details", 16, 16, ico_view_details },
};

static const PcCursorDef g_cursors[PC_CURSOR_COUNT] = {
    [PC_CURSOR_CROSSHAIR] = { "crosshair", 32, 32, 15, 15, cur_crosshair },
    [PC_CURSOR_MOVE] = { "move", 32, 32, 1, 1, cur_move },
    [PC_CURSOR_HAND] = { "hand", 32, 32, 9, 9, cur_hand },
    [PC_CURSOR_GRAB] = { "grab", 32, 32, 9, 9, cur_grab },
    [PC_CURSOR_ZOOM_IN] = { "zoom_in", 32, 32, 7, 7, cur_zoom_in },
    [PC_CURSOR_ZOOM_OUT] = { "zoom_out", 32, 32, 7, 7, cur_zoom_out },
    [PC_CURSOR_ZOOM_NONE] = { "zoom_none", 32, 32, 7, 7, cur_zoom_none },
    [PC_CURSOR_EYEDROPPER] = { "eyedropper", 32, 32, 2, 15, cur_eyedropper },
    [PC_CURSOR_BUCKET] = { "bucket", 32, 32, 4, 15, cur_bucket },
    [PC_CURSOR_PENCIL] = { "pencil", 32, 32, 2, 15, cur_pencil },
    [PC_CURSOR_BRUSH] = { "brush", 32, 32, 2, 16, cur_brush },
    [PC_CURSOR_AIRBRUSH] = { "airbrush", 32, 32, 2, 9, cur_airbrush },
    [PC_CURSOR_STAMP] = { "stamp", 32, 32, 8, 14, cur_stamp },
    [PC_CURSOR_STAMP_SOURCE] = { "stamp_source", 32, 32, 15, 15, cur_stamp_source },
    [PC_CURSOR_ERASER] = { "eraser", 32, 32, 2, 15, cur_eraser },
    [PC_CURSOR_LASSO] = { "lasso", 32, 32, 6, 15, cur_lasso },
    [PC_CURSOR_LASSO_POLY] = { "lasso_poly", 32, 32, 6, 15, cur_lasso_poly },
    [PC_CURSOR_WAND] = { "wand", 32, 32, 12, 4, cur_wand },
    [PC_CURSOR_CROP] = { "crop", 32, 32, 4, 4, cur_crop },
    [PC_CURSOR_IBEAM] = { "ibeam", 32, 32, 15, 15, cur_ibeam },
    [PC_CURSOR_IBEAM_VERTICAL] = { "ibeam_vertical", 32, 32, 15, 15, cur_ibeam_vertical },
    [PC_CURSOR_PEN] = { "pen", 32, 32, 8, 14, cur_pen },
    [PC_CURSOR_PEN_ADD] = { "pen_add", 32, 32, 6, 13, cur_pen_add },
    [PC_CURSOR_PEN_DELETE] = { "pen_delete", 32, 32, 6, 13, cur_pen_delete },
    [PC_CURSOR_PEN_CLOSE] = { "pen_close", 32, 32, 6, 13, cur_pen_close },
    [PC_CURSOR_DIRECT_SELECT] = { "direct_select", 32, 32, 5, 1, cur_direct_select },
    [PC_CURSOR_MEASURE] = { "measure", 32, 32, 1, 14, cur_measure },
    [PC_CURSOR_SMUDGE] = { "smudge", 32, 32, 2, 12, cur_smudge },
    [PC_CURSOR_BLUR] = { "blur", 32, 32, 8, 11, cur_blur },
    [PC_CURSOR_SHARPEN] = { "sharpen", 32, 32, 8, 2, cur_sharpen },
    [PC_CURSOR_DODGE] = { "dodge", 32, 32, 12, 6, cur_dodge },
    [PC_CURSOR_BURN] = { "burn", 32, 32, 7, 5, cur_burn },
    [PC_CURSOR_SPONGE] = { "sponge", 32, 32, 8, 8, cur_sponge },
    [PC_CURSOR_GRADIENT] = { "gradient", 32, 32, 15, 15, cur_gradient },
    [PC_CURSOR_MARQUEE] = { "marquee", 32, 32, 10, 10, cur_marquee },
    [PC_CURSOR_NO] = { "no", 32, 32, 15, 15, cur_no },
    [PC_CURSOR_ROTATE] = { "rotate", 32, 32, 15, 15, cur_rotate },
    [PC_CURSOR_SCALE_NS] = { "scale_ns", 32, 32, 15, 15, cur_scale_ns },
    [PC_CURSOR_SCALE_EW] = { "scale_ew", 32, 32, 15, 15, cur_scale_ew },
    [PC_CURSOR_SCALE_NWSE] = { "scale_nwse", 32, 32, 15, 15, cur_scale_nwse },
    [PC_CURSOR_SCALE_NESW] = { "scale_nesw", 32, 32, 16, 15, cur_scale_nesw },
    [PC_CURSOR_MARQUEE_ADD] = { "marquee_add", 32, 32, 10, 10, cur_marquee_add },
    [PC_CURSOR_MARQUEE_SUB] = { "marquee_sub", 32, 32, 10, 10, cur_marquee_sub },
    [PC_CURSOR_MARQUEE_INT] = { "marquee_int", 32, 32, 10, 10, cur_marquee_int },
    [PC_CURSOR_MOVE_SELECTION] = { "move_selection", 32, 32, 1, 1, cur_move_selection },
};

const PcIconDef *pc_icon_def(int id)
{
    if (id < 0 || id >= PC_ICON_COUNT)
        return 0;
    return &g_icons[id];
}

const PcCursorDef *pc_cursor_def(int id)
{
    if (id < 0 || id >= PC_CURSOR_COUNT)
        return 0;
    return &g_cursors[id];
}

int pc_icon_rgb(char c, unsigned *rgb)
{
    unsigned v;
    switch (c) {
    case 'k': v = 0x000000u; break;
    case 'm': v = 0x800000u; break;
    case 'G': v = 0x008000u; break;
    case 'o': v = 0x808000u; break;
    case 'n': v = 0x000080u; break;
    case 'p': v = 0x800080u; break;
    case 't': v = 0x008080u; break;
    case 'g': v = 0xC0C0C0u; break;
    case 'd': v = 0x808080u; break;
    case 'R': v = 0xFF0000u; break;
    case 'L': v = 0x00FF00u; break;
    case 'Y': v = 0xFFFF00u; break;
    case 'B': v = 0x0000FFu; break;
    case 'F': v = 0xFF00FFu; break;
    case 'C': v = 0x00FFFFu; break;
    case 'w': v = 0xFFFFFFu; break;
    default: return 0;
    }
    if (rgb)
        *rgb = v;
    return 1;
}
