# PhotoCraft Classic: historical UI reference and UI specification

> **Status:** v1 specification, 2026-10-30. **Audience:** whoever implements `src/ui/`.
> **Scope:** where everything goes (Photoshop CS6 for Windows, 2012) and how it is drawn
> (Windows 95/98 and Photoshop 5.0 for Windows, May 1998).

PhotoCraft Classic takes its **arrangement** from Photoshop CS6. The menu bar is on top, the
Options bar runs along the top, a one-column Tools panel is docked at the left, a panel dock sits at
the right (an icon column plus a column of tabbed groups), and documents open as tabs in an
application frame. Every pixel of that arrangement is then **drawn** the way a Windows 95/98
program of 1998 drew things: gray `#C0C0C0` bevels, navy captions, MS Sans Serif 8, 16-color
icons and the palette details of Photoshop 5.0 (palette tabs, pop-up sliders, triangle palette
menus). Photoshop 5.0 is also the reference for dialogs, palette internals, menu density and the
feature set.

**Legal guard rails, which apply to every section of this document.**

- We copy *behaviour and layout*, never artwork, code, icons, logos, splash screens or the
  "Adobe"/"Photoshop" names.
- Generic command and tool names that are industry vocabulary (Levels, Curves, Marquee, Lasso,
  Clone Stamp, Layers…) are fine.
- Our icons are drawn from scratch in the 16-color VGA palette.
- The About box, title bar and installer say **PhotoCraft Classic** and nothing that implies
  Adobe.
- Period screenshots are referenced by URL in the table at the end. They are **not** committed.
  Only our own measurements taken from them appear here.

---

## 0. Sources: what was and was not accessible

All research was done on 2026-10-30 from this session through its egress proxy.

| Source | Result | Used for |
|---|---|---|
| Photoshop 5.0 User Guide on ManualShelf, `https://www.manualshelf.com/manual/adobe/photoshop-5-0/user-guide-english/page-N.html`, all 400 pages | **Accessed.** I downloaded all 400 pages and extracted their text. ManualShelf page N = printed guide page N−8, so page-34 is guide p. 26. | Toolbox and shortcuts (p.25), work area (pp.26–37), Options palette and pop-up sliders (p.30), Info (p.31), Navigator and zoom (pp.33–36), Preferences (p.37), History (pp.167–172), Levels/Curves/Color Balance/Hue-Sat/Brightness-Contrast/Unsharp Mask (pp.117–130), Layers (pp.259–292), Numeric Transform (p.187), index |
| Photoshop 5.0 User Guide on archive.org, `https://archive.org/details/adobephotosh00adob` | **Partly accessed.** The item page loads, but it is a lending-library item: `/download/…_djvu.txt` returns HTTP 401, and the `/stream/` full-text page is an empty shell. | Not used. ManualShelf has the same book. |
| GUIdebook, `https://guidebookgallery.org/apps/photoshop/500` (Photoshop 5.0 running on Windows 98, by Marcin Wichary) | **Accessed.** I downloaded 20 screenshots to a scratch folder (they are not in the repo) and measured them pixel by pixel. | Every pixel measurement marked "measured" below |
| Photoshop CS6 Help PDF, `https://help.adobe.com/archive/en/photoshop/cs6/photoshop_reference.pdf` (11.7 MB) | **Accessed** and converted with `pdftotext`. | Workspace basics (application frame, docking, panel groups, collapse to icons, document tabs), Window > Arrange commands, screen modes, status bar menu, tool shortcut table |
| Photoshop Essentials, "Managing Panels in Photoshop CS6", `https://www.photoshopessentials.com/basics/managing-panels-in-photoshop-cs6/` | **Accessed.** | Default CS6 Essentials contents: icon column *History, Properties*; main column *Color\|Swatches, Adjustments\|Styles, Layers\|Channels\|Paths*. Panel screenshots. |
| `https://www.webdesignmuseum.org/software/adobe-photoshop-5-0-in-1998` | **Not accessible** (HTTP 403 from this session; not retried with a different identity). | — |
| `https://helpx.adobe.com/…` (current Adobe help) | **Not accessible** (HTTP 403). The CS6 PDF above covers the same ground. | — |
| `https://web.archive.org/…` (Wayback Machine) | **Not accessible.** The tunnel closed mid-exchange, then HTTP 429. | — |
| Wine source: `dlls/win32u/sysparams.c` (current), and `windows/syscolor.c` at tag wine-20041201 (via goma.googlesource.com) | **Accessed.** | Default system colors (the 2004 table is the Win95/98 "Windows Standard" scheme; the current one is the Windows 2000 classic scheme) and default metrics in twips |
| Microsoft Learn: `DrawEdge`, UX guide "Layout" | **Accessed.** | Edge flag definitions; DLU conventions |
| Paint Shop Pro 5: irt.org review `https://www.irt.org/software/sw003/` | **Accessed.** | Feature context (layers, undo history, picture tubes) |
| Corel Photo-Paint 8, Ulead PhotoImpact 4 | **Search snippets only**: MacAddict/Macworld/TidBITS mentions for Photo-Paint 8; a kaisernet review and Route Fifty (1998) for PhotoImpact. No manuals or screenshots were reachable. | Period details from those snippets and from memory, marked *(unverified)* |
| manualslib.com, winworldpc.com, design.tutsplus.com | Not accessible (HTTP 403/451). | — |

Where a number comes from my own memory of these programs rather than from a source above, it is
marked *(recollection)*.

---

## 1. The synthesis in one table

| Element | Arrangement from CS6 | Drawing from Win95/98 + PS 5.0 |
|---|---|---|
| Main window | Application frame, maximized by default | Standard overlapped window: 4 px raised sizing frame, 18 px caption with Win98 gradient `#000080→#1084D0` (solid navy on Win95 or below 16 bpp), MS Sans Serif 8 Bold white caption text |
| Menu bar | Menus: File Edit Image Layer Type Select Filter View Window Help (CS6 minus 3D) | Custom-drawn and 19 px tall. An open item is drawn **sunken** by 1 px (measured in PS5 on Win98). Navy highlight inside popups. Etched separators. Underlined mnemonics always visible. |
| Options bar | One horizontal 30 px bar under the menu, contents change per tool; workspace switcher at the right end | A rebar-style band with an etched gripper. Controls are PS5 Options-palette widgets: `Opacity: [100]%▸` pop-up sliders, 21 px combo boxes, MS Sans Serif 8 labels ending in ":" |
| Tools | Single column docked at the left; a « » chevron switches to two columns; CS6 grouping and order; color chips, Quick Mask and Screen Mode at the bottom | PS5 toolbox grid: 25×21 px cells sharing 1 px black lines, white/gray bevel, a 3 px black flyout triangle at bottom-right, 16-color glyphs |
| Panels | Right dock: icon column (32 px) plus expanded column (214 px) of tabbed groups. Groups can be dragged, stacked, collapsed, floated. | PS5 palette internals: trapezoid tabs (active tab white and bold), ▸ palette-menu button, size grip. Floating palettes get a PS5 12 px navy mini caption. |
| Documents | Tabs `name @ 66.7% (Layer 1, RGB/8)`. Float, Cascade, Tile, Consolidate. Per-document status bar. | Tabs in Win95 property-sheet style (18 px). Floating documents are MDI-style child windows with the Win98 gradient caption. The status bar is the PS5 one (zoom field, `Doc: 795K/795K`, ▸ menu). |
| Screen modes | Standard, Full Screen With Menu Bar, Full Screen; F cycles | Gray `#808080` or black backdrop |
| Dialogs | — | PS5 layouts and measured sizes (§9) |

---

## 2. Windows 95/98 visual foundation

### 2.1 System colors

At run time **always** use `GetSysColor()`, so the program follows the user's Appearance scheme
(including High Contrast and the 16-color VGA fallbacks). The table gives the defaults of the
"Windows Standard" scheme. They are used for rendering tests, for screenshots and if
`GetSysColor` fails.

| Index | Constant | Win95/98 "Windows Standard" | Win2000 classic (for comparison) | Our use |
|---|---|---|---|---|
| 0 | `COLOR_SCROLLBAR` | `#C0C0C0` | `#D4D0C8` | Scroll bar track (dithered with 3DHILIGHT, §2.4) |
| 1 | `COLOR_BACKGROUND` | `#008080` (teal desktop) | `#3A6EA5` | — |
| 2 | `COLOR_ACTIVECAPTION` | `#000080` | `#0A246A` | Main/floating-doc caption, start of the gradient. Floating palette mini caption (solid). |
| 27 | `COLOR_GRADIENTACTIVECAPTION` (Win98/2000 only) | `#1084D0` | `#A6CAF0` | End of the gradient (right side) |
| 3 | `COLOR_INACTIVECAPTION` | `#808080` | `#808080` | Inactive caption start |
| 28 | `COLOR_GRADIENTINACTIVECAPTION` (98/2000) | `#B5B5B5` | `#C0C0C0` | Inactive caption end |
| 9 | `COLOR_CAPTIONTEXT` | `#FFFFFF` | `#FFFFFF` | Active caption text |
| 19 | `COLOR_INACTIVECAPTIONTEXT` | `#C0C0C0` | `#D4D0C8` | Inactive caption text |
| 4 | `COLOR_MENU` | `#C0C0C0` | `#D4D0C8` | Menu bar and popup background |
| 7 | `COLOR_MENUTEXT` | `#000000` | `#000000` | Menu text |
| 13 | `COLOR_HIGHLIGHT` | `#000080` | `#0A246A` | Selected menu item, selected list rows, active-layer row, text selection |
| 14 | `COLOR_HIGHLIGHTTEXT` | `#FFFFFF` | `#FFFFFF` | Text on highlight |
| 5 | `COLOR_WINDOW` | `#FFFFFF` | `#FFFFFF` | Edit fields, list views, active palette tab |
| 8 | `COLOR_WINDOWTEXT` | `#000000` | `#000000` | |
| 6 | `COLOR_WINDOWFRAME` | `#000000` | `#000000` | 1 px frames (popup sliders, canvas outline) |
| 15 | `COLOR_3DFACE` / `BTNFACE` | `#C0C0C0` | `#D4D0C8` | Everything gray |
| 22 | `COLOR_3DLIGHT` | **`#DFDFDF`** (measured in PS5 on Win98: window frame, buttons. Wine 2004 has `#E0E0E0`.) | `#D4D0C8` | Second bevel line, top/left |
| 20 | `COLOR_3DHILIGHT` / `BTNHIGHLIGHT` | `#FFFFFF` | `#FFFFFF` | Outer bevel line, top/left |
| 16 | `COLOR_3DSHADOW` / `BTNSHADOW` | `#808080` | `#808080` | Bevel line, bottom/right |
| 21 | `COLOR_3DDKSHADOW` | `#000000` | `#404040` | Outer bevel line, bottom/right |
| 18 | `COLOR_BTNTEXT` | `#000000` | `#000000` | |
| 17 | `COLOR_GRAYTEXT` | `#808080` | `#808080` | Disabled text (with a white emboss, §2.5) |
| 12 | `COLOR_APPWORKSPACE` | `#808080` | `#808080` | Empty document area. Backdrop for "Full Screen With Menu Bar". |
| 24 | `COLOR_INFOBK` | `#FFFFE1` | `#FFFFE1` | Tooltips |
| 23 | `COLOR_INFOTEXT` | `#000000` | `#000000` | |
| 26 | `COLOR_HOTLIGHT` (98+) | `#0000FF` | `#0000C8` | Hyperlinks in About/Update dialogs only |

**Rules**

- **Gradient captions.** Draw one only when
  `SystemParametersInfo(SPI_GETGRADIENTCAPTIONS)` succeeds and returns TRUE (Win98/2000+) **and**
  the display has at least 15 bpp. Otherwise fill solid `COLOR_ACTIVECAPTION`.
  `GetSysColor(27/28)` on Windows 95 and NT 4 returns 0, so never call it there.
- **The PS5 gradient is linear in RGB** from left to right. Measured on PS5/Win98:
  `#000080` at x=4 → `#032093` at x=199 → `#1084D0` at the right end.
  - On 256-color displays Win98 itself draws a solid caption. We do the same.
- **16-color displays.** The only colors that survive are the 16 VGA colors. `#DFDFDF` maps to
  `#C0C0C0` there, which is fine (the light and face lines merge).
  - All chrome must stay readable with `3DLIGHT = 3DFACE`.
  - Icons use only the 16 VGA colors (§8).
- **"Windows Standard" defaults vs Win2000.** Wine's current defaults are the Windows 2000 classic
  scheme (`#D4D0C8`, `#0A246A`). They are **not** the Win95/98 values. Our fallback table is the
  Win95/98 column.

### 2.2 Metrics (96 dpi "Small Fonts" display)

| Metric | Value | Source / notes |
|---|---|---|
| Sizing frame `SM_CXFRAME`/`SM_CYFRAME` | 4 px | Measured. The PS5 main window frame is `#DFDFDF, #FFFFFF, #C0C0C0, #C0C0C0` on top/left, mirrored with `#808080, #000000` on bottom/right. |
| Fixed dialog frame `SM_CXDLGFRAME` | 3 px | Measured (dialogs: `#DFDFDF, #FFFFFF, #C0C0C0`, then caption) |
| Caption area `SM_CYCAPTION` | 19 px (18 px bar + 1 px line). Wine twips −270 = 18 px. | Measured: PS5 caption y=4…21 is 18 px. The menu starts at y=22. |
| Caption buttons | 16×14 px, 2 px gap between Min and Max, Close separated by 2 px | Measured (doc window: x=393…408 by y=50…63) |
| Small caption `SM_CYSMCAPTION` | 16 px (15+1) | Wine twips −225. We use our own PS5 palette caption instead (12 px, §6.1). |
| Menu bar `SM_CYMENU` | 19 px | Wine twips −270 → 18 px + 1. Measured: an open menu-bar item's sunken box spans y=1…18. |
| Menu bar item | Text plus 6 px padding each side; item box 18 px tall | Measured: "File" box x=1…27 |
| Popup menu item height | **17 px** (MS Sans Serif 8: tmHeight 13 + 4) | Measured: the navy highlight spans y=340…356 |
| Popup separator | 9 px item, etched line (`#808080` over `#FFFFFF`) at its vertical centre | Measured |
| Popup frame | 3 px: outer `3DLIGHT`/`3DDKSHADOW`, inner `3DHILIGHT`/`3DSHADOW`, then 1 px face | Measured (x=0 `#DFDFDF`, x=1 `#FFFFFF`, x=2 `#C0C0C0`) |
| Popup text indent | Text starts 21 px right of the highlight's left edge (check-mark column). Accelerator text is right-aligned, 16 px gap minimum. Submenu arrow 4×7 px, 6 px from the right. | Measured approx. ±1 px |
| Scroll bar `SM_CXVSCROLL`/`SM_CYHSCROLL` | 16 px; arrow buttons 16×16; minimum thumb 8 px | Wine twips −240 |
| Border `SM_CXBORDER` | 1 px | |
| Edge `SM_CXEDGE` | 2 px | |
| Check box glyph | 13×13 px (sunken: `#808080`/`#FFFFFF` outer, `#000000`/`#DFDFDF` inner, white fill, 7×7 black check) | |
| Radio glyph | 12×12 px | |
| Push button (Win95 guideline) | 50×14 DLU = **75×23 px** | Win95 guidelines *(recollection)*. Used in our file dialogs and Setup. |
| Push button (PS5 dialogs) | **60×20 px**, 8 px vertical gap (28 px pitch) | Measured in Levels: OK x=319…378, y=43…62; Cancel y=71…90 |
| Edit field (PS5 dialogs) | 20 px tall (sunken 2 px edge + 16 px interior) | Measured (Image Size width field: 70×20) |
| Combo box (drop-down list) | 21 px collapsed. Drop button 16×17 inside a 2 px sunken edge. | Win95 *(recollection)*; PS5 shows 20–21 |
| Group box | Etched 2 px frame; caption text sits on the line, inset 8 px, with 2 px face padding each side | Measured (`#808080` then `#FFFFFF`) |
| Dialog margins | 11 px (7 DLU) to the frame; 6 px (4 DLU) between related controls | Win95 guidelines *(recollection)*; PS5 dialogs use 12–14 px |
| Taskbar (bottom, default) | 28 px | Win95 default *(recollection)* |

