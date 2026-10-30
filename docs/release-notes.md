Since 0.1.0 (not yet released)
==============================

- Adjustment layers: Brightness/Contrast, Levels, Curves, Color Balance,
  Hue/Saturation, Channel Mixer, Invert, Threshold and Posterize, the types
  Photoshop 5.0 had. Layer > New Adjustment Layer opens the adjustment's
  dialog with a live preview; the layer keeps its settings (Layer > Layer
  Content Options or a double-click on its thumbnail changes them) and has
  a mask that the painting tools, fills and gradients edit. Opacity, blend
  modes, clipping groups and visibility apply. Merge Down, Merge Visible
  and Flatten bake them in.
- Adjustments panel (Window > Adjustments): one button per type, placed
  above Layers in Essentials and grouped with Layers in 1998 Palettes.
  Workspaces saved by 0.1.0 still load and gain the new panel.
- PSD files keep adjustment layers, written with Photoshop's own
  adjustment blocks; the flattened image in the file includes them.
- Posterize and Channel Mixer now preview live; typed values in Levels
  preview too.

PhotoCraft Classic 0.1.0
========================

First preview release.

What's in it
------------
- Workspace arranged like Photoshop CS6 (menu bar, Options bar, Tools
  panel, panel dock with icon column, tabbed documents), drawn entirely in
  the Windows 95 style: gray 3D bevels, navy captions, MS Sans Serif,
  16-color pixel-art icons. The "1998 Palettes" workspace switches to
  Photoshop 5.0-style floating palette groups.
- Tools: Move, four Marquees, Lasso, Polygonal and Magnetic Lasso, Magic
  Wand, Crop, Eyedropper, Measure, Paintbrush, Airbrush, Pencil, Rubber
  Stamp, Pattern Stamp, History Brush, Eraser, Gradient (5 types), Paint
  Bucket, Blur, Sharpen, Smudge, Dodge, Burn, Sponge, Pen, Freeform and
  Magnetic Pen, anchor tools, path selection, four Type tools, Rectangle,
  Ellipse and Line, Hand and Zoom. Flyouts, shortcut letters, Shift-cycling.
- Layers with opacity, 17 blend modes, masks, clipping groups, locks, type
  layers and layer effects (drop shadow, inner shadow, outer glow, bevel).
- Channels (view single channels, alpha channels, Quick Mask), Paths, a
  History palette with snapshots and the History Brush, and Actions that
  record and replay commands with their dialog settings.
- Image adjustments: Levels, Curves, Color Balance, Brightness/Contrast,
  Hue/Saturation, Channel Mixer, Invert, Equalize, Threshold, Posterize,
  Desaturate, Auto Levels. Image Size, Canvas Size, Rotate Canvas, Crop,
  Trim, Free Transform and Numeric Transform.
- Filters: Blur, Sharpen, Noise, Stylize, Pixelate, Distort, Render and
  Other families (31 filters), with previews.
- Files: layered Photoshop PSD, BMP, GIF, JPEG, PNG, TIFF and Targa.
- Built-in updater (Help > Check for Updates) with signed manifests and a
  bundled TLS stack; Windows Installer package and setup program.

Compatibility
-------------
Built as a 32-bit Windows 4.0 program that uses only Windows 95 API
functions and no C runtime DLL. Tested under Wine in Windows 95, 98 and
10 modes; not yet tested on real Windows 95-10 machines. See
docs/compatibility.md.
