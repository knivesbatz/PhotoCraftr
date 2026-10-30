# Design decisions

This is a short record of the decisions that shape PhotoCraft Classic, with the reason for each.
The detailed UI specification is in [`historical-ui-reference.md`](historical-ui-reference.md).
The upstream audit is in [`photocraft-audit.md`](photocraft-audit.md).

| # | Decision | Why |
|---|---|---|
| D1 | **Legacy-first C99 + Win32**, cross-compiled with `i686-w64-mingw32`. The binaries link no C runtime (`src/rt/pcrt.c` on KERNEL32), use an i486/x87 baseline and declare PE subsystem 4.0. | **The user chose this.** It is the only practical way to control every import and the PE header for Windows 95 through 10. The Rust upstream cannot target Win9x; see the audit. |
| D2 | **Workspace arrangement = Photoshop CS6 for Windows (2012).** That means: the menu bar; the Options bar; a one-column Tools panel at the left with a « » toggle; a right dock with an icon column and an expanded column of tabbed groups; document tabs in an application frame; a per-document status bar; three screen modes. | **The user chose this.** It is the arrangement current users know, and it uses screen space better than PS5's floating palettes at 800×600 and 1024×768. |
| D3 | **Rendering = Windows 95/98 and Photoshop 5.0 era.** That means `#C0C0C0` bevels, navy captions with the Win98 gradient, MS Sans Serif 8, 16-color icons, PS5 palette tabs, pop-up sliders and PS5 dialogs. | **The user chose this.** It looks native on the target systems and is cheap to draw with GDI in 16 or 256 colors. Photoshop 5.0 (May 1998) is the reference for dialogs, palette internals, menu density and feature scope. |
| D4 | **Documents open as tabs by default; floating windows are supported** (Window > Arrange: Float in Window, Float All in Windows, Cascade, Tile, Consolidate All to Tabs). | Tabs are CS6's default, need no window management at 800×600 and keep the frame tidy. Floating windows keep side-by-side comparison and the PS5 MDI feel, and Cascade/Tile are period commands. Both are drawn by us, not with the system MDI, so they look the same on every Windows version. |
| D5 | **Menus are custom-drawn** (no system `HMENU` rendering). | (a) Identical look on 95…10: Windows 2000+ hides mnemonics and XP+ themes and flattens menus. (b) Full Screen Mode With Menu Bar needs a menu bar in a captionless popup window. (c) Dynamic items (workspaces, the panel list with check marks, recent files) and PS5 accelerator text (`Shft+Ctrl+S`) without owner-draw quirks on Win95. (d) A menu bar that is a child of our frame can be hidden with Tab-style workspace toggles. |
| D6 | **No 3D menu** (CS6's menu bar minus 3D), and no 3D tools or filters (PS5's 3D Transform filter is omitted too). | Out of the 1998 feature set. There is no GPU path on the target hardware, and it would be large to build. |
| D7 | **The tool set is PS 5.0's list in CS6 groups.** **Kept:** Move; the Marquee group (rectangular, elliptical, single row, single column); the Lasso group (lasso, polygonal, magnetic); Magic Wand; Crop; the Eyedropper group (eyedropper, color sampler, measure); the Brush group (paintbrush, airbrush, pencil); Clone/Pattern Stamp; History Brush; Eraser; Gradient/Paint Bucket; Blur/Sharpen/Smudge; Dodge/Burn/Sponge; the Pen group (pen, freeform, magnetic, add, delete, convert); Path/Direct Selection; the Type group (H, V, mask H, mask V); the Shape group (rectangle, ellipse, line); Hand; Zoom; FG/BG colors; Quick Mask; Screen Mode. | It is the feature set of 1998 plus CS6's grouping. Path Selection and the Rectangle/Ellipse shapes (PS6+) are the only additions, and only because the CS6 grouping needs them. Shapes fill pixels only. |
| D8 | **CS6 tools omitted:** Quick Selection, Spot Healing, Healing Brush, Patch, Content-Aware Move, Red Eye, Color Replacement, **Mixer Brush**, Art History Brush, Background/Magic Eraser, Slice/Slice Select, Note, Count, Rotate View, 3D tools, Rounded Rectangle, Polygon, Custom Shape. **Also no Content-Aware Fill or Scale, no Puppet Warp, Liquify, Vanishing Point, Camera Raw, Smart Objects or Smart Filters, and no AI or ML features.** | Not in the 1998 feature set. Most need large memory, FFTs or ML that a 486/Pentium with 16–32 MB cannot run interactively. The user asked for no AI. |
| D9 | **Default dock: icon column [History \| Actions], [Brushes]. Expanded column [Color \| Swatches], [Navigator \| Info], [Adjustments], [Layers \| Channels \| Paths].** | The top and bottom groups match CS6 Essentials exactly. [Navigator \| Info] (PS5's top group) takes the slot of CS6's [Adjustments \| Styles] (Styles has no PS5 equivalent). Adjustments sits right above Layers as in CS6, as one compact row of buttons. History is CS6's top icon. Brushes is iconic because the Options bar has its own brush picker. |
| D19 | **Adjustment layers: only the nine types Photoshop 5.0 had** (Brightness/Contrast, Levels, Curves, Color Balance, Hue/Saturation, Channel Mixer, Invert, Threshold, Posterize). No Vibrance, Black & White, Photo Filter, Exposure, Gradient Map, Color Lookup or Selective Color. They are saved in PSD with Photoshop's own adjustment blocks. | **The user chose this:** only what a 1998 studio worked with. Each type reuses the destructive command's code and dialog, so a layer renders exactly what Image > Adjust would write. Adjustment layers hold no pixels (a 1024×768 layer would cost 3 MB); painting, fills and gradients on one edit its mask, as in Photoshop. |
| D10 | **Options bar = CS6 position, PS5 widgets and option names** (e.g. `Use All Layers`, `Fade: steps`, `Wet Edges`, pop-up sliders). A few CS6 additions are kept: selection-mode buttons, gradient-type buttons, Contiguous, commit/cancel. | PS5's Options *palette* moves to CS6's horizontal bar. Corel Photo-Paint 8's Property Bar and PhotoImpact 4's Attribute toolbar show the idea is period-plausible in 1998. |
| D11 | **8 bits per channel; RGB, Grayscale, Indexed and Bitmap modes only.** No CMYK, Lab, Duotone, Multichannel, 16 or 32 bit. | Memory: a 1024×768 RGBA layer is 3 MB, which matters on 16 MB machines. There is no ICC color management on Win95 (ICM 2.0 arrived with Win98). Simplicity. |
| D12 | **PSD is the native format** (layers, masks, a subset of effects, RLE/PackBits). The other formats (BMP, PNG, JPEG, GIF, TGA) use stb plus our own writers. | Interchange with every editor since 1990. There is no proprietary format to maintain. |
| D13 | **Our own Win95-style Open/Save dialogs** (with a PS5-style preview and file size). The system `ChooseColor` is offered only as an alternative picker, and the system `PrintDlg` is used for printing. | Same look on 95…10, and thumbnails. `GetOpenFileName` hooks are fragile on Win95. |
| D14 | **System colors are read at run time**, with the Win95/98 "Windows Standard" defaults as fallback. Gradient captions are drawn only on Win98+ at 15 bpp or more. | Respects the user's Appearance scheme and High Contrast. Correct in 16- and 256-color modes. |
| D15 | **No Adobe artwork, code, logos or names.** Generic tool and command vocabulary is used. Icons are original 16-color drawings. Product name: **PhotoCraft Classic**. | Legal. We copy behaviour and layout only. |
| D16 | **The upstream PhotoCraft (Rust) is used as a reference for algorithms, not as code.** Algorithms are reimplemented in C with attribution (MIT OR Apache-2.0 kept). | Upstream cannot run on our targets; see the audit. Its algorithms are well tested against Photoshop oracles. |
| D17 | **A "Classic 5.0" workspace** [PCC] reproduces PS5's floating palette arrangement, in addition to Essentials. | A cheap nod to the period look. It is selectable from Window > Workspace. |
| D18 | **One-column tools at 1024×768 and above; two columns automatically at 800×600** (the user can override). | One column needs about 527 px of height; 800×600 leaves 504 px. |
| D19 | **Exports are limited to what a 1998 game studio could produce** (see "1998 output rules" below). There is one quantizer for Indexed Color and every 8-bit export, plus the magenta colour key. **PNG is the one deliberate modern exception:** full 8-bit alpha at 32 bits, and partial alpha per palette entry in PNG-8. | **The user chose this as a hard rule.** The writers enforce it, so dialogs, Actions and code paths cannot get around it. The user asked for the PNG transparency exception explicitly. |

## 1998 output rules

`pc_save_mem` checks every request against this table, and so does each codec on its own. Depth 0 means "the format's default".

| Format | Bits per pixel | Details |
|---|---|---|
| BMP | 8, 16, 24 | 8: palette (a gray ramp for Grayscale documents). 16: 5-5-5 `BI_RGB`, or 5-6-5 `BI_BITFIELDS`; Windows 95 reads both. There is no 32-bit output. |
| Targa | 8, 16, 24, 32 | 8: colour-mapped with a 24-bit map (type 3 gray for Grayscale documents). 16: A1R5G5B5. 32: 8-bit alpha. Rows are stored bottom-up. RLE is optional, and its packets never cross a scanline. |
| GIF | 8 | GIF89a with a transparent index and interlacing. |
| PNG | 8, 24, 32 | 8: PLTE plus tRNS, or 8-bit gray. 32: RGBA, or gray plus alpha. Never 16 bits per channel. |
| TIFF | 8, 24 | 8: palette colour (ColorMap) or gray. Compression: none, LZW or PackBits. Transparency is matted or turned into the colour key. |
| JPEG | 8, 24 | Sequential baseline only, never progressive. 24-bit is always YCbCr 4:2:0. Photoshop's 0-12 quality scale. |
| PSD | 8 bits per channel | Layered, unchanged. |

**Palettes (`src/core/quantize.c`).** These follow Photoshop 5's Indexed Color dialog:

- **Exact:** up to 256 colours.
- **System (Windows):** the 20 static colours at 0-9 and 246-255, with the 6×6×6 cube and a gray ramp between them.
- **System (Mac OS):** white, the descending cube, the R/G/B/gray ramps, then black.
- **Web:** the 216-colour cube.
- **Uniform:** an r×g×b grid, so 256 colours give 6×7×6.
- **Adaptive:** median cut on a 15-bit histogram, entries are the means of the real colours, and an image with few enough colours keeps them exactly.
- **Document:** the palette Image > Mode > Indexed Color stored, reused index for index.

**Dithers.** None, Pattern (8×8 Bayer) and Diffusion (Floyd-Steinberg). Colours that are already in the palette are never moved. 16-bit output rounds each channel to the nearest 5- or 6-bit level and expands it back the way our BMP reader does, with the same dithers. All of it is integer arithmetic, deterministic on x87 and on the host.

**Colour key.** Pixels with alpha below 128 become `key_rgb`, magenta (255, 0, 255) by default. The key's entry is reserved:

- A palette that already contains the key colour reuses that entry.
- Otherwise the key is appended.
- In a full 256-colour palette, the key replaces the most redundant entry.

Opaque pixels never dither onto the key, so the key appears exactly where the image was transparent.

**The PNG exception.** PNG-32 keeps every alpha value, and it is the default PNG depth when the document has transparency. With "Transparency: Full Alpha", PNG-8 quantizes in RGBA:

- Median cut runs on a 4-4-4-4 histogram, and distances are measured in premultiplied space.
- Fully transparent pixels share one entry.
- Translucent entries are sorted first, which keeps tRNS short.

Indexed Color mode stays binary (the 1998 kind).

**Indexed Color mode.** Indexed Color is not a separate pixel format. The document keeps RGBA pixels, all of them palette colours, and remembers the palette (`PcDoc.index_*`, outside history). It flattens to one layer and records one history state. Image > Mode > RGB Color forgets the palette. The command was appended to the command table, not placed beside Grayscale and RGB, so the ids stored in saved Actions stay valid.
