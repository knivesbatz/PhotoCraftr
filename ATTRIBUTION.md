# Attribution

This file lists every third-party component and every upstream source of derived code in this
repository, with its author, source and license. PhotoCraft Classic's own code and original
assets (icons, cursors, the About bitmap, `res/*.ico`) are MIT OR Apache-2.0; see
[`LICENSE-MIT`](LICENSE-MIT), [`LICENSE-APACHE`](LICENSE-APACHE) and [`NOTICE`](NOTICE).

When you add a component, an asset or derived code, add a row here in the same change. A
third-party license file must stay next to the component it covers.

## Bundled third-party code

| Component | Path | Author | Source (version) | License | License file |
|---|---|---|---|---|---|
| stb_image (image decoding: JPEG, PNG, BMP, GIF, TGA, PSD-flat, PNM, PIC, HDR) | `third_party/stb/stb_image.h` | Sean Barrett and contributors | https://github.com/nothings/stb, v2.30 (reference checkout `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`) | MIT **or** public domain (Unlicense), at your option | [`third_party/stb/LICENSE`](third_party/stb/LICENSE) |
| stb_image_write (PNG, BMP, TGA, JPEG writing) | `third_party/stb/stb_image_write.h` | Sean Barrett and contributors | https://github.com/nothings/stb, v1.16 | MIT **or** public domain (Unlicense), at your option | [`third_party/stb/LICENSE`](third_party/stb/LICENSE) |
| BearSSL (TLS 1.2 client, X.509, SHA-2 for the updater) | `third_party/bearssl/` (`inc/`, `src/`) | Thomas Pornin | https://bearssl.org/ (reference checkout `7bea48e5e850ab4cafbe68d3765cdaba13a86d6f`) | MIT | [`third_party/bearssl/LICENSE.txt`](third_party/bearssl/LICENSE.txt) |
| Monocypher (Ed25519 signature verification of the update manifest) | `third_party/monocypher/monocypher.{c,h}`, `third_party/monocypher/optional/monocypher-ed25519.{c,h}` | Loup Vaillant, Michael Savage, Fabio Scotoni and contributors; X25519/EdDSA parts from SUPERCOP ref10 (D. J. Bernstein, B.-Y. Yang, N. Duif, P. Schwabe, T. Lange; public domain) | https://monocypher.org/ (git master, reference checkout `f5f0a19a5dc6c14a49adb3cd63ea79a60f6ba77b`; CHANGELOG head 4.0.3) | BSD-2-Clause **or** CC0-1.0, at your option | [`third_party/monocypher/LICENCE.md`](third_party/monocypher/LICENCE.md) |

## Derived code (reimplemented in C from upstream PhotoCraft)

PhotoCraft (https://github.com/storytold/photocraft), Copyright (c) 2026 ArtCraft Team and the
PhotoCraft contributors, MIT OR Apache-2.0. The reference commit is
`b1cfff9d30298eea4f446a8f6a5e4285cb43c6e7` (v0.17.0).

Each derived C file carries a header comment that names its upstream file and says it was
modified. The table lists the planned mapping (see
[`docs/photocraft-audit.md`](docs/photocraft-audit.md) §3). **Add the actual path when the file
lands.**

| Our module (planned) | Upstream source | What is derived | License |
|---|---|---|---|
| `src/core/blend.c` | `crates/color/src/blend.rs`, `crates/compose/src/psblend.rs` | Blend-mode formulas (Photoshop variants) | MIT OR Apache-2.0 |
| `src/core/adjust.c` | `crates/compose/src/adjust.rs`, `adjust/tone.rs`, `adjust/color.rs`, `adjust/channels.rs` | Levels (gamma toe), Curves, Hue/Saturation, Color Balance, legacy Brightness/Contrast, Posterize | MIT OR Apache-2.0 |
| `src/core/filter_blur.c`, `filter_sharpen.c`, `filter_noise.c` | `crates/algo/src/blur.rs`, `separable.rs`, `sharpen.rs`, `sharpen/exact.rs`, `noise.rs` | Gaussian kernels, box-blur approximation, Unsharp Mask, median, noise hash | MIT OR Apache-2.0 |
| `src/core/resample.c`, `src/core/transform.c` | `crates/algo/src/resample.rs`, `transform.rs` | Resampling kernels, homography warp | MIT OR Apache-2.0 |
| `src/core/select.c`, `src/core/fill.c` | `crates/algo/src/selection.rs`, `selection/*.rs`, `selection_blur.rs`, `paint.rs` | Magic wand, selection combine and modify, bucket fill, gradient parameterization | MIT OR Apache-2.0 |
| `src/core/brush.c` | `crates/paint/src/brush.rs`, `dynamics.rs`, `render.rs` | Computed-tip dab model, spacing walker, stroke-buffer accumulation | MIT OR Apache-2.0 |
| `src/core/history.c`, `src/core/tiles.c` | `crates/ops/src/lib.rs`, `crates/raster/src/lib.rs` | Snapshot history over copy-on-write tiles | MIT OR Apache-2.0 |
| `src/core/compose.c`, `src/core/effects.c` | `crates/compose/src/composite.rs`, `masks.rs`, `effects/*.rs` | 8-bit compositing, masks, layer effects | MIT OR Apache-2.0 |
| `src/io/psd_read.c`, `src/io/psd_write.c`, `src/io/packbits.c`, `src/io/psd_doc.c` | `crates/psd/src/{header,resources,layer,tagged,compression,rows,image_data,builder}.rs`, `crates/io/src/psd_import*.rs`, `psd_export*.rs`, `effects_map*` | PSD structure, PackBits RLE, layer records, document mapping | MIT OR Apache-2.0. Upstream notes that some undocumented PSD details follow the MIT-licensed psd-tools and ag-psd projects. |

## Reference material (not distributed)

These sources were consulted for behaviour and layout only. Nothing from them is copied into the
repository. Their URLs are listed in
[`docs/historical-ui-reference.md`](docs/historical-ui-reference.md) §0 and §13.

| Material | Owner | Use |
|---|---|---|
| Adobe Photoshop 5.0 User Guide (via manualshelf.com) | Adobe Systems Inc. | Behaviour and layout reference |
| Adobe Photoshop CS6 Help (help.adobe.com archive PDF) | Adobe Systems Inc. | Workspace arrangement reference |
| Photoshop 5.0 screenshots on GUIdebook (guidebookgallery.org) | Adobe Systems Inc. (software); GUIdebook (collection) | Pixel measurements only |
| Photoshop CS6 screenshots on Photoshop Essentials | Adobe Systems Inc. (software); Steve Patterson / photoshopessentials.com | Panel arrangement only |
| Wine `syscolor.c` / `sysparams.c` default tables | The Wine project (LGPL-2.1+) | Default Win95/98 color *values* (facts) only; no code copied |
