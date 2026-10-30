/* Engine tests: document model, history, compositing, adjustments, filters,
 * transforms, selections, painting and fills. */
#include <stdio.h>
#include "testutil.h"
#include "core/pcops.h"

static const u8 *px_at(const PcDoc *d, int layer, int x, int y)
{
    return d->s.layers[layer]->pix->px + ((size_t)y * d->s.w + x) * 4;
}

static void test_fmt(void)
{
    char b[64];
    pc_snprintf(b, sizeof b, "%d|%5.2f|%-4s|%04x|%s", -12, 3.14159, "ab", 0xbeef, "z");
    CHECK(!strcmp(b, "-12| 3.14|ab  |beef|z"));
    pc_snprintf(b, 4, "%s", "abcdef");
    CHECK(!strcmp(b, "abc"));
    CHECK_EQ(pc_atol("  -42x"), -42);
    CHECK_NEAR(pc_atof("2.5") * 10, 25, 0);
}

static void test_doc_history(void)
{
    PcDoc *d = pc_doc_new(16, 8, PC_MODE_RGB, 72, 0, 0);
    PcLayer *l;
    CHECK(d != NULL);
    if (!d) return;
    CHECK_EQ(d->s.nlayers, 1);
    CHECK(d->s.layers[0]->is_background);
    CHECK_EQ(px_at(d, 0, 3, 3)[0], 255);
    l = pc_doc_add_layer(d, NULL);
    CHECK(l != NULL);
    CHECK(!strcmp(l->name, "Layer 1"));
    pc_hist_commit(d, "New Layer");
    /* Paint red into the new layer, commit, then undo: the buffer must not
     * be modified in place (shared with history). */
    CHECK_EQ(pc_fill_color(d, 0xFF0000, 100, PC_BLEND_NORMAL, 0), 0);
    pc_hist_commit(d, "Fill");
    CHECK_EQ(px_at(d, 1, 0, 0)[0], 255);
    CHECK_EQ(px_at(d, 1, 0, 0)[3], 255);
    CHECK_EQ(pc_hist_undo(d), 0);
    CHECK_EQ(d->s.nlayers, 2);
    CHECK_EQ(px_at(d, 1, 0, 0)[3], 0);
    CHECK_EQ(pc_hist_redo(d), 0);
    CHECK_EQ(px_at(d, 1, 0, 0)[0], 255);
    CHECK_EQ(pc_hist_goto(d, 0), 0);
    CHECK_EQ(d->s.nlayers, 1);
    CHECK_EQ(pc_hist_redo(d), 0);
    CHECK_EQ(d->s.nlayers, 2);
    /* New edit after undo truncates redo. */
    pc_hist_undo(d);
    pc_doc_add_layer(d, NULL);
    pc_hist_commit(d, "New Layer 2");
    CHECK(!pc_hist_can_redo(d));
    /* Snapshot survives trimming. */
    d->hist_limit = 3;
    pc_hist_snapshot(d, "Snapshot 1");
    {
        int k;
        for (k = 0; k < 10; k++) { pc_fill_color(d, 0x00FF00, 50, PC_BLEND_NORMAL, 0); pc_hist_commit(d, "Fill"); }
        CHECK(d->nhist <= 3 + 2);
        CHECK(!strcmp(d->hist[1].label, "Snapshot 1"));
    }
    pc_doc_free(d);
}

