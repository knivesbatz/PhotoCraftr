/*
 * Platform services on Win32 (Windows 95 API set): files, clipboard, GDI
 * text rendering for type layers, printing, launching the updater.
 */
#include "app.h"

int plat_read_file(const char *path, u8 **data, size_t *n, char *err, size_t errn)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, hi = 0, got = 0;
    u8 *buf;
    if (f == INVALID_HANDLE_VALUE) { pc_snprintf(err, errn, "the file could not be found or opened."); return -1; }
    size = GetFileSize(f, &hi);
    if (hi || size > 512u * 1024 * 1024) { CloseHandle(f); pc_snprintf(err, errn, "the file is too large."); return -1; }
    buf = (u8 *)malloc(size ? size : 1);
    if (!buf) { CloseHandle(f); pc_snprintf(err, errn, "there is not enough memory."); return -1; }
    if (size && (!ReadFile(f, buf, size, &got, NULL) || got != size)) {
        free(buf); CloseHandle(f);
        pc_snprintf(err, errn, "of a disk read error.");
        return -1;
    }
    CloseHandle(f);
    *data = buf; *n = size;
    return 0;
}

int plat_write_file(const char *path, const u8 *data, size_t n, char *err, size_t errn)
{
    /* Write to a temporary file next to the target, then replace it, so a
     * full disk never destroys the previous version of the document. */
    char tmp[280];
    HANDLE f;
    DWORD put = 0;
    pc_snprintf(tmp, sizeof tmp, "%s.pctmp", path);
    f = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { pc_snprintf(err, errn, "the file could not be created (is the disk write-protected?)."); return -1; }
    if (!WriteFile(f, data, (DWORD)n, &put, NULL) || put != n) {
        CloseHandle(f); DeleteFileA(tmp);
        pc_snprintf(err, errn, "the disk is full.");
        return -1;
    }
    CloseHandle(f);
    /* MoveFileEx is NT-only; on Windows 95 delete then rename. */
    if (plat_file_exists(path) && !DeleteFileA(path)) { DeleteFileA(tmp); pc_snprintf(err, errn, "the existing file is read-only or in use."); return -1; }
    if (!MoveFileA(tmp, path)) { pc_snprintf(err, errn, "the file could not be renamed."); return -1; }
    return 0;
}

int plat_file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != 0xFFFFFFFF && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* ---------------- clipboard (CF_DIB, works with every 1998 app) ---------------- */

void plat_clipboard_put(const PcClip *c)
{
    HGLOBAL h;
    BITMAPINFOHEADER *bi;
    u8 *bits;
    int x, y, stride;
    if (!c->rgba || !OpenClipboard(g_app.main)) return;
    EmptyClipboard();
    stride = (c->w * 3 + 3) & ~3;
    h = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + (size_t)stride * c->h);
    if (h) {
        bi = (BITMAPINFOHEADER *)GlobalLock(h);
        memset(bi, 0, sizeof *bi);
        bi->biSize = sizeof *bi; bi->biWidth = c->w; bi->biHeight = c->h; bi->biPlanes = 1; bi->biBitCount = 24;
        bits = (u8 *)(bi + 1);
        for (y = 0; y < c->h; y++) {
            u8 *row = bits + (size_t)(c->h - 1 - y) * stride;
            for (x = 0; x < c->w; x++) {
                const u8 *p = c->rgba + ((size_t)y * c->w + x) * 4;
                int a = p[3];
                row[x * 3] = (u8)((p[2] * a + 255 * (255 - a)) / 255);
                row[x * 3 + 1] = (u8)((p[1] * a + 255 * (255 - a)) / 255);
                row[x * 3 + 2] = (u8)((p[0] * a + 255 * (255 - a)) / 255);
            }
        }
        GlobalUnlock(h);
        SetClipboardData(CF_DIB, h);
    }
    CloseClipboard();
}