**Dialog units.** With MS Sans Serif 8 at 96 dpi the dialog base units are **6×13 px**:

- 1 horizontal DLU = 6/4 = 1.5 px.
- 1 vertical DLU = 13/8 = 1.625 px.
- `px_x = MulDiv(dlu_x, baseX, 4)` and `px_y = MulDiv(dlu_y, baseY, 8)`.
- At 120 dpi "Large Fonts" the base units become 8×16. Always compute them with
  `GetDialogBaseUnits()` or `MapDialogRect()` on a dialog that uses our font. Never hard-code
  pixels in resource templates; the pixel numbers in this document are for 96 dpi.

### 2.3 Fonts

| Use | Face | Size | Notes |
|---|---|---|---|
| Captions (main, floating documents, dialogs) | MS Sans Serif | 8 pt **Bold** | `CreateFont(-11, …, FW_BOLD, …)`. Read the real face from `SPI_GETNONCLIENTMETRICS.lfCaptionFont` when available. |
| Menus, dialogs, panels, options bar, status bar, tabs | MS Sans Serif | 8 pt regular | `lfHeight = -MulDiv(8, 96, 72) = -11` → cell height **13 px** (ascent 11, descent 2). Measured PS5 glyphs: cap height 7, x-height 5, descender 2. |
| Active palette tab label | MS Sans Serif | 8 pt **Bold** | Measured: stems 2 px wide. Inactive tabs are regular weight. |
| Active document tab label | MS Sans Serif | 8 pt regular | Bold would widen the tab; the selected tab is shown by geometry instead. |
| Rulers, Navigator zoom readout in tight spots, Info palette secondary labels | Small Fonts | 6–7 pt | Bitmap face `smalle.fon`, sizes 2–7 pt. Measure with `GetTextMetrics` at run time (cell ≈ 9–10 px at 7 pt). |
| Type tool | Any installed TrueType face | User | Via GDI `GetGlyphOutline` (available on Win95). |

- **MS Sans Serif is a bitmap font** (`sserife.fon` at 96 dpi; `sseriff.fon` at 120 dpi) with
  sizes 8, 10, 12, 14, 18 and 24 pt.
- Never ask for sizes in between: GDI would substitute or scale them badly.
- Never synthesize italics, except the PS5 Layers palette, which shows the *Background* layer name
  in italics (measured). GDI will synthesize that from the bitmap face, which matches the
  original look.
- **Do not** use Tahoma (Win2000) or "MS Shell Dlg 2". On NT 4/2000+, "MS Shell Dlg" maps to
  MS Sans Serif, which is fine.
- No ClearType or antialiasing: `NONANTIALIASED_QUALITY` or `DEFAULT_QUALITY`.

### 2.4 Bevel construction (pixel layouts)

The notation is outer ring → inner ring, written as *top/left colour | bottom/right colour*.
Light = `3DLIGHT #DFDFDF`, Hi = `3DHILIGHT #FFFFFF`, Sh = `3DSHADOW #808080`,
Dk = `3DDKSHADOW #000000`, Face = `#C0C0C0`.

| Element | Ring 1 (outer) | Ring 2 | Fill | Notes |
|---|---|---|---|---|
| Window/dialog frame (`EDGE_RAISED` = `BDR_RAISEDOUTER\|BDR_RAISEDINNER`) | Light \| Dk | Hi \| Sh | Face | Measured on PS5 main window and dialogs |
| Push button, normal (`DrawFrameControl DFCS_BUTTONPUSH`) | **Hi \| Dk** | **Light \| Sh** | Face | Measured (Levels Cancel: x=319 `#FFFFFF`, 320 `#DFDFDF`, …, 377 `#808080`, 378 `#000000`). Note the order is swapped relative to window frames. |
| Push button, default (OK) | 1 px Dk all round | then Hi \| Dk, Light \| Sh | Face | Measured: OK has a black outer ring |
| Push button, pressed | Dk all round (1 px) | Sh all round (1 px) | Face | Label offset **+1,+1 px**. Flat pressed look as in Win95. |
| Push button, focused | — | — | — | Dotted focus rectangle (`DrawFocusRect`, 1 on/1 off, XOR) inset 4 px from the outer edge |
| Edit, list, sunken field (`EDGE_SUNKEN`) | **Sh \| Hi** | **Dk \| Light** | Window `#FFFFFF` | Measured (Image Size field: y=58 `#808080`, 59 `#000000`, …, 78 `#DFDFDF`, 79 `#FFFFFF`) |
| Static thin sunken (`BDR_SUNKENOUTER`) | Sh \| Hi | — | Face | Status-bar panes, readouts |
| Static thin raised (`BDR_RAISEDINNER`) | Hi \| Sh | — | Face | Options-bar band, dock header, toolbar buttons in the hot state |
| Etched line / group box (`EDGE_ETCHED`) | Sh \| Hi | Hi \| Sh | — | Horizontal separator = a Sh row over a Hi row |
| Toolbox cell, normal (PS5) | 1 px Dk grid line (shared with neighbours) | Hi \| Sh | Face | Measured: cell pitch 25×21 px, interior 23×19 |
| Toolbox cell, selected (PS5) | Dk grid line | Sh top/left only | **Hi (white) fill** | Measured: PS5 shows the active tool on a white face |
| Toolbar check button, pressed/latched (options bar mode buttons) | Sh \| Hi | Dk \| Light (thin) | Dither of Hi and Face (50% checkerboard) | Win95 "checked" toolbar look |
| Scroll bar track | — | — | 50% checkerboard of `COLOR_SCROLLBAR` and Hi | Pressed page area: checkerboard of Dk |

**Disabled text.**

- Draw the string in Hi at (+1,+1), then in Sh (`GRAYTEXT`) at (0,0). This is the "embossed" look
  of Win95 controls and menus.
- On a navy highlight (a selected disabled menu item), draw GRAYTEXT only, with no emboss.

### 2.5 Menus (custom-drawn; we do not use `HMENU` rendering)

**Why custom-drawn.** We need identical rendering on Win95…10 (Win2000+ flattens menus and hides
mnemonics, and XP+ themes the menu bar). We need CS6-style dynamic items such as the workspace
list and panel check marks. And full-screen-with-menu-bar mode must show the menu bar inside a
captionless window. See `design-decisions.md`.

**Behaviour (Win95/98 classic).**

1. **No hover effect** on the menu bar while no menu is open. When a menu is open, moving across
   the bar opens the neighbouring menu (menu-tracking mode).
2. **Open item**: a 1 px sunken bevel (Sh top/left, Hi bottom/right) around the item box. The text
   stays black and is offset +1,+1 (measured on PS5/Win98).
3. **Keyboard activation** (Alt alone or F10) shows a 1 px **raised** bevel on the first item.
   Arrows move it. Enter or Down opens the menu. Esc steps back. *(recollection: Win95 behaviour)*
4. **Mouse**: the menu opens on button down; the user can drag and release on an item to choose
   it. A click opens the menu and leaves it open.
5. **Popup items**:
   - The selected item is filled with navy (`COLOR_HIGHLIGHT`) across the full item width inside
     the frame, with white text.
   - Disabled items use embossed gray text.
   - The accelerator is right-aligned.
   - A submenu has a black ▸ (4×7) arrow and opens after a 400 ms hover, or immediately on click
     or Right.
6. **Check marks**: a 7×7 black ✓ glyph in the 21 px left column. Radio-style exclusive items
   (Screen Mode, Workspace) use a 5×5 ● bullet.
7. **Mnemonics are always underlined** (Win95/98 behaviour). `SPI_GETKEYBOARDCUES` is ignored.
8. **Separators** are an etched 2 px line in a 9 px item, inset 1 px from the frame.
9. **Popups have no drop shadow and no animation.** They are clipped to the monitor work area and
   flip left or up when needed.
10. **Accelerator text uses PS5's Windows notation**, measured in the PS5 File menu:
    `Ctrl+N`, `Alt+Ctrl+O`, `Shft+Ctrl+S`, `Shft+Ctrl+P`.
    - Order the modifiers *Alt, Shft, Ctrl*, e.g. `Alt+Shft+Ctrl+L`.
    - This period detail is cheap to keep.
    - The command tables in §4 use CS6 spelling for readability.
11. **Long menus** (Filter, Window): a menu taller than the work area gets scroll arrows (▲/▼
    strips 9 px tall), as in Win98.

### 2.6 Other control conventions

- **Pressed buttons move their label +1,+1.** Toolbox glyphs, options-bar buttons and tab labels
  do the same.
- **Tooltips**: `COLOR_INFOBK` background, 1 px black frame, MS Sans Serif 8, 2 px padding.
  - They appear after 500 ms of hover and stay up for 5 s.
  - Tool tooltips read `Marquee Tool (M)`, PS5 style.
  - Preference: *Show Tool Tips* (PS5 General preferences).
- **Focus**: dotted rectangle around the label of the focused check box or radio button, and
  inside the focused push button.
- **Drop-down lists**: a white list with a 1 px black frame and navy selection. Up to 8 visible
  items, then a scroll bar.
- **No hot tracking anywhere** except two places: toolbox and options-bar flat buttons show a
  1 px raised bevel on hover (Office 97 and IE4 toolbars did this in 1997–98), and panel icon
  buttons in the dock do the same.

---

## 3. Main window layout

### 3.1 Fixed sizes