static void test_composite(void)
{
    PcDoc *d = pc_doc_new(4, 4, PC_MODE_RGB, 72, 1, 0x808080);
    PcLayer *l;
    PcBuf *c;
    if (!d) { CHECK(0); return; }
    l = pc_doc_add_layer(d, NULL);
    pc_fill_color(d, 0xFFFFFF, 100, PC_BLEND_NORMAL, 0);
    l->blend = PC_BLEND_MULTIPLY;
    c = pc_doc_composite(d);
    CHECK_EQ(c->px[0], 128);
    pc_buf_unref(c);
    l->blend = PC_BLEND_SCREEN;
    c = pc_doc_composite(d);
    CHECK_EQ(c->px[0], 255);
    pc_buf_unref(c);
    l->blend = PC_BLEND_NORMAL; l->opacity = 128;
    c = pc_doc_composite(d);
    CHECK_NEAR(c->px[0], 192, 1);
    CHECK_EQ(c->px[3], 255);
    pc_buf_unref(c);
    l->visible = 0;
    c = pc_doc_composite(d);
    CHECK_EQ(c->px[0], 128);
    pc_buf_unref(c);
    /* Every blend mode composites without crashing and stays in range. */
    {
        int b;
        l->visible = 1; l->opacity = 200;
        for (b = 0; b < PC_BLEND_COUNT; b++) {
            l->blend = b;
            c = pc_doc_composite(d);
            CHECK(c != NULL);
            pc_buf_unref(c);
        }
    }
    /* Flatten keeps the look. */
    l->blend = PC_BLEND_NORMAL; l->opacity = 255;
    CHECK_EQ(pc_doc_flatten(d), 0);
    CHECK_EQ(d->s.nlayers, 1);
    CHECK_EQ(px_at(d, 0, 1, 1)[0], 255);
    pc_doc_free(d);
}

static void test_adjust(void)
{
    PcDoc *d = pc_doc_new(8, 8, PC_MODE_RGB, 72, 1, 0x406080);
    PcLevels lv;
    PcCurves cv;
    PcColorBalance cb;
    PcHueSat hs;
    u8 lut[256];
    if (!d) { CHECK(0); return; }
    pc_adj_invert(d);
    CHECK_EQ(px_at(d, 0, 0, 0)[0], 255 - 0x40);
    pc_adj_invert(d);
    pc_levels_default(&lv);
    lv.in_white[0] = 128;
    pc_adj_levels(d, &lv);
    CHECK_EQ(px_at(d, 0, 0, 0)[2], 255);       /* 0x80 -> 255 */
    CHECK_NEAR(px_at(d, 0, 0, 0)[0], 0x80, 1);   /* 0x40 -> ~0x80 */
    pc_curves_default(&cv);
    pc_curve_lut(&cv.c[0], lut);
    CHECK_EQ(lut[100], 100);
    cv.c[0].n = 3; cv.c[0].x[1] = 128; cv.c[0].y[1] = 160;
    cv.c[0].x[2] = 255; cv.c[0].y[2] = 255;
    pc_curve_lut(&cv.c[0], lut);
    CHECK_EQ(lut[128], 160);
    CHECK(lut[64] > 64);
    memset(&cb, 0, sizeof cb);
    pc_adj_color_balance(d, &cb);   /* identity */
    CHECK_NEAR(px_at(d, 0, 0, 0)[0], 0x80, 1);
    memset(&hs, 0, sizeof hs);
    hs.sat[0] = -100;
    pc_adj_hue_sat(d, &hs);
    CHECK(px_at(d, 0, 0, 0)[0] == px_at(d, 0, 0, 0)[1] && px_at(d, 0, 0, 0)[1] == px_at(d, 0, 0, 0)[2]);
    pc_adj_brightness_contrast(d, 100, 0);
    pc_adj_threshold(d, 128);
    CHECK(px_at(d, 0, 0, 0)[0] == 0 || px_at(d, 0, 0, 0)[0] == 255);
    pc_adj_posterize(d, 4);
    pc_adj_equalize(d);
    pc_adj_auto_levels(d);
    pc_doc_free(d);
}