int plat_clipboard_get(PcClip *c)
{
    HGLOBAL h;
    BITMAPINFOHEADER *bi;
    int ok = -1;
    if (!IsClipboardFormatAvailable(CF_DIB) || !OpenClipboard(g_app.main)) return -1;
    h = GetClipboardData(CF_DIB);
    if (h && (bi = (BITMAPINFOHEADER *)GlobalLock(h)) != NULL) {
        int w = bi->biWidth, hh = bi->biHeight < 0 ? -bi->biHeight : bi->biHeight, bpp = bi->biBitCount;
        int top_down = bi->biHeight < 0, x, y;
        DWORD size = (DWORD)GlobalSize(h);
        int ncol = bpp <= 8 ? (bi->biClrUsed ? (int)bi->biClrUsed : 1 << bpp) : 0;
        int masks = (bi->biCompression == BI_BITFIELDS) ? 12 : 0;
        const u8 *pal = (const u8 *)bi + bi->biSize;
        const u8 *bits = pal + masks + ncol * 4;
        int stride = ((w * bpp + 31) / 32) * 4;
        if (w > 0 && hh > 0 && w <= PC_MAX_DIM && hh <= PC_MAX_DIM && (bpp == 8 || bpp == 24 || bpp == 32) &&
            (bi->biCompression == BI_RGB || bi->biCompression == BI_BITFIELDS) &&
            (size_t)(bits - (const u8 *)bi) + (size_t)stride * hh <= size) {
            c->rgba = (u8 *)pc_alloc_checked((size_t)w, (size_t)hh, 4, 0);
            if (c->rgba) {
                c->w = w; c->h = hh; c->x = -1; c->y = -1;
                for (y = 0; y < hh; y++) {
                    const u8 *row = bits + (size_t)(top_down ? y : hh - 1 - y) * stride;
                    for (x = 0; x < w; x++) {
                        u8 *o = c->rgba + ((size_t)y * w + x) * 4;
                        if (bpp == 8) { const u8 *q = pal + masks + row[x] * 4; o[0] = q[2]; o[1] = q[1]; o[2] = q[0]; }
                        else { const u8 *q = row + x * (bpp / 8); o[0] = q[2]; o[1] = q[1]; o[2] = q[0]; }
                        o[3] = 255;
                    }
                }
                ok = 0;
            }
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    return ok;
}

/* ---------------- type rendering (GDI) ----------------
 * Windows 95 GDI cannot anti-alias text, so smooth type is rendered at 4x
 * into a monochrome-ish DIB and box-filtered down - the same trick 1998
 * graphics programs used. */

int plat_text_render(const PcText *t, PcBuf *rgba, void *user)
{
    int ss = t->antialias ? 4 : 1;
    int W = rgba->w, H = rgba->h;
    HDC sdc = GetDC(NULL), dc = CreateCompatibleDC(sdc);
    HFONT font, old;
    HBITMAP bmp, oldb;
    BITMAPINFO bi;
    u8 *bits = NULL;
    TEXTMETRICA tm;
    int lines = 1, maxw = 0, lh, x, y, bw, bh;
    const char *s;
    (void)user;
    ReleaseDC(NULL, sdc);
    font = CreateFontA(-t->size * ss, 0, 0, 0, t->bold ? FW_BOLD : FW_NORMAL, (BYTE)t->italic, (BYTE)t->underline, 0,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, t->font);
    if (!font) { DeleteDC(dc); return -1; }
    old = (HFONT)SelectObject(dc, font);
    GetTextMetricsA(dc, &tm);
    lh = t->leading > 0 ? t->leading * ss : tm.tmHeight;
    for (s = t->text; *s; s++) if (*s == '\n') lines++;
    /* Measure. */
    {
        const char *a = t->text;
        while (1) {
            const char *e = strchr(a, '\n');
            int len = e ? (int)(e - a) : (int)strlen(a);
            SIZE sz;
            if (len > 0 && a[len - 1] == '\r') len--;
            GetTextExtentPoint32A(dc, a, len, &sz);
            sz.cx += t->tracking * ss * len;
            if (sz.cx > maxw) maxw = sz.cx;
            if (!e) break;
            a = e + 1;
        }
    }
    if (t->vertical) { bw = tm.tmMaxCharWidth + 4 * ss; bh = (int)strlen(t->text) * lh + 4 * ss; }
    else { bw = maxw + 4 * ss + tm.tmOverhang; bh = lines * lh + 4 * ss; }
    bw = PC_MIN(bw, W * ss + 64); bh = PC_MIN(bh, H * ss + 64);
    if (bw <= 0 || bh <= 0) { SelectObject(dc, old); DeleteObject(font); DeleteDC(dc); return 0; }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = bw; bi.bmiHeader.biHeight = -bh;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!bmp || !bits) { SelectObject(dc, old); DeleteObject(font); DeleteDC(dc); return -1; }
    oldb = (HBITMAP)SelectObject(dc, bmp);
    memset(bits, 0, (size_t)bw * bh * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    if (t->tracking) SetTextCharacterExtra(dc, t->tracking * ss);
    if (t->vertical) {
        /* Vertical type: characters stacked top to bottom, centred. */
        int yy = 2 * ss;
        for (s = t->text; *s; s++) {
            SIZE sz;
            if (*s == '\n' || *s == '\r') continue;
            GetTextExtentPoint32A(dc, s, 1, &sz);
            TextOutA(dc, (bw - sz.cx) / 2, yy, s, 1);
            yy += lh;
        }
    } else {
        const char *a = t->text;
        int ln = 0;
        while (1) {
            const char *e = strchr(a, '\n');
            int len = e ? (int)(e - a) : (int)strlen(a), xx = 2 * ss;
            SIZE sz;
            if (len > 0 && a[len - 1] == '\r') len--;
            GetTextExtentPoint32A(dc, a, len, &sz);
            if (t->align == 1) xx = (bw - sz.cx) / 2;
            else if (t->align == 2) xx = bw - sz.cx - 2 * ss;
            TextOutA(dc, xx, 2 * ss + ln * lh, a, len);
            ln++;
            if (!e) break;
            a = e + 1;
        }
    }
    GdiFlush();
    /* Downsample coverage and composite the colour into the layer buffer. */
    {
        int ox = t->x - 2, oy = t->y - 2;
        int r = pc_r(t->color), g = pc_g(t->color), b = pc_b(t->color);
        if (t->align == 1) ox = t->x - bw / ss / 2;
        else if (t->align == 2) ox = t->x - bw / ss;
        for (y = 0; y < bh / ss; y++) {
            int dy = oy + y;
            if (dy < 0 || dy >= H) continue;
            for (x = 0; x < bw / ss; x++) {
                int dx = ox + x, cov = 0, sx, sy, a;
                u8 *o;
                if (dx < 0 || dx >= W) continue;
                for (sy = 0; sy < ss; sy++)
                    for (sx = 0; sx < ss; sx++) cov += bits[((size_t)(y * ss + sy) * bw + (x * ss + sx)) * 4 + 1];
                a = cov / (ss * ss);
                if (t->antialias == 1 && a) a = a > 96 ? PC_MIN(255, a * 3 / 2) : a / 2;    /* crisp */
                else if (t->antialias == 2 && a) a = PC_MIN(255, a * 5 / 4);                  /* strong */
                if (!a) continue;
                o = rgba->px + ((size_t)dy * W + dx) * 4;
                o[0] = (u8)r; o[1] = (u8)g; o[2] = (u8)b;
                o[3] = (u8)PC_MAX(o[3], a);
            }
        }
    }
    SelectObject(dc, oldb);
    DeleteObject(bmp);
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    return 0;
}

/* ---------------- printing ---------------- */

int plat_print(DocView *v, HWND owner)
{
    /* Uses the printer chosen in Page Setup (registry), else the default
     * printer from WIN.INI - the Windows 95 way, no common dialog. */
    char dev[256], *drv, *port;
    char printer[128];
    HDC pdc;
    DOCINFOA di;
    PcBuf *c;
    BITMAPINFO bi;
    int pw, ph, dpi_x, dpi_y, w, h, x0, y0, rc = -1;
    (void)owner;
    reg_get_str("Print", "Printer", printer, sizeof printer);
    if (printer[0]) {
        char key[160];
        pc_snprintf(key, sizeof key, "%s", printer);
        GetProfileStringA("Devices", key, "", dev, sizeof dev);
        if (!dev[0]) printer[0] = 0;
        else {
            char tmp[256];
            pc_snprintf(tmp, sizeof tmp, "%s,%s", printer, dev);
            pc_strlcpy(dev, tmp, sizeof dev);
        }
    }
    if (!printer[0]) GetProfileStringA("windows", "device", "", dev, sizeof dev);
    if (!dev[0]) { app_error("No printer is installed. Use the Printers folder in Control Panel to add one."); return -1; }
    /* dev = "Printer Name,driver,port" */
    drv = strchr(dev, ',');
    if (!drv) return -1;
    *drv++ = 0;
    port = strchr(drv, ',');
    if (port) *port++ = 0;
    pdc = CreateDCA(drv, dev, port, NULL);
    if (!pdc) { app_error("Could not open the printer \"%s\".", dev); return -1; }
    c = pc_doc_composite(v->doc);
    if (!c) { DeleteDC(pdc); return -1; }
    pw = GetDeviceCaps(pdc, HORZRES); ph = GetDeviceCaps(pdc, VERTRES);
    dpi_x = GetDeviceCaps(pdc, LOGPIXELSX); dpi_y = GetDeviceCaps(pdc, LOGPIXELSY);
    /* Print at the document resolution, scaled down to fit the page. */
    w = (int)(c->w * dpi_x / v->doc->s.dpi); h = (int)(c->h * dpi_y / v->doc->s.dpi);
    if (w > pw || h > ph) {
        double s = PC_MIN((double)pw / w, (double)ph / h);
        w = (int)(w * s); h = (int)(h * s);
    }
    x0 = (pw - w) / 2; y0 = (ph - h) / 2;
    if (reg_get_int("Print", "Center", 1) == 0) { x0 = 0; y0 = 0; }
    {
        size_t i, n = (size_t)c->w * c->h;
        u8 *bgrx = (u8 *)malloc(n * 4);
        if (bgrx) {
            for (i = 0; i < n; i++) {
                const u8 *p = c->px + i * 4;
                int a = p[3];
                bgrx[i * 4] = (u8)((p[2] * a + 255 * (255 - a)) / 255);
                bgrx[i * 4 + 1] = (u8)((p[1] * a + 255 * (255 - a)) / 255);
                bgrx[i * 4 + 2] = (u8)((p[0] * a + 255 * (255 - a)) / 255);
                bgrx[i * 4 + 3] = 0;
            }
            memset(&di, 0, sizeof di);
            di.cbSize = sizeof di;
            di.lpszDocName = v->doc->title;
            if (StartDocA(pdc, &di) > 0 && StartPage(pdc) > 0) {
                memset(&bi, 0, sizeof bi);
                bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = c->w; bi.bmiHeader.biHeight = -c->h;
                bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
                SetStretchBltMode(pdc, COLORONCOLOR);
                StretchDIBits(pdc, x0, y0, w, h, 0, 0, c->w, c->h, bgrx, &bi, DIB_RGB_COLORS, SRCCOPY);
                EndPage(pdc);
                EndDoc(pdc);
                rc = 0;
            }
            free(bgrx);
        }
    }
    pc_buf_unref(c);
    DeleteDC(pdc);
    if (rc) app_error("Printing failed.");
    return rc;
}

/* ---------------- updater ---------------- */

void plat_launch_updater(const char *args)
{
    char exe[260], cmd[400];
    char *slash;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    GetModuleFileNameA(NULL, exe, sizeof exe);
    slash = strrchr(exe, '\\');
    if (!slash) return;
    pc_strlcpy(slash + 1, "PCUpdate.exe", sizeof exe - (size_t)(slash + 1 - exe));
    if (!plat_file_exists(exe)) {
        if (strstr(args, "--check")) app_error("The updater (PCUpdate.exe) is missing from the installation folder. Please reinstall PhotoCraft Classic.");
        return;
    }
    pc_snprintf(cmd, sizeof cmd, "\"%s\" %s", exe, args);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