| Strip | Height/width | Contents |
|---|---|---|
| Caption | 19 px (18 + 1) | App icon 16×16, `PhotoCraft Classic` (CS6's application frame title is just the product name; the document name is in its tab), Min/Max/Close |
| Menu bar | **19 px** | `File Edit Image Layer Type Select Filter View Window Help` |
| Options bar | **30 px** | 1 px Hi top line, 1 px Sh bottom line, then a 2 px etched separator. Controls are vertically centred: 21 px controls get top padding 4 px and bottom 5 px. |
| Tools panel | **31 px** wide in one column; **56 px** in two columns | `1 + 2 + 25 + 2 + 1 = 31`; `1 + 2 + 25 + 1 + 25 + 1 + 1 = 56` (button cells share black lines) |
| Dock: icon column | **32 px** | `1 + 2 + 26 + 2 + 1`; icon buttons 26×24 |
| Dock: panel column | **214 px** default (resizable 180–400) | PS5 palettes measured 212 px wide including the frame; +2 px for the dock splitter edge |
| Dock header strip (each column) | 11 px | Gripper and «/» chevron button 11×9 |
| Document tab strip | **18 px** | Tabs 16 px tall plus 2 px for the selected tab's lift |
| Per-document bottom row | **17 px** | Zoom field + doc info + ▸ (status bar) on the left; horizontal scroll bar (16 px) on the right; 1 px Sh line on top |
| Vertical scroll bar | 16 px | |
| Splitters | 3 px between panel groups, 4 px hot zone at the dock's left edge (no visible gap) | Cursor: `IDC_SIZENS`/`IDC_SIZEWE` |

### 3.2 800×600, maximized, taskbar at the bottom (work area 800×572)

The non-client caption is 19 px, so the client area is **800×553**. Two-column tools are the
default here because the one-column panel needs about 527 px of height (§5) and only 504 px is
available.

```
x:0                                                                                         799
y:0  ┌──────────────────────────────────────────────────────────────────────────────────────┐ caption (non-client)
     │▣ PhotoCraft Classic                                                        [_][□][×] │ 19 px, Win98 gradient
     ├──────────────────────────────────────────────────────────────────────────────────────┤ client y=0
     │ File  Edit  Image  Layer  Type  Select  Filter  View  Window  Help                   │ menu bar 19 (y 0–18)
     ├──────────────────────────────────────────────────────────────────────────────────────┤ y=19
     │┊┊[⬚▾]│[▣][⊞][⊟][⊡]│ Feather:[0  ]px  ☐Anti-alias  Style:[Normal     ▾]  W:[  ] H:[  ]│[Essentials▾]│ options 30 (y 19–48)
     ├──┬───────────────────────────────────────────────────────────┬──┬─────────────────────┤ y=49
     │»«│ Untitled-1 @ 100% (RGB/8) × │ Shoes.psd @ 66.7% (Layer…×│  │«│»             header 11│
     │▒▒├───────────────────────────────────────────────────────────┤▒▒├─────────────────────┤
     │MV│┌─────────────────────────────────────────────────────────┐│Hi│/Color\Swatches\   [▸]│ y≈60
     │LW││                                                         ▲│Ac│ ■□ R ═══════▲══ [0] │ Color|Swatches
     │CI││                                                         ││  │    G ═══════▲══ [0] │ group 110
     │──││            canvas viewport 478 × 465                    ││──│    B ═══════▲══ [0] │
     │BS││          (pasteboard #C0C0C0 around image)              ││Br│ [spectrum ramp    ] │
     │YE││                                                         ││  ├─────────────────────┤ +3 splitter
     │GR││                                                         ││  │/Navigator\Info\ [▸] │ Navigator|Info: starts
     │O ││                                                         ││  ├─────────────────────┤ collapsed below 600 px (20)
     │──││                                                         ││  │/Layers\Channels\Paths│ Layers|Channels|Paths
     │PT││                                                         ││  │[Normal ▾] Opacity:[100]▸│ fills the rest:
     │AU││                                                         ▼│  │☐ Preserve Transparency│ 504−11−110−3−20−3 = 357
     │──│└─────────────────────────────────────────────────────────┘│  │👁│✎│[▓] Background │
     │HZ│[66.67%][Doc: 900K/900K ▸][◂▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▸][ ]│  │        ...          │
     │■□│                                                            │  │                     │
     │◐◑│                                                            │  │[⊕][❏][🗑]           │
     │▭ │                                                            │  │                     │
     └──┴───────────────────────────────────────────────────────────┴──┴─────────────────────┘ y=552
      0–55            document area x 56–553 (498 × 504)            554–585   586–799
      tools (2-col 56)                                              icons 32  panels 214
```

- Document area: 498×504. Tabs 18 → frame 2+2 → bottom row 17 → **canvas viewport 478×465**
  (498−4−16 by 504−18−4−17).
- Panel column below 600 px of workspace height: Navigator|Info starts collapsed to its tab row.
  The user can double-click a tab to expand it.

### 3.3 1024×768, maximized (work area 1024×740 → client 1024×721)

```
x:0                                                                                                           1023
     ┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
     │▣ PhotoCraft Classic                                                                            [_][□][×] │ caption 19
     ├──────────────────────────────────────────────────────────────────────────────────────────────────────────┤ client y=0
     │ File  Edit  Image  Layer  Type  Select  Filter  View  Window  Help                                       │ menu 19 (0–18)
     ├──────────────────────────────────────────────────────────────────────────────────────────────────────────┤
     │┊┊[✎▾]│Brush:[● 21▾] Mode:[Normal   ▾] Opacity:[100]%▸ Fade:[0 ]steps ☐Wet Edges        │ [Essentials ▾] │ options 30 (19–48)
     ├─┬─────────────────────────────────────────────────────────────────────────────┬──┬───────────────────────┤ y=49
     │»│ Shoes.psd @ 66.7% (Layer 1, RGB/8) * ×│ Untitled-1 @ 100% (RGB/8) ×│        «│»│               header │
     │M├─────────────────────────────────────────────────────────────────────────────┤──├───────────────────────┤
     │▭│┌───────────────────────────────────────────────────────────────────────────┐│Hi│/Color\Swatches\    [▸]│ Color|Swatches 110
     │L││                                                                           ▲│Ac│                        │ (y 60–169)
     │W││                                                                           ││──├───────────────────────┤
     │C││                                                                           ││Br│/Navigator\Info\    [▸]│ Navigator|Info 142
     │I││                                                                           ││  │ ┌────────────────┐    │ (y 173–314)
     │─││                 canvas viewport 727 × 633                                 ││  │ │  thumb + red   │    │
     │B││                                                                           ││  │ └────────────────┘    │
     │S││                                                                           ││  │[66.67%][▵]═▲══[▵▵] ◢  │
     │Y││                                                                           ││  ├───────────────────────┤
     │E││                                                                           ││  │/Layers\Channels\Paths\[▸]│ Layers|Channels|Paths
     │G││                                                                           ││  │[Normal  ▾] Opacity:[100]▸│ 672−11−110−3−142−3
     │R││                                                                           ││  │☐ Preserve Transparency │ = 403 (y 318–720)
     │O││                                                                           ││  │👁│✎│[▓▓] Layer 1      │
     │─││                                                                           ││  │👁│ │[▓▓] Background   │
     │P││                                                                           ▼│  │                        │
     │T│└───────────────────────────────────────────────────────────────────────────┘│  │                        │
     │A│[66.67%][Doc: 900K/1.2M ▸][◂▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▸][ ] │  │[⊕][❏][🗑]              │
     │U│                                                                             │  │                        │
     │─│                                                                             │  │                        │
     │H│                                                                             │  │                        │
     │Z│                                                                             │  │                        │
     │■│                                                                             │  │                        │
     │◐│                                                                             │  │                        │
     │▭│                                                                             │  │                        │
     └─┴─────────────────────────────────────────────────────────────────────────────┴──┴───────────────────────┘ y=720
      0–30                    document area x 31–777 (747 × 672)                      778–809  810–1023
```

- **Canvas viewport 727×633** (747−4−16 by 672−18−4−17).
- **640×480** (supported, not recommended): the client is about 640×433. Two-column tools (56),
  the panel column auto-collapses to the icon column (32), and the document area is 552×384. A
  click on an icon opens a flyout over the document area.

### 3.4 Workspace behaviour (CS6 semantics)

Sources: CS6 Help pp.34–38 and the Photoshop Essentials article.

- **Application frame.** All elements live inside one frame window. Resizing the frame re-lays out
  docks and documents so nothing overlaps.
- **Tab** hides or shows all panels including the Tools panel and Options bar. **Shift+Tab** hides
  or shows all panels except Tools and Options. Both keys are PS5 behaviour too (guide p.28).
- **Docks.**
  - Drag a panel by its tab, or a group by its title/tab-row background, into a dock. A **drop
    zone** highlights:
    - In CS6 the highlight is blue.
    - We draw an XOR-inverted 3 px bar (between groups) or a 3 px inverted frame (into a group),
      using `PatBlt(PATINVERT)` with a 50% gray dither brush. That is the period drag-feedback
      idiom: Win95 drag-full-windows-off rectangles.
  - **Ctrl** while dragging prevents docking. **Esc** cancels.
  - Dropping outside a zone creates a **floating palette** (PS5 look: 12 px navy mini caption).
  - Floating palettes can be stacked: drop on the bottom zone of another floating palette.
  - Removing all panels from a dock removes the dock. Dragging to the right edge of the frame
    creates a new dock column.
- **Panel groups.** Drag a tab within the row to reorder. Drag it out to float. Drag it into
  another group's tab row to join that group.
- **Minimize/expand.**
  - Double-clicking a tab or the empty tab-row area collapses the group to its tab row.
    Double-clicking again restores it. (CS6, and PS5 p.29.)
  - Floating palettes also have the PS5 minimize box (first click: default size; second click:
    collapse).
- **Resize.** Drag group splitters. The Color group has a fixed height (as in CS6 and PS5: Color,
  Info and Options cannot be resized).
- **Collapse to icons.**
  - The «/» chevron on the dock header collapses or expands the whole column.
  - Clicking a panel icon expands just that panel as a flyout.
  - The flyout collapses again when the user clicks its tab or icon, or the chevron in its
    header. With *Auto-Collapse Icon Panels* (preference), it collapses when the user clicks
    anywhere else.
  - Widening the icon column shows labels next to the icons (`History`, `Actions`…). Narrowing it
    back to 32 px hides them.
- **Window menu** lists every panel with a check mark. Choosing a hidden panel shows it where it
  was last docked.
- **Workspaces** (Window > Workspace):
  - `Essentials (Default)`.
  - `Classic 5.0`, our extra: the PS5 floating arrangement (§6.3). It is not a CS6 workspace and
    is marked [PCC].
  - `Reset Essentials`, `New Workspace…` (saves panel locations and the tools column mode),
    `Delete Workspace…`.
  - Workspaces are stored as INI files in `%APPDATA%`, or in the program folder on 9x, which has
    no per-user AppData.
  - The workspace switcher (a 100×21 drop-down) sits at the right end of the Options bar, as in
    CS6.

---

## 4. Menus (full tree for v1)

Tags:

- **[5]**: present in Photoshop 5.0 with this name (location may differ).
- **[CS6]**: added after 5.0 (in CS6), kept for the arrangement.
- **[PCC]**: PhotoCraft Classic's own.
- **[later]**: the menu item exists but is disabled in v1 (shows grayed).

Shortcuts are CS6 where CS6 and PS5 differ; the PS5 shortcut is given in parentheses. Items
grayed by state (no document, Background layer, …) follow PS5 rules.

```
File
  New...                          Ctrl+N            [5]
  Open...                         Ctrl+O            [5]
  Open As...                      Alt+Ctrl+O        [5]
  Open Recent                  ▸  (1..10, ─, Clear Recent File List)   [CS6] (PS5: MRU list at menu bottom)
  ─────────
  Close                           Ctrl+W            [5]
  Close All                       Alt+Ctrl+W        [CS6] (PS5: Window > Close All, Shift+Ctrl+W)
  Save                            Ctrl+S            [5]
  Save As...                      Shift+Ctrl+S      [5]
  Save a Copy...                  Alt+Ctrl+S        [5] (dropped in CS6; we keep it)
  Revert                          F12               [5] (PS5: no shortcut)
  ─────────
  Place...                                          [5] [later]
  Import                       ▸  TWAIN Acquire..., Select TWAIN Source...   [5] [later]
  Export                       ▸  GIF89a Export...  [5] [later]
  ─────────
  Automate                     ▸  Batch... [later], Fit Image..., Conditional Mode Change... [later]   [5]
  File Info...                    Alt+Shift+Ctrl+I  [5] (PS5: no shortcut)
  ─────────
  Page Setup...                   Shift+Ctrl+P      [5]
  Print...                        Ctrl+P            [5]
  Print One Copy                  Alt+Shift+Ctrl+P  [CS6]
  ─────────
  Exit                            Ctrl+Q            [5]

Edit
  Undo <cmd> / Redo <cmd>         Ctrl+Z            [5]  (toggles, PS5 and CS6 semantics)
  Step Forward                    Shift+Ctrl+Z      [5 via History palette, CS6 menu]
  Step Backward                   Alt+Ctrl+Z        [5 via History palette, CS6 menu]
  ─────────
  Fade <cmd>...                   Shift+Ctrl+F      [5] (PS5: Filter > Fade)
  ─────────
  Cut                             Ctrl+X            [5]
  Copy                            Ctrl+C            [5]
  Copy Merged                     Shift+Ctrl+C      [5]
  Paste                           Ctrl+V            [5]
  Paste Special                ▸  Paste in Place  Shift+Ctrl+V [CS6]
                                  Paste Into      Alt+Shift+Ctrl+V [5] (PS5: Edit > Paste Into, Shift+Ctrl+V)
  Clear                           Delete            [5]
  ─────────
  Fill...                         Shift+F5          [5]
  Stroke...                                         [5]
  ─────────
  Free Transform                  Ctrl+T            [5] (PS5: Layer > Free Transform)
  Transform                    ▸  Again  Shift+Ctrl+T; ─; Scale; Rotate; Skew; Distort; Perspective;
                                  ─; Numeric... (no shortcut)            [5; Numeric was dropped in CS6]
                                  ─; Rotate 180°; Rotate 90° CW; Rotate 90° CCW; ─; Flip Horizontal; Flip Vertical  [5]
  ─────────
  Define Brush...                                   [5 via Brushes palette menu] (CS6: Define Brush Preset)
  Define Pattern...                                 [5]
  ─────────
  Purge                        ▸  Undo, Clipboard, Histories, Pattern, ─, All   [5] (All: CS6)
  ─────────
  Keyboard Shortcuts...           Alt+Shift+Ctrl+K  [CS6] [later]
  Preferences                  ▸  General...  Ctrl+K                         [5] (PS5 location: File > Preferences)
                                  ─
                                  Saving Files...
                                  Display & Cursors...
                                  Transparency & Gamut...
                                  Units & Rulers...
                                  Guides & Grid...
                                  Scratch Disks & Updates...        [PCC rename of "Plug-Ins & Scratch Disks"]
                                  Memory & Image Cache...

Image
  Mode                         ▸  Bitmap..., Grayscale, Indexed Color..., ●RGB Color, ─, ✓8 Bits/Channel, ─, Color Table...
                                  (Duotone, CMYK Color, Lab Color, Multichannel, 16 Bits: omitted; see design-decisions)   [5]
  Adjustments                  ▸  (PS5 name "Adjust"; CS6 order, PS5 commands only)
                                  Brightness/Contrast...                  [5]
                                  Levels...            Ctrl+L             [5]
                                  Curves...            Ctrl+M             [5]
                                  ─
                                  Hue/Saturation...    Ctrl+U             [5]
                                  Color Balance...     Ctrl+B             [5]
                                  Channel Mixer...                        [5]
                                  ─
                                  Invert               Ctrl+I             [5]
                                  Posterize...                            [5]
                                  Threshold...                            [5]
                                  Selective Color...                      [5] [later]
                                  ─
                                  Variations...                           [5]
                                  ─
                                  Desaturate           Shift+Ctrl+U       [5]
                                  Replace Color...                        [5]
                                  Equalize                                [5]
  ─────────
  Auto Levels                     Shift+Ctrl+L      [5] (PS5: Adjust > Auto Levels; CS6 name: Auto Tone)
  Auto Contrast                   Alt+Shift+Ctrl+L  [CS6]
  ─────────
  Image Size...                   Alt+Ctrl+I        [5] (PS5: no shortcut)
  Canvas Size...                  Alt+Ctrl+C        [5] (PS5: no shortcut)
  Image Rotation               ▸  180°, 90° CW, 90° CCW, Arbitrary..., ─, Flip Canvas Horizontal, Flip Canvas Vertical
                                                    [5] (PS5 name: Rotate Canvas)
  Crop                                              [5]
  Trim...                                           [CS6]
  Reveal All                                        [CS6]
  ─────────
  Duplicate...                                      [5]
  Apply Image...                                    [5]
  Calculations...                                   [5] [later]
  ─────────
  Histogram...                                      [5] (CS6: Histogram panel)

Layer
  New                          ▸  Layer...                 Shift+Ctrl+N   [5]
                                  Background from Layer / Layer from Background...  [5] (PS5: Layer > New > Background / double-click)
                                  Layer via Copy           Ctrl+J         [5]
                                  Layer via Cut            Shift+Ctrl+J   [5]
  Duplicate Layer...                                [5]
  Delete Layer                                      [5]
  ─────────
  Layer Properties...                               [CS6 name] (name + color tag; PS5: part of Layer Options...)
  Layer Style                  ▸  Blending Options...      [5 content of "Layer Options..."]
                                  ─
                                  Drop Shadow...                          [5 "Effects"]
                                  Inner Shadow...                         [5]
                                  Outer Glow...                           [5]
                                  Inner Glow...                           [5]
                                  Bevel and Emboss...                     [5]
                                  ─
                                  Copy Layer Style / Paste Layer Style / Clear Layer Style   [5: Copy/Paste/Clear Effects]
                                  Global Light...                         [5: Global Angle...]
                                  Create Layer                            [5]
                                  Hide All Effects                        [5]
  ─────────
  New Adjustment Layer         ▸  Levels..., Curves..., Color Balance..., Brightness/Contrast..., Hue/Saturation...,
                                  Channel Mixer..., Invert, Threshold..., Posterize...   [5]
  Layer Content Options...                          [5: Adjustment Options...]
  ─────────
  Layer Mask                   ▸  Reveal All, Hide All, Reveal Selection, Hide Selection, ─, Delete, Apply, ─, Disable/Enable
                                                    [5: Add Layer Mask ▸ / Remove Layer Mask ▸ / Disable Layer Mask]
  ─────────
  Create Clipping Mask            Alt+Ctrl+G        [5: Group with Previous, Ctrl+G]
  Release Clipping Mask           Alt+Ctrl+G        [5: Ungroup, Shift+Ctrl+G]
  ─────────
  Arrange                      ▸  Bring to Front Shift+Ctrl+]; Bring Forward Ctrl+]; Send Backward Ctrl+[; Send to Back Shift+Ctrl+[   [5]
  Align Linked                 ▸  Top Edges, Vertical Centers, Bottom Edges, ─, Left Edges, Horizontal Centers, Right Edges   [5]
  Distribute Linked            ▸  (same six)                                     [5]
  ─────────
  Merge Down                      Ctrl+E            [5] (becomes "Merge Linked" when the active layer is linked, as PS5)
  Merge Visible                   Shift+Ctrl+E      [5]
  Flatten Image                                     [5]
  ─────────
  Matting                      ▸  Defringe..., Remove Black Matte, Remove White Matte   [5]

Type                                                [CS6 menu; PS5 had these under Layer > Type]
  Edit Text...                    (double-click type layer thumbnail)   [PCC] (opens the PS5-style Type Tool dialog)
  Anti-Alias                   ▸  ●None, Crisp, Strong, Smooth          [5 (type dialog)]
  Orientation                  ▸  ●Horizontal, Vertical                 [5: Layer > Type > Horizontal/Vertical]
  ─────────
  Rasterize Type Layer                              [5: Layer > Type > Render Layer]
  Update All Text Layers                            [CS6]

Select
  All                             Ctrl+A            [5]
  Deselect                        Ctrl+D            [5]
  Reselect                        Shift+Ctrl+D      [5]
  Inverse                         Shift+Ctrl+I      [5]
  ─────────
  Color Range...                                    [5]
  ─────────
  Modify                       ▸  Border..., Smooth..., Expand..., Contract..., Feather...  Shift+F6   [5] (PS5: Feather at Select top level, Alt+Ctrl+D)
  Grow                                              [5]
  Similar                                           [5]
  ─────────
  Transform Selection                               [5]
  ─────────
  Edit in Quick Mask Mode         Q                 [CS6 menu item; Quick Mask is 5]
  ─────────
  Load Selection...                                 [5]
  Save Selection...                                 [5]

Filter
  Last Filter (<name>)            Ctrl+F            [5]
  ─────────
  Artistic, Brush Strokes, Sketch, Texture          [5 Gallery Effects] [later] (whole submenus, disabled)
  Blur                         ▸  Blur, Blur More, Gaussian Blur..., Motion Blur..., Radial Blur... [later], Smart Blur... [later]   [5]
  Distort                      ▸  Pinch..., Polar Coordinates..., Ripple..., Shear... [later], Spherize..., Twirl..., Wave... [later], ZigZag... [later], Displace... [later]   [5]
  Noise                        ▸  Add Noise..., Despeckle, Dust & Scratches..., Median...   [5]
  Pixelate                     ▸  Color Halftone... [later], Crystallize... [later], Facet [later], Fragment [later], Mezzotint... [later], Mosaic..., Pointillize... [later]   [5]
  Render                       ▸  Clouds, Difference Clouds, Lens Flare... [later], Lighting Effects... [later], Texture Fill... [later]   [5] (3D Transform omitted)
  Sharpen                      ▸  Sharpen, Sharpen Edges, Sharpen More, Unsharp Mask...   [5]
  Stylize                      ▸  Diffuse..., Emboss..., Extrude... [later], Find Edges, Glowing Edges [later], Solarize, Tiles..., Trace Contour..., Wind...   [5]
  Video                        ▸  De-Interlace..., NTSC Colors   [5] [later]
  Other                        ▸  Custom..., High Pass..., Maximum..., Minimum..., Offset...   [5]

View
  Zoom In                         Ctrl++            [5]
  Zoom Out                        Ctrl+-            [5]
  Fit on Screen                   Ctrl+0            [5]
  Actual Pixels                   Ctrl+1            [5] (PS5: Alt+Ctrl+0)
  Print Size                                        [5]
  ─────────
  Screen Mode                  ▸  ●Standard Screen Mode, Full Screen Mode With Menu Bar, Full Screen Mode   [CS6 menu; modes are 5]
  ─────────
  ✓Extras                         Ctrl+H            [CS6] (PS5: Hide Edges, Ctrl+H)
  Show                         ▸  ✓Selection Edges, ✓Target Path, Grid  Ctrl+', ✓Guides  Ctrl+;   [5]
  ─────────
  ✓Rulers                         Ctrl+R            [5]
  ─────────
  ✓Snap                           Shift+Ctrl+;      [5: Snap To Guides / Snap To Grid]
  Snap To                      ▸  ✓Guides, Grid, ✓Document Bounds   [CS6 grouping]
  ─────────
  Lock Guides                     Alt+Ctrl+;        [5]
  Clear Guides                                      [5]
  New Guide...                                      [CS6]

Window
  Arrange                      ▸  Tile All Vertically, Tile All Horizontally, ─, Consolidate All to Tabs, ─,
                                  Cascade, Tile, Float in Window, Float All in Windows, ─,
                                  Match Zoom, Match Location, Match All, ─,
                                  New Window for <document>           [CS6] (PS5: Window > Cascade/Tile/Arrange Icons; View > New View)
  Workspace                    ▸  ●Essentials (Default), Classic 5.0 [PCC], <saved workspaces>, ─,
                                  Reset <current>, New Workspace..., Delete Workspace...   [CS6]
  ─────────
  Actions                         Alt+F9            [5] (PS5: F9)
  Brushes                         F5                [5]
  Channels                                          [5]
  Color                           F6                [5]
  History                                           [5]
  Info                            F8                [5]
  Layers                          F7                [5]
  Navigator                                         [5]
  ✓Options                                          [5] (shows/hides the Options bar)
  Paths                                             [5]
  Swatches                                          [5]
  ✓Tools                                            [5]
  ─────────
  Status Bar                                        [5] (application status bar, off by default; §7.4)
  ─────────
  ✓1 Shoes.psd
   2 Untitled-1                                     [5]

Help
  PhotoCraft Classic Help...      F1                [5 equivalent]
  Keyboard Shortcuts                                [PCC]
  ─────────
  Check for Updates...                              [PCC] (runs PCUpdate.exe)
  System Info...                                    [CS6]
  ─────────
  About PhotoCraft Classic...                       [5 equivalent]
```

**Notes on the tree.**

- The *Layer Style* effect items have no menu shortcuts. PS5 used `Ctrl+1…5` only *inside* the
  Effects dialog, to switch pages.
- *Create Clipping Mask* and *Release Clipping Mask* are one toggling command on `Alt+Ctrl+G`, as
  in CS6. The menu shows whichever applies.
- **`Ctrl+G` is deliberately left unassigned.** CS6 uses it for Group Layers (layer groups are
  PS6+ and are not in v1). PS5 used it for Group with Previous.

**Context menus** (right-click, as in PS5 p.32):

- Canvas with a selection tool: Deselect, Select Inverse, Feather…, Save Selection…, Make Work
  Path, Layer via Copy, Layer via Cut, Free Transform, Fill…, Stroke…, Last Filter, Fade…
- Canvas with the Move tool: a list of the layers under the pointer (PS5 p.260).
- Layers palette row: Layer Properties…, Blending Options…, Duplicate Layer…, Delete Layer.
- Document tab: Close, Close All, Move to New Window (= Float in Window), New Window for <doc>,
  Consolidate All to Here.

---

## 5. Tools panel

### 5.1 Order, groups, shortcuts

There are four groups separated by etched lines. The order is CS6. The contents are the PS5 tool
list plus CS6's Path Selection and Rectangle/Ellipse. In a column, a flyout lists the group's
tools as `[icon] Name ......... Key`, and the current tool has a ■ marker (CS6 flyout look, drawn
as a Win95 popup menu with 22 px items).

| # | Button (default tool first) | Flyout members | Key | Shift+key cycles | Origin |
|---|---|---|---|---|---|
| 1 | Move | — | V | — | PS5 V |
| 2 | Rectangular Marquee | Elliptical Marquee, Single Row Marquee, Single Column Marquee | M | Rect ↔ Ellipse only (as PS5/CS6) | PS5 M (PS5 kept Crop in this flyout) |
| 3 | Lasso | Polygonal Lasso, Magnetic Lasso | L | all three | PS5 L |
| 4 | Magic Wand | — | W | — | PS5 W (CS6 adds Quick Selection: omitted) |
| 5 | Crop | — | C | — | PS5 C (CS6 adds Slice tools: omitted) |
| 6 | Eyedropper | Color Sampler, Measure | I | all three | PS5: Eyedropper/Color Sampler I, Measure U. CS6: Ruler in the I group. |
| — | *separator* | | | | |
| 7 | Paintbrush | Airbrush, Pencil | B | all three | PS5: Paintbrush B, Airbrush J, Pencil N. CS6: Brush/Pencil B. Airbrush is a separate tool here (PS5). |
| 8 | Clone Stamp | Pattern Stamp | S | both | PS5 "Rubber Stamp"/Pattern Stamp, S |
| 9 | History Brush | — | Y | — | PS5 Y (CS6 Art History omitted) |
| 10 | Eraser | — (modes in the Options bar: Paintbrush, Airbrush, Pencil, Block) | E | (Shift+E cycles the eraser *mode*, as PS5 p.199) | PS5 E (CS6 Background/Magic Eraser omitted) |
| 11 | Gradient | Paint Bucket | G | both | CS6 grouping. PS5: five gradient tools (G) and Paint Bucket (K). The gradient type is chosen in the Options bar. |
| 12 | Blur | Sharpen, Smudge | R | all three | PS5 R (CS6 has no key; R is Rotate View there, omitted) |
| 13 | Dodge | Burn, Sponge | O | all three | PS5 O |
| — | *separator* | | | | |
| 14 | Pen | Freeform Pen, Magnetic Pen, Add Anchor Point (+), Delete Anchor Point (−), Convert Point | P | Pen ↔ Freeform ↔ Magnetic | PS5 P with + and − keys |
| 15 | Horizontal Type | Vertical Type, Horizontal Type Mask, Vertical Type Mask | T | all four | PS5 T |
| 16 | Path Selection | Direct Selection | A | both | PS5: Direct Selection A. Path Selection is PS6/CS6. |
| 17 | Rectangle | Ellipse, Line | U | all three | CS6 U. PS5: Line in the Pencil group (N). Rectangle/Ellipse are PS6+ (pixel-fill mode only in v1). |
| — | *separator* | | | | |
| 18 | Hand | — | H | — | PS5 H |
| 19 | Zoom | — | Z | — | PS5 Z |
| — | *separator* | | | | |
| 20 | Foreground/Background chips | Default colors icon (top-left), Swap icon (top-right) | D, X | — | PS5 |
| 21 | Quick Mask toggle | (Standard / Quick Mask in two-column mode, as in PS5: two buttons) | Q | — | PS5 |
| 22 | Screen Mode button with flyout | Standard, Full Screen With Menu Bar, Full Screen | F (Shift+F reverses) | — | CS6 (PS5: three buttons) |

**Keyboard behaviour**

- *Use Shift Key for Tool Switch* is ON by default (a CS6 preference). With it OFF, pressing the
  key repeatedly cycles the group.
- Alt-click on a tool button cycles its group (PS5).
- While typing in a field, tool keys are not intercepted.
- **Space** gives a temporary Hand. **Ctrl+Space** gives Zoom In and **Alt+Space** gives Zoom
  Out (PS5 p.35). **Ctrl** gives a temporary Move. **Alt** with painting tools gives a temporary
  Eyedropper (PS5).
- Double-clicking a tool button shows the Options bar if hidden. In PS5 this opened the Options
  palette (PS5 p.30). Double-clicking Hand = Fit on Screen; double-clicking Zoom = Actual Pixels
  (PS5 p.35).

### 5.2 Geometry

- **Header strip**: 11 px. Etched double-line gripper (Office 97 style) on the left. A 9×7 «/»
  chevron button toggles one column ↔ two columns.
  - Floating: the PS5 12 px navy caption, with no title text. PS5's toolbox caption is empty;
    measured y=47…58.
- **Cells**: 25×21 px with shared 1 px black grid lines (§2.4). Each tool group sits in a sunken
  frame (Sh top/left, Hi bottom/right). Groups are 4 px apart: 1 px etched line plus padding.
- **Glyphs**: 16×16, centred in the 23×19 interior, 16-color. A group with members shows a
  **3×3 black triangle** in the bottom-right interior corner (measured in PS5).
- **Flyout**: press and hold for 300 ms, or right-click, or click the triangle. The flyout is a
  Win95 popup next to the button.
- **Color chips**: a 46 px tall block.
  - Foreground 18×18 at (5,7) and background 18×18 at (13,15), each with a 1 px black frame and a
    1 px white inner frame.
  - Default icon 10×10 at the top-left; swap ↰ 10×10 at the top-right.
  - In one column this is 25 px wide (chips 14×14). In two columns it is 50 px wide (chips 22×22,
    PS5 proportions).
- **Quick Mask**: one column, one 25×21 toggle. Two columns: two buttons, as in PS5 (measured
  row y≈366…390).
- **Screen mode**: 25×21 with a flyout triangle.
- **Height** in one column: 11 + 6 + (6+7+4+2) × 21 + 3 × 5 + 46 + 21 + 21 + paddings ≈ **527 px**.
  - When the workspace is shorter than that, the panel uses two columns automatically (CS6
    remembers the user's choice; we honour an explicit choice and scroll by 21 px steps with
    ▲/▼ strips if it still does not fit).
- **Two-column order**: the one-column sequence wraps row by row within each group:
  - (Move, Marquee), (Lasso, Wand), (Crop, Eyedropper)
  - (Brush, Stamp), (History, Eraser), (Gradient, Blur), (Dodge, —)
  - (Pen, Type), (PathSel, Shape)
  - (Hand, Zoom)
  - That is 10 rows, ≈ 310 px.

---

## 6. Panels (palettes)

### 6.1 Panel chrome (PS5 internals, measured)

- **Floating palette frame**: 3 px (Light, Hi, Face on top/left; Sh, Dk on bottom/right). Total
  PS5 palette width **212 px**.
- **Floating mini caption**: **12 px** solid navy (no gradient, no title text).
  - Two **10×9** caption buttons at the right: minimize/maximize (glyph ▬) and close (×), with a
    1 px gap.
  - Drag anywhere on the caption to move.
- **Tab row**: 14 px tabs on a face-colored strip (16 px including the 2 px top margin). A 1 px
  black baseline runs along the bottom, except under the active tab.
  - **Tab shape**: trapezoid. Vertical left edge, flat top, **diagonal right edge at 45°** running
    down-right over the full tab height (about 12 px). Tabs overlap: each tab's left edge starts
    under the previous tab's slanted edge.
  - **Active tab**: `COLOR_WINDOW` (white) fill, **bold** label, merged with the body (no
    baseline under it), drawn last.
  - **Inactive tabs**: face fill, regular label, black outline.
  - Label padding: 6 px left, 4 px before the slant.
- **Palette menu button**: at the right end of the tab row, a 13×14 raised button with a black ▸
  (5×9). It opens the palette menu (PS5 p.29). It also works while the group is collapsed.
- **Body**: face color with 4 px padding.
- **Size grip**: 12×12 diagonal-lines grip at the bottom-right (measured on Navigator). Only
  panels that can be resized show it (not Color or Info).
- **Docked groups** have no caption (CS6). The dock header strip carries the chevron.

**Pop-up slider control** (PS5 p.30).

- It is a numeric field plus an **11×16** button with a ▸ glyph immediately to its right.
- Pressing the button opens a borderless popup directly under the field: a 1 px black frame and
  raised bevel, about **150×24 px**, holding a horizontal track with one triangle thumb.
- **Press-drag-release** sets the value and closes the popup. **Click** opens it and leaves it
  open.
- Up/Down change the value by ±1, Shift+Up/Down by ±10. Enter or a click outside commits. Esc
  restores the old value.
- The angle variant (gradient angle, Global Angle) is a 40×40 circle with a radius line instead
  of the track.

**PS5 dialog slider** (Levels, Hue/Sat, filters).

- The track is a 1 px black line with a 1 px white line under it.
- Thumbs are triangles 9 px wide and 6 px tall, pointing up, below the track. Outline black.
  - Fill face normally, black when dragging.
  - Levels has black, gray and white thumbs.
- The thumb snaps to integer values. Keyboard focus is shown with a dotted focus rectangle around
  the slider.

### 6.2 Panel contents (PS5 layout, CS6 placement)

**Layers.** PS5 pp.259–292; measured in the PS5 screenshot.

- **Top row**: blend Mode drop-down (124×20) and `Opacity: [100]%▸` (a 34×20 field plus the pop-up
  slider button). Both are disabled when Background is active.
- **Second row**: `☐ Preserve Transparency`. This is the PS5 control; the CS6 Lock buttons are
  omitted.
- **Rows**, top layer first:
  - **eye column**: a 20×26 raised cell with an eye glyph. Click toggles; drag across toggles many;
    Alt-click shows only this layer.
  - **brush/link column**: 20×26. It shows a ✎ brush on the active layer, a chain on linked
    layers, and a mask glyph when the mask is the target.
  - **thumbnail**: 1 px black frame.
  - **name**: MS Sans Serif 8. *Background* is shown in italics.
  - The active row is filled navy with white text.
- **Row height**: 28 px at the "small" thumbnail size (measured: a 31×23 thumbnail for a 4:3 image
  and a 27 px eye cell).
- **Palette Options** thumbnail boxes, measured with frames: **None, 32, 56, 80 px** (33/58/83
  including the frame). Row heights are then 36 / 60 / 84.
- **Layer mask thumbnail**: right of the layer thumbnail. A chain glyph between them toggles the
  link.
- **Effects**: an ƒ glyph at the right end of the row.
- **Adjustment layers**: a half-black circle icon instead of a thumbnail.
- **Bottom bar**: 22 px with buttons 20×18: **Add Layer Mask** (◘), **New Layer** (❏; Alt-click
  opens the dialog), **Trash** (🗑). CS6 adds Link/Style/Adjustment/Group buttons; omitted in v1.
- **Drag a row** to reorder: an inverted 2 px insertion line follows the pointer. Background stays
  at the bottom.
- **Double-click**: on the name, rename in place (CS6); on the thumbnail, Layer Style >
  Blending Options; on a type layer's thumbnail, Edit Text.

**Channels.**

- Rows: eye cell, a 32 px thumbnail, the name, and the shortcut right-aligned.
- **Channel shortcuts (decision).** PS5 used `Ctrl+~` (composite) and `Ctrl+1…3` (R, G, B), with
  alpha channels from `Ctrl+4`. CS6 uses `Ctrl+2` (composite) and `Ctrl+3…5`, because it gives
  `Ctrl+1` to Actual Pixels.
  - We follow the CS6 View menu, so `Ctrl+1` is Actual Pixels.
  - Channels are therefore on **`Ctrl+2` = RGB, `Ctrl+3/4/5` = Red/Green/Blue, `Ctrl+6…` = alpha
    channels** (CS6 keys).
  - Inside the Levels, Curves and Hue/Saturation dialogs, the channel menu keys are
    **`Alt+2…`** (CS6). PS5 used `Ctrl+~/1/2/3` there.
- Bottom: Load channel as selection, Save selection as channel, New channel, Trash.

**Paths.**

- Rows: a 32 px thumbnail and the name. *Work Path* is shown in italics.
- Bottom (PS5): Fill path, Stroke path, Load path as selection, Make work path from selection, New
  path, Trash.

**History** (PS5 pp.168–172).

- **Snapshot area** at the top, separated by an etched line: snapshot rows with a 32 px
  thumbnail. The first snapshot is created automatically (*Automatically Create First Snapshot*).
- **State list**: one row per state (18 px: a 16×16 tool or command icon plus the name). Oldest
  at the top.
  - States after the selected one are drawn in GRAYTEXT.
  - **History-brush source column**: a 16 px column at the left. A click sets the source and shows
    the History Brush glyph.
  - PS5 also had a draggable **state slider** (a ▶ marker in the left gutter). We support dragging
    the selection, which is equivalent.
- **Bottom buttons**: *New document from current state*, *New snapshot*, *Trash*.
- **Menu**: Step Forward, Step Backward, New Snapshot, Delete, Clear History, New Document,
  History Options… The options are Maximum History Items (default **20**, PS5), Automatically
  Create First Snapshot, and Allow Non-Linear History.

**Actions** (PS5 pp.347–358; recording is [later] in v1).

- **List mode** has toggle columns:
  - A ✓ column: include the command (red ✓ when partly disabled).
  - A dialog column: a ▭ dialog glyph means stop at the dialog.
  - Then a disclosure triangle and the set/action/command tree.
- **Button mode**: a grid of 2-column raised buttons labelled with the action name and an F-key,
  in the color chosen for the action.
- Bottom: Stop ■, Record ●, Play ▶, New set, New action, Trash.

**Navigator** (PS5 p.34; measured).

- A thumbnail centred in a sunken box.
- **The view box** is a 2 px rectangle in light red `#FF4A4A` (measured). Palette Options can
  change it.
- **Bottom strip**, 22 px:
  - zoom field (52×18, `66.67%`, two decimals);
  - **small mountains** button (zoom out, 22×16);
  - slider with a triangle thumb (logarithmic, 0.2%–1600%);
  - **large mountains** button (zoom in, 22×16);
  - size grip.
- Dragging the view box pans. A click centres the view. Ctrl-drag zooms to the dragged area.
  Shift+Enter in the field applies and keeps the field focused.

**Info** (PS5 p.31).

- Four quadrants:
  - top-left: first color readout (eyedropper icon; click for the mode menu: Actual Color, RGB,
    HSB, Grayscale, Opacity);
  - top-right: second color readout;
  - bottom-left: **position** X/Y (cross-hair icon; click for the units menu);
  - bottom-right: **size** W/H.
- Tool-dependent extra lines: A (angle), D (distance), ΔX/ΔY for Move/Line/Measure; W/H/A/H/V
  during Transform.
- In adjustment dialogs: before/after values shown as `128/140`.
- Fixed height 96 px in the body; it cannot be resized.

**Color** (PS5).

- Foreground/background chip pair at the left (21×21 overlapping). The active chip has a double
  frame.
- Three slider rows: label `R`, a track that is a **color ramp** 128×6 px with dynamic colors
  (*Dynamic Color Sliders* preference) and a triangle thumb, and a 30×18 field.
- Below them, a full-width spectrum ramp (16 px). A click picks a color.
- Palette menu: Grayscale Slider, RGB Sliders, HSB Sliders, ─, Color Bar…
- Fixed body height 86 px.

**Swatches.**

- A grid of **11×11** swatches (10×10 color plus a 1 px gap), starting at 4 px padding. The
  default set is a 122-color set: our own list, not Adobe's file.
- Click sets the foreground; Ctrl-click sets the background; Shift-click replaces a swatch;
  clicking empty space adds a swatch; Alt-click deletes (PS5).
- Menu: Reset Swatches, Load Swatches… (.ACO), Replace Swatches…, Save Swatches…

**Brushes** (PS5 pp.200–203).

- A grid of brush tips in cells of **30×30** (PS5 style).
  - Small tips are drawn as solid discs.
  - Tips larger than the cell are drawn scaled, with the diameter below in Small Fonts (e.g. `65`).
  - The selected cell has a 1 px black frame on white.
- Default set: hard 1, 3, 5, 9, 13, 19; soft 5, 9, 13, 17, 21, 27; soft 35, 45, 65, 100, 200, 300.
- Double-click opens **Brush Options**: Diameter 1–999 px, Hardness 0–100%, Spacing 1–999% (with
  a ☐ to disable it), Angle −180…180°, Roundness 0–100%. A preview box shows angle and roundness;
  a second box shows the tip.
- Menu: Reset Brushes, Load Brushes… (PS5 .ABR v1), Replace, Save, New Brush…, Delete Brush,
  Brush Options…, Define Brush.
- In CS6 placement the Brushes panel lives in the icon column. The Options bar's brush picker
  shows the same grid as a drop-down popup (CS6 brush preset picker).

### 6.3 Default arrangement

**CS6 Essentials** (reference, verified via Photoshop Essentials):

- Icon column: *History*, *Properties*.
- Expanded column: *[Color | Swatches]*, *[Adjustments | Styles]*, *[Layers | Channels | Paths]*.

**PS5 default** (measured screenshot): floating groups.

- `[Navigator | Info | Options]` at the top right.
- `[Color | Swatches | Brushes]` below it.
- `[Layers | Channels | Paths | History | Actions]` at the bottom right.
- The toolbox floats at the top left.

**PhotoCraft Classic Essentials (decision)**:

| Column | Group (top to bottom) | Default height at 1024×768 | Why |
|---|---|---|---|
| Icon column (32 px) | **[History \| Actions]** (two icons, one group) | flyout 214×260 | History is CS6's top icon. Actions was grouped with History and Layers in PS5. Both are used occasionally. |
| | **[Brushes]** | flyout 214×200 | The Options bar has its own brush picker, so the full panel is needed only to edit tips. CS6 puts brush panels in icons too. |
| Expanded column (214 px) | **[Color \| Swatches]** | 110 (fixed) | Exactly CS6's top group, and a PS5 pair |
| | **[Navigator \| Info]** | 142 (collapsed to its 20 px tab row when the workspace is under 600 px tall) | Takes the slot of CS6's [Adjustments \| Styles], which has no PS5 equivalent. Navigator and Info were PS5's top group. |
| | **[Layers \| Channels \| Paths]** | fills the rest (403 px at 1024×768) | Exactly CS6's bottom group and PS5's |

- There is no Character/Paragraph panel in v1. Type is set in the PS5 Type Tool dialog plus the
  Options bar. A [Character | Paragraph] icon group is reserved for later.
- The Options panel of PS5 becomes the Options bar (§7).
- **Classic 5.0 workspace** [PCC]: the PS5 arrangement above, with floating palettes. The tools
  are floating in two columns at (6,40) relative to the frame client. The palettes are 212 px
  wide at the right edge: y=0 Navigator group (130 tall), y=134 Color group (125), y=263 Layers
  group (to the bottom). The Options bar stays visible, because we have no Options palette.

### 6.4 Icon column details

- Header: 11 px with «.
- Icon buttons: 26×24, flat. On hover they show a 1 px raised bevel (Hi/Sh). The button whose
  panel is open is drawn sunken with a dithered face.
- Groups are separated by a 2 px etched line with 3 px padding.
- Panel icons are 16×16 in 16 colors (§8).
- **Flyout**:
  - It is a palette (tab row and body) 214 px wide, opened to the **left** of the icon column and
    top-aligned with the clicked icon.
  - It is drawn over the document area, with a 1 px Dk outer line and the raised frame.
  - It has an 11 px header with ▸▸ (collapse).
  - It stays open until it is collapsed (by default; Auto-Collapse is a preference).

---

## 7. Options bar

### 7.1 Layout grammar

`[gripper 6] [tool icon button 28×22 ▾] │ [tool-specific controls …] │ … [right: workspace switcher 100×21]`

- The **tool icon button** shows the current tool's 16×16 glyph and a ▾. Its menu holds Reset Tool
  and Reset All Tools (PS5 Options palette menu, p.30). CS6's tool presets are omitted in v1.
- **Labels** end with ":" (`Feather:`, `Opacity:`). Units follow the field (`px`, `%`). The
  spacing is label, 3 px, field, 2 px, unit, 10 px to the next control.
- **Groups** are separated by an etched vertical 2 px line with 6 px on each side.
- **Mode buttons** (selection combine modes, gradient type) are 23×22 latching toolbar buttons
  with the Win95 dither when latched.
- **Commit/cancel** buttons (✓ 23×22 and ⊘ 23×22) appear at the right while a transform, crop or
  type edit is pending (CS6).
- If the bar is too narrow, the overflowing controls are hidden behind a » button that drops down
  a popup containing them (period-neutral).

### 7.2 Contents per tool

PS5 option names are kept. Items marked [CS6] came after 5.0.

| Tool | Controls |
|---|---|
| Move | `☐ Auto Select Layer` [5] · `☐ Pixel Doubling` [5] · `☐ Show Bounding Box` [CS6] · align buttons ×6 (enabled when layers are linked) [CS6 placement of PS5's Align Linked] |
| Marquee (rect/ellipse) | selection mode ×4 (New, Add, Subtract, Intersect) [CS6] · `Feather: [0] px` [5] · `☐ Anti-aliased` (ellipse only) [5] · `Style: [Normal ▾]` (Normal, Constrained Aspect Ratio, Fixed Size) [5] · `Width: [64]` `Height: [64]` (enabled by the style) [5] |
| Single row/column | mode ×4 · `Feather:` |
| Lasso, Polygonal Lasso | mode ×4 · `Feather: [0] px` · `☐ Anti-aliased` [5] |
| Magnetic Lasso | as Lasso + `Lasso Width: [10] px` (1–40) · `Frequency: [57]` (0–100) · `Edge Contrast: [10]%` (1–100) · `☐ Stylus Pressure` [5] |
| Magic Wand | mode ×4 · `Tolerance: [32]` (0–255) [5] · `☐ Anti-aliased` [5] · `☐ Contiguous` [CS6] · `☐ Use All Layers` [5] (CS6 name: Sample All Layers) |
| Crop | `☐ Fixed Target Size` · `Width: [ ]` `Height: [ ]` `Resolution: [ ] [pixels/inch ▾]` · `[Front Image]` [5]. While cropping: ✓ ⊘ [CS6]. |
| Eyedropper | `Sample Size: [Point Sample ▾]` (Point Sample, 3 by 3 Average, 5 by 5 Average) [5] |
| Color Sampler | `Sample Size:` · `[Clear]` [CS6] |
| Measure | read-only `X: Y: W: H: A: D1: D2:` (Small Fonts) · `[Clear]` [CS6 placement; PS5 showed these in Info] |
| Paintbrush | brush picker `[● 21 ▾]` [CS6 placement] · `Mode: [Normal ▾]` · `Opacity: [100]%▸` · `Fade: [0] steps to [Transparent ▾]` · `☐ Wet Edges` · `Stylus: ☐Size ☐Color ☐Opacity` [5] |
| Airbrush | brush · `Mode:` · `Pressure: [50]%▸` · `Fade:` · `Stylus: ☐Pressure ☐Color` [5] |
| Pencil | brush · `Mode:` · `Opacity:` · `Fade:` · `☐ Auto Erase` · `Stylus: ☐Size ☐Opacity` [5] |
| Clone Stamp | brush · `Mode:` · `Opacity:` · `☐ Aligned` · `☐ Use All Layers` · Stylus [5] |
| Pattern Stamp | brush · `Mode:` · `Opacity:` · `☐ Aligned` [5] (the pattern comes from Edit > Define Pattern) |
| History Brush | brush · `Mode:` · `Opacity:` · `Fade:` · `☐ Impressionist` · Stylus [5] |
| Eraser | brush · `Mode: [Paintbrush ▾]` (Paintbrush, Airbrush, Pencil, Block) · `Opacity:`/`Pressure:` · `Fade:` · `☐ Wet Edges` · `☐ Erase to History` · Stylus [5] |
| Gradient | gradient picker `[▒▒▒ ▾]` (Foreground to Background, Foreground to Transparent, Transparent to Foreground, Black, White, Spectrum, …) · type ×5 (Linear, Radial, Angle, Reflected, Diamond) [CS6; PS5 had 5 tools] · `Mode:` · `Opacity:` · `☐ Reverse` · `☐ Dither` · `☐ Transparency` · `[Edit…]` (Gradient Editor) [5] |
| Paint Bucket | `Fill: [Foreground ▾]` (Foreground, Pattern) · `Mode:` · `Opacity:` · `Tolerance: [32]` · `☐ Anti-aliased` · `☐ Contiguous` [CS6] · `☐ Use All Layers` [5] |
| Blur, Sharpen | brush · `Mode:` · `Pressure: [50]%▸` · `☐ Use All Layers` · Stylus [5] |
| Smudge | as Blur + `☐ Finger Painting` [5] |
| Dodge, Burn | brush · `Range: [Midtones ▾]` (Shadows, Midtones, Highlights) · `Exposure: [50]%▸` · Stylus [5] |
| Sponge | brush · `Mode: [Desaturate ▾]` (Desaturate, Saturate) · `Pressure:` · Stylus [5] |
| Pen | `☐ Rubber Band` [5] |
| Freeform Pen | `Curve Fit: [2] px` [5] |
| Magnetic Pen | `Curve Fit:` · `Pen Width: [10] px` · `Frequency:` · `Edge Contrast:` · `☐ Stylus Pressure` [5] |
| Add/Delete/Convert, Path/Direct Selection | (empty) |
| Type (4 tools) | `Font: [Arial ▾]` (150) · `[Regular ▾]` (70) · `Size: [12 ▾] pt` · `Anti-alias: [Crisp ▾]` · align L/C/R ×3 · color chip · ✓ ⊘ while editing [CS6 placement; values = PS5 Type dialog defaults] |
| Rectangle, Ellipse | `Mode:` · `Opacity:` · `☐ Anti-aliased` (fill pixels with the foreground color; v1) |
| Line | `Mode:` · `Opacity:` · `Weight: [1] px` · `☐ Anti-aliased` · `Arrowheads: ☐Start ☐End [Shape…]` [5, PS5 Line tool options] |
| Hand | `☐ Scroll All Windows` [CS6] · `[Actual Pixels]` `[Fit on Screen]` `[Print Size]` [5] |
| Zoom | `☐ Resize Windows To Fit` [5, applies to floating windows] · `[Actual Pixels]` `[Fit on Screen]` `[Print Size]` [5] |

---

## 8. Icons and artwork (all original)

- **Palette**: the 16 VGA colors only: `000000 800000 008000 808000 000080 800080 008080 C0C0C0
  808080 FF0000 00FF00 FFFF00 0000FF FF00FF 00FFFF FFFFFF`. Background transparency is a mask or
  a magenta key (`#FF00FF` reserved as a key in the source BMPs).
- **Sizes**:
  - tool glyphs 16×16;
  - panel icons 16×16;
  - small buttons (eye, link, trash, new) 12×12 or 14×14;
  - the application icon at 32×32 and 16×16 in 4 bpp (plus 8 bpp variants in the `.ico`);
  - the document icon at 32×32 and 16×16.
- **Style**:
  - 1 px black outlines and flat fills;
  - one highlight edge in white and one in `#808080`, light from the top-left;
  - no antialiasing;
  - tool silhouettes are *generic objects* (an arrow with a cross, a dashed rectangle, a lasso
    loop, a wand, a brush, a stamp, an eraser, a droplet, a pen nib, a letter T, a hand, a
    magnifier).
  - Do not trace PS5 icons. In particular do not copy the PS5 toolbox "eye" banner image (we show
    no banner; the CS6 header strip replaces it).
- **Cursors**: 32×32 monochrome (AND/XOR).
  - Standard cursors show the tool glyph with a hot spot.
  - **Precise cursors** are a cross-hair with a 1 px gap at the centre.
  - **Brush Size** cursors are an XOR outline circle drawn by us on the canvas, not a cursor
    resource.
  - **Caps Lock** toggles standard → precise, precise → brush size and brush size → precise for
    painting tools (PS5 p.27).
  - Very small brushes show four dots around the brush (PS5 p.27).

---

## 9. Dialogs

Sizes are **outer** sizes including the 3 px frame and the 18 px caption, measured from PS5 on
Win98 unless marked otherwise. Fonts are MS Sans Serif 8. PS5 buttons are 60×20 at the right edge,
11 px from the frame, at a 28 px pitch: OK (default), Cancel, then a gap, then Load…/Save…/Auto or
Prev/Next. Holding **Alt** turns *Cancel* into *Reset*: a PS5 behaviour in every adjustment
dialog. **Preview** is a check box under the buttons. Dialogs remember their position (PS5 *Save
Palette Locations*).

### 9.1 New… (measured 363×332)

```
┌ New ────────────────────────────────────────── [×]┐
│ Name: [Untitled-1_____________________]   [  OK  ]│
│ ┌ Image Size: 532K ────────────────────┐  [Cancel]│
│ │ Width:      [7    ]  [inches     ▾]  │          │
│ │ Height:     [5    ]  [inches     ▾]  │          │
│ │ Resolution: [72   ]  [pixels/inch▾]  │          │
│ │ Mode:       [RGB Color   ▾]          │          │
│ └──────────────────────────────────────┘          │
│ ┌ Contents ────────────────────────────┐          │
│ │ (•) White                            │          │
│ │ ( ) Background Color                 │          │
│ │ ( ) Transparent                      │          │
│ └──────────────────────────────────────┘          │
└───────────────────────────────────────────────────┘
```

- The Name edit is 170×20. Number fields are 50×20. Unit drop-downs are 100×21. Mode is 100×21.
- Units: pixels, inches, cm, points, picas, columns (width); pixels/inch, pixels/cm (resolution).
- Mode: Bitmap, Grayscale, RGB Color (v1; Indexed is created via conversion).
- Defaults come from the clipboard size if an image is on the clipboard (PS5 p.54).
- The "Image Size: 532K" caption updates live.
- Our default is **640×480 pixels, 72 ppi**.

### 9.2 Open…, Save As…, Save a Copy… (our own Win95-style file dialogs)

We draw these ourselves because:

- The Win95 common dialog has no thumbnail preview.
- The look differs between 95, 2000 and XP (Places bar).
- `GetOpenFileName` hooks (`OFN_EXPLORER|OFN_ENABLEHOOK`) are fragile on Win95.

Size 470×350, laid out like the Win95 Explorer-style common dialog *(recollection)*. PS5 additions:
a preview and the file size.

```
┌ Open ─────────────────────────────────────────────────────────── [?][×]┐
│ Look in: [📁 My Documents          ▾]  [⬆][✱📁][≣][☰]                  │
│ ┌─────────────────────────────────────────────┐ ┌ Preview ───────────┐ │
│ │📁 Images     📄 Shoes.psd                    │ │                    │ │
│ │📁 Scans      📄 Logo.bmp                     │ │   (thumbnail       │ │
│ │              📄 Photo.jpg                    │ │    up to 128×128)  │ │
│ │  (list view, small icons; Details view:      │ │                    │ │
│ │   Name | Size | Type | Modified)             │ └────────────────────┘ │
│ └─────────────────────────────────────────────┘  640 × 480, RGB        │
│ File name:     [Shoes.psd___________________]   [  Open  ]   File Size:│
│ Files of type: [All Formats              ▾]     [ Cancel ]   900K      │
└────────────────────────────────────────────────────────────────────────┘
```

- **Buttons** are 75×23 (Win95 metric, because this mimics the system dialog). The toolbar
  buttons are 23×22 flat with hover bevels: Up One Level, Create New Folder, List, Details.
- **Look in** shows the drive and folder tree: Desktop, My Computer, drives, then the path
  segments, with indentation and 16-color drive and folder icons.
- **Files of type**: All Formats; Photoshop (*.PSD); BMP (*.BMP;*.RLE;*.DIB); JPEG
  (*.JPG;*.JPE;*.JPEG); PNG (*.PNG); GIF (*.GIF); TGA (*.TGA); PCX (*.PCX) [later];
  All Files (*.*).
- **Save As** adds:
  - `Save As: [Photoshop (*.PSD) ▾]`;
  - PS5's `☐ Save a Copy`, `☐ Exclude Alpha Channels`, `☐ Exclude Non-Image Data` (disabled where
    not applicable);
  - a warning line "Layers will be flattened" for flat formats (CS6 shows a ⚠ similarly).
  - Overwriting asks "Replace existing <name>?" (Win95 wording).
- **Format option dialogs** (PS5):
  - JPEG Options: Matte, Quality 0–10 with [Low/Medium/High/Maximum ▾] and a slider, Format
    Options (Baseline ("Standard"), Baseline Optimized, Progressive with Scans 3–5).
  - BMP Options: File Format Windows/OS2, Depth 1/4/8/24, ☐ Compress (RLE).
  - GIF: via Indexed Color conversion.

### 9.3 Image Size… (measured 400×312)

```
┌ Image Size ─────────────────────────────────────── [×]┐
│ ┌ Pixel Dimensions: 900K ────────────────┐  [  OK  ]  │
│ │ Width:  [640    ] [pixels      ▾] ┐    │  [Cancel]  │
│ │ Height: [480    ] [pixels      ▾] ┘ 8  │  [Auto...] │
│ └────────────────────────────────────────┘            │
│ ┌ Print Size: ───────────────────────────┐            │
│ │ Width:      [8.889 ] [inches    ▾] ┐   │            │
│ │ Height:     [6.667 ] [inches    ▾] ┘ 8 │            │
│ │ Resolution: [72    ] [pixels/inch▾]    │            │
│ └────────────────────────────────────────┘            │
│ ☑ Constrain Proportions                               │
│ ☑ Resample Image: [Bicubic          ▾]                │
└───────────────────────────────────────────────────────┘
```

- Fields are 70×20 (measured). Unit drop-downs are 100×21.
- The chain bracket glyph (8 = our own link icon, 9×18) appears when proportions are constrained.
- Pixel units: pixels, percent. Print units: percent, inches, cm, points, picas, columns.
- Resample: Nearest Neighbor, Bilinear, Bicubic (PS5's three).
- **Auto…** opens *Auto Resolution*: Screen [133] [lines/inch ▾], Quality (•)Draft ( )Good
  ( )Best.

### 9.4 Canvas Size… (measured 335×294)

- **Current Size: 900K**: Width 8.889 inches, Height 6.667 inches (static text).
- **New Size: 900K**: Width [ ] [units ▾], Height [ ] [units ▾].
- **Anchor**: a 3×3 grid of 22×22 buttons. The selected button shows a dot. CS6 draws arrows
  pointing away; PS5 drew a gray square with outlines. Ours: the selected cell is sunken white
  with a ■.
- `☐ Relative` [CS6].
- `Canvas extension color: [Background ▾]` [CS6]. PS5 always used the background color; we keep
  that default.

### 9.5 Levels… (Ctrl+L; measured 405×280)

```
┌ Levels ──────────────────────────────────────────── [×]┐
│ ┌ Channel: [RGB      ▾] ───────────────────┐ [  OK   ] │
│ │      Input Levels: [0  ] [1.00] [255]    │ [Cancel ] │
│ │ ┌──────────────────────────────────────┐ │ [Load...] │
│ │ │       histogram 256 × 100            │ │ [Save...] │
│ │ │                                      │ │ [ Auto  ] │
│ │ └──────────────────────────────────────┘ │ [⁄][⁄][⁄] │  ← black/gray/white eyedroppers 23×22
│ │ ▲               ▲                     △  │ ☑ Preview │
│ │     Output Levels: [0  ] [255]           │           │
│ │ [■■■■■■■■ gradient bar 256×10 □□□□□□□]   │           │
│ │ ▲                                     △  │           │
│ └──────────────────────────────────────────┘           │
└────────────────────────────────────────────────────────┘
```

- Measured: the histogram frame is 256 wide plus a 1 px black frame (x=27…285 → 259 including
  the frame), about 100 tall.
- Input fields: 30×20, 40×20, 40×20. Gamma range 0.10–9.99.
- Thumbs: black, gray and white triangles (9×6).
- Channel menu: RGB `Alt+2`, Red `Alt+3`, Green `Alt+4`, Blue `Alt+5` (CS6 keys), plus alpha
  channels when present.
- The eyedroppers set the black, gray and white point. Double-clicking an eyedropper opens the
  Color Picker for the target color (PS5 p.118).
- Alt+Cancel → Reset. **Auto** is the same as Image > Auto Levels (clip 0.5% per channel, PS5).

### 9.6 Curves… (Ctrl+M; no PS5 screenshot available; layout from the PS5 manual and *recollection*)

About 420×420. Channel drop-down at the top.

- **Graph**: **256×256** px with a 1 px black frame and a dotted grid every 64 px (quarters).
  Alt-click toggles a 10×10 grid (PS5). The PS5 grid was smaller *(unverified)*; 256 gives an
  exact 1:1 level mapping.
- Under the graph: a horizontal gradient bar (black→white, 256×10). A click on it flips light ↔
  dark: levels ↔ ink percentages.
- Left of the graph: a vertical gradient bar.
- `Input: [ ]` and `Output: [ ]` readouts update under the pointer.
- **Mode buttons**: curve (points) and pencil (*Arbitrary Map*, PS5 p.120), each 23×22.
  [Smooth] is enabled in pencil mode.
- Buttons: OK, Cancel, Load…, Save…, Smooth, Auto, eyedroppers ×3, ☑ Preview.
- **Up to 16 points** ("keeping up to 15 other values constant", PS5 p.120). Drag a point off the
  graph to delete it. Ctrl-click in the image adds a point at that pixel's value.
- Evaluation: a natural cubic spline through the points, clamped. See audit: port of
  `compose/adjust/tone.rs curve_lut`.

### 9.7 Hue/Saturation… (Ctrl+U; measured 341×262)

```
┌ Hue/Saturation ─────────────────────────────── [×]┐
│ ┌ Edit: [Master      ▾] ──────────────┐ [  OK   ]  │
│ │ Hue:                       [0    ]   │ [Cancel ]  │
│ │ ──────────────▲─────────────────     │            │
│ │ Saturation:                [0    ]   │ [Load...]  │
│ │ ──────────────▲─────────────────     │ [Save...]  │
│ │ Lightness:                 [0    ]   │            │
│ │ ──────────────▲─────────────────     │ ☐ Colorize │
│ └──────────────────────────────────────┘ ☑ Preview  │
│                          [⁄][⁄+][⁄−]                │
│ [▒▒▒▒▒▒▒▒▒▒▒▒▒ spectrum before 310×8 ▒▒▒▒▒▒▒▒▒▒▒▒▒] │
│ [▒▒▒▒▒▒▒▒▒▒▒▒▒ spectrum after  310×8 ▒▒▒▒▒▒▒▒▒▒▒▒▒] │
└──────────────────────────────────────────────────────┘
```

- Edit menu: Master `Alt+2`, Reds `Alt+3`, Yellows `Alt+4`, Greens `Alt+5`, Cyans `Alt+6`,
  Blues `Alt+7`, Magentas `Alt+8` (CS6 keys; PS5 used `Ctrl+~` and `Ctrl+1…6`).
- Ranges: Hue −180…180, Saturation −100…100, Lightness −100…100.
- With a range other than Master, a **range slider** appears between the two spectrum bars (PS5
  p.124): 4 handles (fall-off ▯, range ▮▮, fall-off ▯) and a centre bar. The default range is 30°
  wide with 30° of fall-off on each side (PS5 p.133).
- Colorize: Hue 0…360, Saturation 0…100 (default 25), Lightness −100…100.

### 9.8 Color Balance… (Ctrl+B; layout from PS5 p.124 and *recollection*)

About 410×235.

```
┌ Color Balance ───────────────────────────────────────── [×]┐
│ ┌ Color Balance ─────────────────────────────────┐ [  OK  ]│
│ │   Color Levels: [0   ] [0   ] [0   ]           │ [Cancel]│
│ │ Cyan    ───────────────▲─────────────── Red    │         │
│ │ Magenta ───────────────▲─────────────── Green  │☑ Preview│
│ │ Yellow  ───────────────▲─────────────── Blue   │         │
│ └────────────────────────────────────────────────┘         │
│ ┌ Tone Balance ──────────────────────────────────┐         │
│ │ ( ) Shadows  (•) Midtones  ( ) Highlights      │         │
│ │ ☑ Preserve Luminosity                          │         │
│ └────────────────────────────────────────────────┘         │
└────────────────────────────────────────────────────────────┘
```

- Values are −100…+100 (PS5 p.124). Each of the three tone ranges keeps its own triple.

### 9.9 Brightness/Contrast… (measured 312×129)

- Two rows: `Brightness: [0 ]` and `Contrast: [0 ]`, each with a slider 200 px wide (−100…+100,
  PS5 p.130).
- OK, Cancel, ☑ Preview.
- Algorithm: **legacy** (linear) as in PS5. CS6 defaults to a non-linear mode with "Use Legacy";
  we implement only legacy. See audit.

### 9.10 Gaussian Blur… (measured 206×211) and the generic filter dialog

```
┌ Gaussian Blur ───── [×]┐
│ ┌────────────┐ [  OK  ]│
│ │ preview    │ [Cancel]│
│ │ 100 × 100  │ ☑Preview│
│ └────────────┘         │
│   [+] 100% [−]         │
│ Radius: [1.0 ] pixels  │
│ ▲───────────────────── │
└────────────────────────┘
```

- **Preview**: a 100×100 sunken box. Drag inside it to pan the preview. A click in the image
  window (the pointer becomes a square) centres the preview there (PS5).
- `[+]` and `[−]` are 11×11 zoom buttons around the zoom text (Small Fonts).
- Radius 0.1–250.0 px. The slider is logarithmic-ish (PS5).
- This dialog is the template for every single-parameter filter (Median, Maximum, Minimum, High
  Pass, Dust & Scratches with two sliders, Add Noise with Amount, Distribution
  (•)Uniform ( )Gaussian and ☐Monochromatic, Motion Blur with an angle dial).

### 9.11 Unsharp Mask… (layout like Gaussian Blur, about 206×260)

- `Amount: [50 ] %` (1–500)
- `Radius: [1.0] pixels` (0.1–250.0)
- `Threshold: [0 ] levels` (0–255)
- Ranges are PS5's. The recommended values in the manual are Amount 150–200 and Radius 1–2
  (p.128).

### 9.12 Color Picker (measured 494×328)

```
┌ Color Picker ──────────────────────────────────────────────────── [×]┐
│ Select foreground color:                                    [  OK  ] │
│ ┌──────────────────────────────┐ ┌─┐  ┌──────┐ ⚠            [Cancel] │
│ │                              │ │ │  │ new  │ ▪                     │
│ │   field 256 × 256            │▸│h│◂ ├──────┤             [Custom] │
│ │   (S × B for the H radio;    │ │u│  │ old  │                      │
│ │    other radios change axes) │ │e│  └──────┘                      │
│ │                              │ │ │  (•)H:[0  ]°   ( )L:[58 ]      │
│ │       ○ ← 11 px circle cursor│ │ │  ( )S:[75 ]%   ( )a:[71 ]      │
│ │                              │ │ │  ( )B:[100]%   ( )b:[47 ]      │
│ │                              │ │ │  ( )R:[255]                    │
│ │                              │ │ │  ( )G:[64 ]    C:[0  ]%        │
│ │                              │ │ │  ( )B:[64 ]    M:[82 ]%        │
│ └──────────────────────────────┘ └─┘                Y:[74 ]%        │
│ ☐ Only Web Colors [CS6]          # [FF4040] [CS6]   K:[0  ]%        │
└──────────────────────────────────────────────────────────────────────┘
```

Measured geometry:

- Field: x=14…269, y=54…309 (256×256), 1 px black frame outside it.
- Hue strip: x≈285…304 (20 wide) by 256, with ▸ ◂ triangle markers outside it.
- New/old swatch: 60×60, split horizontally (top = new).
- Fields: 35×20.

Behaviour:

- The ⚠ out-of-gamut (CMYK) warning and its swatch are omitted in v1, because there is no CMYK.
  The Lab and CMYK fields are **read-only readouts** (computed with fixed sRGB/D65 and naive
  SWOP-like math). We keep them for the PS5 look. Alternatively they can be hidden behind a
  preference; decision: show them, read-only, grayed.
- **Only Web Colors** and the **#hex field** came after PS5 (PS 5.5/6.0); CS6 has both. Include
  them on the bottom row. Web-safe snapping uses the 6 levels 00/33/66/99/CC/FF.
- **Custom…** opens a *Custom Colors* dialog. PS5 offered PANTONE and other books; we cannot ship
  those. Ours: a 64-color "period" set plus the user's saved swatches.
- Preferences > General > Color Picker: [PhotoCraft ▾] or [Windows]. Windows = the
  `ChooseColor` common dialog. PS5 offered this choice too.

### 9.13 Preferences (Ctrl+K; measured 428×339)

The panel is chosen from a drop-down at the top-left (200×21); the body is a group box below it.
On the right are OK, Cancel, then **Prev** and **Next**, all 60×20. PS5 p.37.

| Panel | Controls (PS5 names; [PCC] additions) |
|---|---|
| **General** | Color Picker: [PhotoCraft/Windows ▾] · Interpolation: [Nearest Neighbor (Faster)/Bilinear/Bicubic (Better) ▾] · Options group (two columns of check boxes): ☑ Export Clipboard · ☑ Show Tool Tips · ☐ Beep When Done · ☑ Dynamic Color Sliders · ☑ Save Palette Locations · ☑ Use Shift Key for Tool Switch [CS6] · ☐ Auto-Collapse Icon Panels [CS6] · ☑ Open Documents as Tabs [CS6] · ☑ Enable Floating Document Window Docking [CS6] · button **[Reset Palette Locations to Default]** (180×20). PS5's Anti-alias PostScript and Short PANTONE Names are omitted. |
| **Saving Files** | Image Previews: [Always Save/Never Save/Ask When Saving ▾] with ☑ Icon (PS5 saved a resource icon; we save the PSD thumbnail resource 1036 instead) · Append File Extension: [Always/Never/Ask ▾] ☑ Use Lower Case · File Compatibility: ☑ Include Composited Image With Layered Files · Recent file list contains: [10] files [CS6] |
| **Display & Cursors** | Display: ☐ Color Channels in Color · ☑ Use Diffusion Dither (8-bit displays) · ☐ Use System Palette · Painting Cursors: (•)Standard ( )Precise ( )Brush Size · Other Cursors: (•)Standard ( )Precise |
| **Transparency & Gamut** | Grid Size: [None/Small/Medium/Large ▾] (4/8/16 px checks) · Grid Colors: [Light/Medium/Dark/Red/Orange/Green/Blue/Purple/Custom ▾] and two 24×24 swatches · a preview box. The Gamut Warning section is omitted (no CMYK). Defaults: Medium, Light (`#FFFFFF`/`#CCCCCC`). |
| **Units & Rulers** | Rulers: Units [pixels/inches/cm/points/picas/percent ▾] · Column Size: Width [15] [picas ▾] Gutter [1] [picas ▾] · Point/Pica Size: (•)PostScript (72 points/inch) ( )Traditional (72.27 points/inch) |
| **Guides & Grid** | Guides: Color [Light Blue ▾] swatch, Style [Lines/Dashed Lines ▾] · Grid: Color [Custom ▾] swatch, Style [Lines/Dashed Lines/Dots ▾], Gridline every [1] [inches ▾], Subdivisions [4] |
| **Scratch Disks & Updates** [PCC rename] | Scratch folder: [path…][Choose…] (temporary tile swap file) · Updates: ☑ Check for updates on startup (weekly) · [Check Now] · Update server: (read-only) |
| **Memory & Image Cache** | Image Cache: Cache Levels [4] (1–8) · ☑ Use cache for histograms · Physical Memory Usage: Available RAM: 32768K (static) · Used by PhotoCraft: [50]% → `16384K` · History: Maximum states [20] (mirrors the History options) |

### 9.14 Layer Properties… / New Layer… (measured New Layer 365×148)

- **New Layer**: Name [Layer 1], ☐ Group With Previous Layer, Opacity [100] %, Mode [Normal ▾],
  and a disabled line "(No neutral color exists for Normal mode.)" with ☐ Fill with
  Overlay-neutral color. This is PS5.
- **Layer Properties** (CS6 name): Name [ ] and Color [None ▾]. The color tags the eye cell; it is
  a [CS6] nicety. Opacity, Mode and Blend If move to Blending Options.

### 9.15 Layer Style (PS5 "Layer Options" + "Effects", CS6 menu names)

**Blending Options…** is the PS5 Layer Options dialog (measured 392×267):

```
┌ Layer Style: Blending Options ─────────────────── [×]┐
│ Name: [Layer 1_________________________]  [  OK  ]   │
│ ┌ Options ──────────────────────────────┐ [Cancel]   │
│ │ Opacity: [100] %  Mode: [Normal     ▾]│ ☑ Preview  │
│ └───────────────────────────────────────┘            │
│ ┌ Blend If: [Gray ▾] ────────────────────────────┐   │
│ │ This Layer:      0        255                  │   │
│ │ [■■■■ gradient 256×10 □□□□]                    │   │
│ │ ▲                         △   (Alt splits)     │   │
│ │ Underlying:      0        255                  │   │
│ │ [■■■■ gradient 256×10 □□□□]                    │   │
│ │ ▲                         △                    │   │
│ └────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

**Effects dialog** (Drop Shadow…, Inner Shadow…, Outer Glow…, Inner Glow…, Bevel and Emboss…):
PS5 used **one dialog with a page drop-down**, not CS6's style list. We keep the PS5 form, which
is smaller and needs no list control.

```
┌ Effects ─────────────────────────────────────── [×]┐
│ [Drop Shadow       ▾] ☑ Apply          [  OK   ]   │
│ ┌──────────────────────────────────┐   [Cancel ]   │
│ │ Mode:     [Multiply   ▾] [■]     │   [ Prev  ]   │
│ │ Opacity:  [75 ]%▸                │   [ Next  ]   │
│ │ Angle:    [120]° ◔  ☑ Use Global Angle            │
│ │ Distance: [5  ] pixels           │   ☑ Preview   │
│ │ Blur:     [5  ] pixels           │               │
│ │ Intensity:[0  ]%                 │               │
│ └──────────────────────────────────┘               │
└────────────────────────────────────────────────────┘
```

- Page shortcuts: Ctrl+1…5 (PS5).
- Bevel and Emboss: Highlight Mode, color and Opacity; Shadow Mode, color and Opacity; Style
  [Outer Bevel/Inner Bevel/Emboss/Pillow Emboss]; Angle; Depth; Blur; Up/Down.
- Glow: Mode, Opacity, Blur, Intensity, color; plus Center/Edge for Inner Glow.
- The caption reads "Layer Style" with the page name. In the PS5 original it read "Effects".
  Decision: caption `Layer Style`.

### 9.16 Free Transform and Numeric Transform

- **Free Transform** (Ctrl+T):
  - A bounding box with 8 handles: 5×5 px squares with a 1 px black frame and white fill. A centre
    reference point (a 7 px circle with a cross).
  - The cursor changes over handles; outside the box it is a rotate cursor.
  - **Ctrl**-drag a corner = distort. **Ctrl+Shift** = skew. **Ctrl+Alt+Shift** = perspective.
    **Alt** = symmetric. **Shift** = constrain. All PS5 p.186.
  - Enter or a double-click inside commits. Esc cancels.
  - While transforming, the Options bar shows [CS6] `X:[ ] Y:[ ] W:[ ]% H:[ ]% ∠:[ ]° H:[ ]° V:[ ]°`
    and ✓ ⊘. PS5 showed these in the Info palette.
- **Edit > Transform > Numeric…** (PS5 p.187), about 300×300:
  - ☑ Position: X [ ] Y [ ] [pixels ▾], ☐ Relative.
  - ☑ Scale: Width [100] Height [100] [percent ▾], ☑ Constrain Proportions.
  - ☑ Skew: Horizontal [0]° Vertical [0]°.
  - ☑ Rotate: Angle [0]° with a 40 px angle dial.
  - OK, Cancel.

### 9.17 Page Setup… and Print…

- **Page Setup** (measured PS5 = 438×431, the Windows common page-setup dialog with PS5 buttons
  added).
  - Ours: the system `PrintDlg(PD_PRINTSETUP)` is acceptable on all versions (Win95 has it), plus
    our own **Print Options** sub-dialog launched from a [Options…] button: Caption ☐, Labels ☐,
    Crop Marks ☐, Center Crop Marks ☐, Registration Marks ☐, Negative ☐, Emulsion Down ☐,
    Interpolation ☐, [Background…], [Border…].
  - In v1 only Labels, Crop Marks, Background and Border are active.
- **Print…**: the system `PrintDlg` (Win95 look), plus `☐ Print Selected Area`. Print uses the
  document resolution, with scaling via **CS6's Print dialog fields**: Position (☑ Center Image,
  Top, Left) and Scaled Print Size (Scale %, Height, Width, ☐ Scale to Fit Media). These go in a
  `Print with Preview…` dialog, about 560×380, with a 200×260 page preview. The command is PS 5.5
  Print Options / CS6 Print, and it is [later].

### 9.18 Other dialogs (brief)

- **Fill…**: Contents Use [Foreground/Background/Pattern/History/Black/50% Gray/White ▾];
  Blending Opacity % and Mode; ☐ Preserve Transparency.
- **Stroke…**: Width px, Location (•)Inside ( )Center ( )Outside, Blending, ☐ Preserve
  Transparency.
- **Feather Selection**: Feather Radius [ ] pixels (0.2–250).
- **Border / Smooth / Expand / Contract**: one numeric field each.
- **Color Range…**: Select [Sampled Colors ▾], Fuzziness slider (0–200), a 150×150 preview with
  (•)Selection ( )Image, Selection Preview [None ▾], eyedroppers ×3, ☐ Invert, Load/Save.
- **Indexed Color…**: Palette [Exact/System (Windows)/Web/Uniform/Adaptive/Custom/Previous ▾],
  Color Depth, Colors, Dither [None/Pattern/Diffusion ▾]. This is PS5 p.73; we drop Perceptual and
  Selective, which are PS 5.5.
- **Variations…**: PS5 p.129. Original and Current Pick thumbnails; 7 color thumbnails around the
  centre; a Lighter/Darker column; a Fine–Coarse slider; Shadows/Midtones/Highlights/Saturation
  radios; ☐ Show Clipping.
- **Type Tool dialog** (PS5): Font [ ▾] [style ▾], Size [ ] [points ▾], Leading, Kerning ☐Auto,
  Color swatch, Tracking, Baseline, ☐Anti-Alias [Crisp ▾], Alignment ×3, ☐Fractional Widths, ☐Auto
  Kern, ☑Preview. A big edit box (400×100) for the text holds multiple lines. About 520×360.
- **About**: our own art. A 16-color 400×200 bitmap with "PhotoCraft Classic", the version and
  credits. A scrolling credits list opens on a click. It includes the full NOTICE text and links.

---

## 10. Document windows

### 10.1 Tabs (the default; Preferences > General > Open Documents as Tabs)

- **Label** (CS6 format): `<file> @ <zoom>% (<target>, <mode>/8)`, then ` *` when there are
  unsaved changes.
  - Zoom uses at most 1 decimal and drops a trailing `.0`: 100%, 66.7%, 33.3%, 16.7%, 12.5%,
    300%.
  - Target: the active layer's name (`Layer 1`). It is omitted for a Background-only document:
    `Untitled-1 @ 100% (RGB/8)`. It is `Layer 1, Layer Mask` when the mask is targeted, and
    `Quick Mask` in Quick Mask mode.
  - Mode: `RGB/8`, `Gray/8`, `Index`, `Bitmap`. CS6 writes `Index` and `Bitmap` without `/8`.
- **Shape**: Win95 property-sheet tabs.
  - Unselected tabs are 16 px tall with Hi top/left, Sh/Dk right, and a 1 px rounded corner
    (diagonal pixel).
  - The selected tab is 18 px tall, extends 2 px left and right, has no bottom line, and merges
    with the document frame.
  - Labels have 6 px padding.
  - A **close glyph** (× in a 9×9 cell) sits at the right of every tab. It is drawn sunken while
    pressed. Middle-click also closes.
- **Width**: label width + 6 + 9 + 6, minimum 60 px, maximum 240 px. Longer labels elide the
  *file name* with `...`, keeping the `@ zoom (…)` part.
- **Overflow**: a » button at the right of the strip drops down the list of hidden documents
  (CS6). Ctrl+Tab and Ctrl+Shift+Tab cycle documents (CS6 and PS5).
- **Dragging**: drag a tab along the strip to reorder. Drag it off the strip to float it (Window >
  Arrange > Float in Window). Drag a floating window's caption onto the strip to dock it. The drop
  zone is an XOR 3 px frame. Ctrl prevents docking.

### 10.2 Floating document windows and Arrange

- A floating document is a child of the frame client area. It is drawn like a Win98 MDI child:
  - 4 px sizing frame;
  - 18 px caption with the gradient (inactive: `#808080→#B5B5B5`, text `#C0C0C0`);
  - our 16×16 document icon;
  - the same label as the tab, in bold;
  - Min/Max/Close buttons, 16×14.
- **Maximize** = consolidate into tabs.
- **Window > Arrange** (CS6):
  - *Tile All Vertically / Horizontally* is a split view of all documents within the tab area.
  - *Consolidate All to Tabs*.
  - *Cascade* steps by the caption height (19 px) from the top-left.
  - *Tile* is edge-to-edge and re-fits when a window closes.
  - *Float in Window*, *Float All in Windows*.
  - *Match Zoom*, *Match Location*, *Match All*.
  - *New Window for <doc>* is a second view of the same document (PS5: View > New View).
- **Minimized floating window**: a 160×19 caption bar parked at the bottom of the document area,
  as MDI icons were in Win95 MDI.

### 10.3 Per-document status bar (17 px bottom row)

```
[66.67% ][Doc: 795K/795K       ▸][◂▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒ horizontal scroll bar ▒▒▒▒▒▒▒▒▒▒▒▒▸][ ]
 52×16    BDR_SUNKENOUTER 150×16  11×16                                                       16×16 size box
```

- **Zoom field**: a white edit field showing 2 decimals (`66.67%`). Enter applies it.
  Shift+Enter applies and keeps the focus (PS5 p.36).
- **Info pane**: a ▸ button opens a menu with:
  - ●Document Sizes (`Doc: flattened/with layers`);
  - Document Dimensions (`640 px × 480 px (72 ppi)`);
  - Scratch Sizes (`Scr: used/available`);
  - Efficiency (`Eff: 100%`);
  - Timing (`0.4 sec`);
  - Current Tool.
  These are CS6's list (pp.118–119) minus Version Cue, Profile, Measurement Scale and 32-bit
  Exposure. PS5 had the first four plus Current Tool.
- **Pressing on the info pane** (mouse held down) shows the PS5/CS6 *print preview* popup: the
  page rectangle with the image box.
- **Alt-pressing it** shows `Width: 640 pixels (8.889 inches) / Height: … / Channels: 3 (RGB
  Color) / Resolution: 72 pixels/inch` in a tooltip-style popup. This is PS5 behaviour; CS6 uses
  a plain click.
- **The horizontal scroll bar** takes the rest of the row, as in CS6 and PS5-Mac. The vertical
  scroll bar is on the right of the canvas.

### 10.4 Application status bar (optional; Window > Status Bar, off by default)

- PS5 Windows had a 20 px status bar at the bottom of the *program* window (measured y=544…565),
  holding: zoom field, `Doc:` pane, ▸ menu, and the active tool's hint text, for example:
  "Click and drag to move layer or selection. Shift constrains move to 45 degree increments."
- We offer it for period flavour and for tool hints. Since CS6 has no application status bar, it
  is off by default. When it is on, the per-document zoom and Doc panes hide (no duplication), and
  the progress bar for long operations appears in it. With it off, progress appears in the
  per-document info pane as a 150×12 segmented Win95 progress bar (navy blocks).

### 10.5 Screen modes (F cycles forward, Shift+F backward; View > Screen Mode; Tools panel button)

| Mode | What shows | Implementation |
|---|---|---|
| Standard Screen Mode | Everything (§3) | Normal overlapped frame |
| Full Screen Mode With Menu Bar | Menu bar, Options bar, Tools, dock. No caption and no taskbar overlap. Only the active document, centred on `#808080` (CS6 "50% gray") without tabs or scroll bars. | The frame becomes `WS_POPUP` sized to the monitor (`GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`); restore the style on exit |
| Full Screen Mode | Only the image, on **black**. No menu bar. Tab toggles the panels (Tools, Options bar, dock) over the image. F (next mode) or Esc returns to Standard. CS6's hover-to-reveal panels at the screen edges is omitted. | Same popup window, with the panels hidden |

---

## 11. Interaction reference

| Gesture | Behaviour | Source |
|---|---|---|
| **Tab** / **Shift+Tab** | Hide or show all panels / all except Tools and Options | PS5 p.28; CS6 p.34 |
| **F** / **Shift+F** | Cycle screen modes | PS5 (F), CS6 |
| **Space** (held) | Temporary Hand | PS5 p.34 |
| **Ctrl+Space** / **Alt+Space** (held) | Temporary Zoom In / Zoom Out. Click or drag a marquee. | PS5 p.35 |
| **Ctrl+= / Ctrl+-** | Zoom in/out by preset steps | PS5 p.35 |
| **Alt+wheel** | Zoom about the pointer (CS6). Plain wheel scrolls vertically; Ctrl+wheel scrolls horizontally (CS6). | CS6 |
| | **Wheel availability**: `WM_MOUSEWHEEL` exists on Win98/NT4 SP3+. On Win95 the IntelliPoint driver posts the registered `MSWHEEL_ROLLMSG` message, so handle both. | |
| **Zoom presets** | 0.2%…1600% (PS5 maximum) on the PS5/CS6 ladder: 1600, 1200, 800, 700, 600, 500, 400, 300, 200, 100, 66.67, 50, 33.33, 25, 16.67, 12.5, 8.33, 6.25, 4.17, 3.13, 2.08, 1.56, 1.04, 0.78, 0.52, 0.39, 0.26, 0.2 | PS5/CS6 *(recollection)* |
| Double-click Hand / Zoom | Fit on Screen / Actual Pixels | PS5 p.35 |
| Page Up/Down; Shift+Page Up/Down | Scroll by a screen / by 10 px | PS5 p.34 |
| Ctrl+Page Up/Down | Scroll left/right by a screen | CS6 |
| Home / End | Top-left / bottom-right | CS6 |
| **Caps Lock** | Precise cursors toggle | PS5 p.27 |
| **[ / ]** | Brush size down/up in the brush set order. With the Magnetic Lasso: width ±1 px. | PS5 p.141 |
| Shift+[ / Shift+] | First / last brush | PS5 |
| **1…0** | With a painting tool: opacity 10%…100%. With a non-painting tool: the active layer's opacity. | PS5 p.272 |
| Arrow keys (Move tool) | Nudge 1 px; Shift = 10 px | PS5 |
| Alt+Backspace / Ctrl+Backspace | Fill with foreground / background | PS5 |
| Esc during a long operation | Cancel (hold) | PS5 p.167 |
| Palette dragging | Drag by the caption (floating) or the tab-row background (docked) to move a group. Drag a tab to move one panel. | CS6 pp.36–38 |
| Double-click a palette tab | Collapse/expand the group | PS5 p.29; CS6 |
| Alt-click the palette minimize box | Collapse to titles only | PS5 p.29 |
| Right-click a panel tab | Close, Close Tab Group (CS6) | CS6 p.37 |
| Drag the canvas edge | No effect (CS6 has no canvas-edge resize) | |
| Drag a file from Explorer onto the frame | Open it (WM_DROPFILES; Win95 OK) | |

**Cursor behaviour.**

- Over a palette, menu or dialog the arrow cursor is shown.
- Over the canvas the tool cursor is shown.
- Over a document tab the arrow cursor is shown. While dragging a tab, the drag cursor is the
  system "move" cursor (`IDC_SIZEALL` is acceptable).
- An unavailable action shows the PS5 "no" cursor: a circle with a slash, 32×32, mono. One example
  is painting on a hidden layer.
- A wait cursor (`IDC_WAIT`) appears only for operations longer than 250 ms that block the UI.
  Filters show the progress bar instead.

---

## 12. Period details borrowed from other 1998 editors

These are *(unverified)*: the only sources reachable for these programs were search snippets and
one review.

- **Paint Shop Pro 5** (Jasc, April 1998).
  - Floating toolbars and a separate *Tool Options* window. The PSP Tool Options was a small
    tabbed palette; it is the period precedent for tabbed option pages.
  - Its *Color palette* shows an "available colors" spectrum with FG/BG swatches. Our Color
    panel's spectrum ramp is the same idea.
  - *Undo History* (new in PSP5, irt.org review): a list dialog with "Undo to selected". It
    confirms the History idiom was period-standard by 1998.
  - We borrow: **status bar image info** "Image: 640 x 480 x 16 Million". This is optional text
    for our Document Dimensions mode.
- **Corel Photo-Paint 8** (1997).
  - *Roll-ups*: floating panels that roll up into their caption when the caption's ▲ button is
    clicked. This is the same idea as our double-click-to-collapse.
  - The *Property Bar* (introduced with Corel 8): a horizontal, context-sensitive bar under the
    menu.
  - **These two are the strongest 1997–98 precedent for putting CS6's Options bar into a 1998
    look.** Photoshop got its Options bar only in 6.0 (2000).
- **Ulead PhotoImpact 4** (1998).
  - The *Attribute toolbar*: a horizontal tool-options bar under the menu.
  - *EasyPalette*: a docked gallery of thumbnails (effects and styles) with drag-onto-image.
  - Three interface levels (basic toolbar, intermediate vertical toolbar, advanced: color palette
    plus EasyPalette; kaisernet review).
  - We borrow nothing visual, but it confirms horizontal option bars were a 1998 idiom.
- **Common period dialog traits worth copying**:
  - live preview check boxes;
  - small (100×100) previews with a zoom −/+ pair;
  - numeric fields **paired** with sliders;
  - Load…/Save… for settings files;
  - Alt turns Cancel into Reset (PS5).

---

## 13. Reference screenshots (URLs only; do not commit these images)

All were measured in a scratch folder on 2026-10-30. Copyright belongs to their owners; they are
listed for reference.

| # | URL | What it shows | Our measurements taken |
|---|---|---|---|
| 1 | https://guidebookgallery.org/pics/apps/photoshop/workspace/full/500-1-1.png | PS 5.0 on Win98 at 800×600 (794×566 shown): toolbox, three palette groups, a document, the app status bar | Caption 18 px; gradient endpoints; menu bar; toolbox 60 px wide with 25×21 cells; palette 212 px wide; 12 px palette caption; 14 px tabs; status bar 20 px |
| 2 | https://guidebookgallery.org/pics/apps/photoshop/workspace/empty/500.png | Empty workspace (default palette layout) | Default PS5 arrangement |
| 3 | https://guidebookgallery.org/pics/apps/photoshop/workspace/menus/500-1-1.png | File menu with the Preferences submenu open | Item 17 px; separator 9 px; popup frame; sunken menu-bar item; accelerator notation `Shft+Ctrl+S` |
| 4 | https://guidebookgallery.org/pics/apps/photoshop/workspace/toolbox/500-1-1.png | Toolbox (59×374) | Group layout |
| 5 | https://guidebookgallery.org/pics/apps/photoshop/usage/colours/levels/500.png | Levels (405×280) | Button 60×20 at a 28 px pitch; histogram 256 wide |
| 6 | https://guidebookgallery.org/pics/apps/photoshop/usage/colours/colourpicker/500-1-1.png | Color Picker (494×328) | Field 256×256 at (14,54); hue strip 20 wide |
| 7 | https://guidebookgallery.org/pics/apps/photoshop/usage/colours/huesaturation/500.png | Hue/Saturation (341×262) | Layout |
| 8 | https://guidebookgallery.org/pics/apps/photoshop/usage/colours/brightnesscontrast/500.png | Brightness/Contrast (312×129) | Layout |
| 9 | https://guidebookgallery.org/pics/apps/photoshop/usage/resizing/imagesize/500.png | Image Size (400×312) | Field 70×20; etched group boxes |
| 10 | https://guidebookgallery.org/pics/apps/photoshop/usage/resizing/canvassize/500.png | Canvas Size (335×294) | Layout |
| 11 | https://guidebookgallery.org/pics/apps/photoshop/usage/filters/gaussianblur/500.png | Gaussian Blur (206×211) | 100×100 preview |
| 12 | https://guidebookgallery.org/pics/apps/photoshop/inputoutput/images/newimage/500.png | New (363×332) | Layout |
| 13 | https://guidebookgallery.org/pics/apps/photoshop/settings/preferences/500-1-1.png | Preferences > General (428×339) | Panel drop-down; Prev/Next |
| 14 | https://guidebookgallery.org/pics/apps/photoshop/usage/layers/layeroptions/500.png | Layer Options (392×267) | Blend If |
| 15 | https://guidebookgallery.org/pics/apps/photoshop/usage/layers/layerspaletteoptions/500.png | Layers Palette Options (244×293) | Thumbnail boxes 33/58/83 px |
| 16 | https://guidebookgallery.org/pics/apps/photoshop/usage/layers/newlayer/500.png | New Layer (365×148) | Layout |
| 17 | https://guidebookgallery.org/pics/apps/photoshop/inputoutput/printing/pagesetup/500.png | Page Setup (438×431) | System dialog plus additions |
| 18 | https://pe-images.s3.amazonaws.com/basics/cs6/interface/manage-panels/collapse-icon.gif | CS6 dock: History/Properties icon column expanded next to Color\|Swatches and Adjustments\|Styles | CS6 group contents; tab row about 20 px; dock header about 10 px (1:1 crop, 540×263) |
| 19 | https://pe-images.s3.amazonaws.com/basics/cs6/interface/manage-panels/essentials-workspace.gif | CS6 Essentials (reduced) | Arrangement |
| 20 | https://pe-images.s3.amazonaws.com/basics/cs6/interface/manage-panels/double-arrow-icon.gif | CS6 dock double-arrow (collapse to icons) | Chevron placement |
| 21 | https://pe-images.s3.amazonaws.com/basics/photoshop-cs4/interface/tools-panel-double-column.gif | CS4/CS6-style two-column tools | Two-column flow |
| 22 | Full GUIdebook PS5 index: https://guidebookgallery.org/apps/photoshop/500 | Also About, Help, Transform, Fill, Stroke, Marquee and Paintbrush option palettes, Histogram, Batch, File Info, Print | — |

---

## 14. Implementation checklist (for `src/ui/`)

1. A theme module that reads `GetSysColor` and `SPI_GETNONCLIENTMETRICS` at start-up and on
   `WM_SYSCOLORCHANGE`/`WM_SETTINGCHANGE`, with fallbacks to §2.1.
2. Bevel primitives matching §2.4 exactly: `pc_edge_window`, `pc_edge_button`,
   `pc_edge_button_pressed`, `pc_edge_sunken`, `pc_edge_thin_raised`, `pc_edge_thin_sunken`,
   `pc_etched_h/v`, `pc_dither_fill`, `pc_text_disabled`.
3. A custom menu bar plus popup engine (§2.5) driven by a static command table. The table is
   shared with accelerators and the Window panel list.
4. A dock layout engine: columns → groups → panels, with icon mode, flyouts and floating palettes.
   INI serialization per workspace.
5. A tools panel (§5), an Options bar (§7), document tabs with floating children (§10), the status
   row, screen modes.
6. The dialog framework: PS5 metrics (buttons 60×20 at a 28 px pitch, fields 20 px), the slider,
   the pop-up slider, the Alt=Reset behaviour, the 100×100 preview, Preview check box.
7. Every string in the UI must fit at MS Sans Serif 8. Test the 800×600 and 1024×768 layouts and
   16- and 256-color modes.