static void test_selection_and_fill(void)
{
    PcDoc *d = pc_doc_new(20, 20, PC_MODE_RGB, 72, 0, 0);
    PcRect r;
    float tri[6] = { 2, 2, 18, 2, 10, 18 };
    if (!d) { CHECK(0); return; }
    CHECK(!pc_sel_has(d));
    pc_sel_rect(d, 5, 5, 10, 10, PC_SEL_REPLACE, 0);
    CHECK(pc_sel_bounds(d, &r));
    CHECK(r.x0 == 5 && r.y0 == 5 && r.x1 == 10 && r.y1 == 10);
    pc_fill_color(d, 0x0000FF, 100, PC_BLEND_NORMAL, 0);
    CHECK_EQ(px_at(d, 0, 6, 6)[2], 255);
    CHECK_EQ(px_at(d, 0, 6, 6)[0], 0);
    CHECK_EQ(px_at(d, 0, 2, 2)[0], 255);
    pc_sel_rect(d, 8, 8, 12, 12, PC_SEL_SUBTRACT, 0);
    CHECK(pc_sel_bounds(d, &r));
    CHECK(r.x1 == 10);
    pc_sel_inverse(d);
    CHECK(pc_sel_at(d, 0, 0) == 255);
    pc_sel_clear(d);
    pc_sel_ellipse(d, 0, 0, 20, 20, PC_SEL_REPLACE, 1, 0);
    CHECK(pc_sel_at(d, 10, 10) == 255);
    CHECK(pc_sel_at(d, 0, 0) == 0);
    pc_sel_feather(d, 2);
    pc_sel_polygon(d, tri, 3, PC_SEL_REPLACE, 1, 0);
    CHECK(pc_sel_at(d, 10, 6) == 255);
    CHECK(pc_sel_at(d, 1, 17) == 0);
    pc_sel_clear(d);
    /* Magic wand on the blue square selects exactly it. */
    pc_sel_magic_wand(d, 6, 6, 10, 1, 0, 0, PC_SEL_REPLACE);
    CHECK(pc_sel_bounds(d, &r));
    CHECK(r.x0 == 5 && r.y1 == 10);
    pc_sel_modify(d, 2, 1);
    CHECK(pc_sel_bounds(d, &r) && r.x0 == 4);
    pc_sel_save_channel(d, NULL);
    CHECK_EQ(d->s.nchannels, 1);
    pc_sel_clear(d);
    pc_sel_load_channel(d, 0, 0, PC_SEL_REPLACE);
    CHECK(pc_sel_has(d));
    pc_sel_clear(d);
    /* Bucket fill the white area. */
    pc_bucket_fill(d, 0, 0, 0x00FF00, 0, 1, 0, 0, 0, 100, PC_BLEND_NORMAL);
    CHECK_EQ(px_at(d, 0, 0, 0)[1], 255);
    CHECK_EQ(px_at(d, 0, 0, 0)[0], 0);
    CHECK_EQ(px_at(d, 0, 6, 6)[2], 255);
    /* Gradient black -> white left to right. */
    {
        PcGradient g;
        memset(&g, 0, sizeof g);
        g.type = PC_GRAD_LINEAR; g.fg = 0; g.bg = 0xFFFFFF; g.opacity = 100;
        pc_gradient_fill(d, &g, 0, 0, 20, 0);
        CHECK(px_at(d, 0, 1, 5)[0] < 30);
        CHECK(px_at(d, 0, 18, 5)[0] > 225);
        g.type = PC_GRAD_RADIAL; pc_gradient_fill(d, &g, 10, 10, 20, 10);
        g.type = PC_GRAD_ANGLE; pc_gradient_fill(d, &g, 10, 10, 20, 10);
        g.type = PC_GRAD_REFLECTED; pc_gradient_fill(d, &g, 10, 10, 20, 10);
        g.type = PC_GRAD_DIAMOND; pc_gradient_fill(d, &g, 10, 10, 20, 10);
    }
    pc_sel_rect(d, 2, 2, 18, 18, PC_SEL_REPLACE, 0);
    CHECK_EQ(pc_stroke_selection(d, 2, 1, 0xFF0000, 100, PC_BLEND_NORMAL), 0);
    pc_doc_free(d);
}

