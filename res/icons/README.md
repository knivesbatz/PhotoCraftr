# PhotoCraft Classic icon art

All icons, cursors and emblems in this project are original works created
for PhotoCraft Classic. That covers the interface icons and cursors in
`src/ui/icons_data.c`, and the application, document, updater and setup
emblems drawn here as ASCII art and built into `res/*.ico`. They are drawn
pixel by pixel in the style of late-1990s Windows graphics programs. No
artwork from Adobe or any other proprietary product was copied or traced.

They are licensed **MIT OR Apache-2.0**, like the rest of the project (see
`LICENSE-MIT` and `LICENSE-APACHE` at the repository root).

## Files

| File | Purpose |
|------|---------|
| `app-16.txt`, `app-32.txt` | application emblem (framed picture + paintbrush) -> `res/app.ico` |
| `doc-16.txt`, `doc-32.txt` | document icon (dog-eared page with a picture) -> `res/doc.ico` |
| `update-16.txt`, `update-32.txt` | emblem + green download arrow -> `res/update.ico` |
| `setup-16.txt`, `setup-32.txt` | emblem + floppy disk -> `res/setup.ico` |

Rebuild the `.ico` files with `python3 tools/make_ico.py` (add
`--preview out.png` to render the written files back for checking). Each
`.ico` holds a 32x32 and a 16x16 image, stored as 4-bit (16-colour) BMP
icons with a 1-bit AND mask, which Windows 95 can read.

The interface icons and cursors live in `src/ui/icons_data.c`.
`python3 tools/render_icons.py` renders them to
`docs/images/icon-sheet.png` and `docs/images/icon-sheet-4x.png`.

## Palette

The palette is the Windows 95 16-colour VGA palette, so the art also looks
right in 16-colour display modes. Lines starting with `#` are comments.

| char | colour | char | colour | char | colour | char | colour |
|---|---|---|---|---|---|---|---|
| `.` | transparent | `k` | black 000000 | `m` | maroon 800000 | `G` | green 008000 |
| `o` | olive 808000 | `n` | navy 000080 | `p` | purple 800080 | `t` | teal 008080 |
| `g` | silver C0C0C0 | `d` | gray 808080 | `R` | red FF0000 | `L` | lime 00FF00 |
| `Y` | yellow FFFF00 | `B` | blue 0000FF | `F` | fuchsia FF00FF | `C` | aqua 00FFFF |
| `w` | white FFFFFF | | | | | | |