static void test_transforms(void)
{
    PcDoc *d = pc_doc_new(10, 6, PC_MODE_RGB, 72, 0, 0);
    PcRect r;
    if (!d) { CHECK(0); return; }
    pc_sel_rect(d, 0, 0, 1, 1, PC_SEL_REPLACE, 0);
    pc_fill_color(d, 0xFF0000, 100, PC_BLEND_NORMAL, 0);
    pc_sel_clear(d);
    CHECK_EQ(pc_image_rotate(d, 1), 0);
    CHECK(d->s.w == 6 && d->s.h == 10);
    CHECK_EQ(px_at(d, 0, 5, 0)[1], 0);     /* red moved to top-right */
    CHECK_EQ(pc_image_rotate(d, 3), 0);
    CHECK_EQ(px_at(d, 0, 0, 0)[1], 0);
    CHECK_EQ(pc_image_flip(d, 0), 0);
    CHECK_EQ(px_at(d, 0, 9, 0)[1], 0);
    CHECK_EQ(pc_image_resize(d, 20, 12, PC_RESAMPLE_BICUBIC, 0), 0);
    CHECK(d->s.w == 20 && d->s.h == 12);
    CHECK_EQ(pc_image_resize(d, 5, 3, PC_RESAMPLE_BILINEAR, 0), 0);
    CHECK_EQ(pc_canvas_resize(d, 9, 9, 4, 0x000000), 0);
    CHECK(d->s.w == 9 && d->s.h == 9);
    CHECK_EQ(px_at(d, 0, 0, 0)[0], 0);     /* new background area is black */
    r.x0 = 2; r.y0 = 3; r.x1 = 7; r.y1 = 6;
    CHECK_EQ(pc_image_crop(d, &r), 0);
    CHECK(d->s.w == 5 && d->s.h == 3);
    CHECK_EQ(pc_image_rotate_arbitrary(d, 30, 0xFFFFFF), 0);
    CHECK(d->s.w > 5);
    pc_doc_layer_from_background(d);
    CHECK_EQ(pc_layer_offset(d, 1, 1), 0);
    CHECK_EQ(pc_layer_flip(d, 1), 0);
    CHECK_EQ(pc_layer_rotate(d, 1), 0);
    CHECK_EQ(pc_doc_to_gray(d), 0);
    CHECK_EQ(d->s.mode, PC_MODE_GRAY);
    CHECK_EQ(pc_image_resize(d, 1, 1, PC_RESAMPLE_NEAREST, 0), 0);
    pc_doc_free(d);
}

static void test_filters(void)
{
    PcDoc *d = pc_doc_new(24, 18, PC_MODE_RGB, 72, 0, 0);
    PcFilterParams p;
    int f;
    if (!d) { CHECK(0); return; }
    pc_sel_rect(d, 4, 4, 12, 12, PC_SEL_REPLACE, 0);
    pc_fill_color(d, 0x2050A0, 100, PC_BLEND_NORMAL, 0);
    pc_sel_clear(d);
    for (f = 0; f < PC_FILTER_COUNT; f++) {
        pc_filter_defaults(f, &p);
        CHECK_EQ(pc_filter_apply(d, f, &p), 0);
    }
    /* Gaussian blur of a flat image is the identity. */
    {
        PcDoc *e = pc_doc_new(8, 8, PC_MODE_RGB, 72, 1, 0x336699);
        pc_filter_defaults(PC_FILTER_GAUSSIAN_BLUR, &p);
        p.radius = 3;
        pc_filter_apply(e, PC_FILTER_GAUSSIAN_BLUR, &p);
        CHECK_NEAR(px_at(e, 0, 4, 4)[0], 0x33, 1);
        CHECK_NEAR(px_at(e, 0, 0, 0)[2], 0x99, 1);
        pc_doc_free(e);
    }
    pc_doc_free(d);
}

static void test_paint(void)
{
    PcDoc *d = pc_doc_new(40, 40, PC_MODE_RGB, 72, 0, 0);
    PcPaintOpts o;
    PcStroke *s;
    PcRect r;
    int tool;
    if (!d) { CHECK(0); return; }
    memset(&o, 0, sizeof o);
    o.tool = PC_PAINT_BRUSH; o.brush.diameter = 9; o.brush.hardness = 100; o.brush.spacing = 25;
    o.brush.roundness = 100; o.opacity = 50; o.color = 0x000000; o.bg = 0xFFFFFF;
    s = pc_stroke_begin(d, &o, 5, 20);
    CHECK(s != NULL);
    pc_stroke_to(s, 35, 20, &r);
    pc_stroke_to(s, 5, 20, &r);    /* crossing itself must not exceed 50% */
    pc_stroke_end(s);
    CHECK_NEAR(px_at(d, 0, 20, 20)[0], 128, 2);
    CHECK_EQ(px_at(d, 0, 20, 5)[0], 255);
    pc_hist_commit(d, "Paintbrush");
    /* Every tool runs. */
    pc_clone_set_source(d, 10, 10);
    for (tool = PC_PAINT_BRUSH; tool <= PC_PAINT_SPONGE; tool++) {
        o.tool = tool; o.opacity = 60;
        s = pc_stroke_begin(d, &o, 10, 30);
        CHECK(s != NULL);
        pc_stroke_to(s, 30, 30, &r);
        pc_stroke_tick(s, &r);
        pc_stroke_end(s);
    }
    /* Pencil is aliased: only 0 or full coverage. */
    o.tool = PC_PAINT_PENCIL; o.opacity = 100; o.color = 0xFF0000;
    s = pc_stroke_begin(d, &o, 20, 2);
    pc_stroke_to(s, 20, 2, &r);
    pc_stroke_end(s);
    CHECK_EQ(px_at(d, 0, 20, 2)[0], 255);
    CHECK_EQ(px_at(d, 0, 20, 2)[1], 0);
    /* Eraser on a normal layer clears alpha. */
    pc_doc_add_layer(d, NULL);
    pc_fill_color(d, 0x00FF00, 100, PC_BLEND_NORMAL, 0);
    o.tool = PC_PAINT_ERASER; o.eraser_mode = 0; o.opacity = 100;
    s = pc_stroke_begin(d, &o, 20, 20);
    pc_stroke_to(s, 20, 20, &r);
    pc_stroke_end(s);
    CHECK_EQ(px_at(d, 1, 20, 20)[3], 0);
    CHECK_EQ(px_at(d, 1, 2, 2)[3], 255);
    pc_doc_free(d);
}

static void test_layers_ops(void)
{
    PcDoc *d = pc_doc_new(8, 8, PC_MODE_RGB, 72, 0, 0);
    PcClip c;
    if (!d) { CHECK(0); return; }
    pc_doc_add_layer(d, NULL);
    pc_fill_color(d, 0xFF0000, 100, PC_BLEND_NORMAL, 0);
    CHECK_EQ(pc_doc_duplicate_layer(d, 1), 0);
    CHECK_EQ(d->s.nlayers, 3);
    CHECK(!strcmp(d->s.layers[2]->name, "Layer 1 copy"));
    CHECK_EQ(pc_doc_move_layer(d, 2, 1), 0);
    CHECK_EQ(pc_doc_move_layer(d, 1, 0), -1);   /* cannot go under Background */
    CHECK_EQ(pc_doc_add_mask(d, 1, 0, 0), 0);
    CHECK(d->s.layers[1]->mask != NULL);
    CHECK_EQ(pc_doc_apply_mask(d, 1, 1), 0);
    CHECK_EQ(px_at(d, 1, 0, 0)[3], 0);
    CHECK_EQ(pc_doc_merge_down(d, 2), 0);
    CHECK_EQ(d->s.nlayers, 2);
    pc_sel_rect(d, 0, 0, 4, 4, PC_SEL_REPLACE, 0);
    CHECK_EQ(pc_copy(d, 0, &c), 0);
    CHECK(c.w == 4 && c.h == 4);
    CHECK_EQ(pc_paste(d, &c, 0), 0);
    CHECK_EQ(d->s.nlayers, 3);
    pc_clip_free(&c);
    CHECK_EQ(pc_doc_merge_visible(d), 0);
    CHECK_EQ(pc_doc_delete_layer(d, 0), d->s.nlayers > 1 ? 0 : -1);
    pc_doc_free(d);
}

int test_core(void)
{
    int before = g_test_failures;
    test_fmt();
    test_doc_history();
    test_composite();
    test_adjust();
    test_selection_and_fill();
    test_transforms();
    test_filters();
    test_paint();
    test_layers_ops();
    return g_test_failures - before;
}
