#include "pc_mupdf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#define PC_REFLOW_W 450.f
#define PC_REFLOW_H 600.f
#define PC_SHEET_W 1191.f
#define PC_SHEET_H 842.f

static int ascii_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static const char *path_basename(const char *path) {
    const char *base;
    if (!path || !path[0]) return path;
    base = strrchr(path, '/');
#ifdef _WIN32
    {
        const char *bs = strrchr(path, '\\');
        if (bs && (!base || bs > base)) base = bs;
    }
#endif
    return base ? base + 1 : path;
}

static int path_has_suffix(const char *path) {
    const char *base, *dot;
    base = path_basename(path);
    if (!base || !base[0]) return 0;
    dot = strrchr(base, '.');
    return dot && dot != base && dot[1] != '\0';
}

static int ends_with_ci(const char *s, const char *ext) {
    size_t n, e, i;
    if (!s || !ext) return 0;
    n = strlen(s);
    e = strlen(ext);
    if (n < e) return 0;
    for (i = 0; i < e; i++) {
        if (ascii_tolower((unsigned char)s[n - e + i]) != ascii_tolower((unsigned char)ext[i]))
            return 0;
    }
    return 1;
}

static const char *open_magic(const char *path, const char *magic) {
    if (path_has_suffix(path)) return path;
    if (magic && magic[0]) return magic;
    return path ? path : "";
}

static int is_spreadsheet(const char *path, const char *magic) {
    if (ends_with_ci(path, ".xlsx")) return 1;
    if (magic && strstr(magic, "spreadsheetml.sheet")) return 1;
    if (ends_with_ci(magic, ".xlsx")) return 1;
    return 0;
}

static void reflow_size(const char *path, const char *magic, float *w, float *h) {
    if (is_spreadsheet(path, magic)) {
        *w = PC_SHEET_W;
        *h = PC_SHEET_H;
    } else {
        *w = PC_REFLOW_W;
        *h = PC_REFLOW_H;
    }
}

struct pc_engine {
    fz_context *ctx;
    fz_document *doc;
    char err[512];
};

static char *pc_strdup(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static void set_err(pc_engine *e, const char *msg) {
    if (!e) return;
    snprintf(e->err, sizeof e->err, "%s", msg ? msg : "error");
}

pc_engine *pc_new(void) {
    pc_engine *e = (pc_engine *)calloc(1, sizeof *e);
    if (!e) return NULL;
    e->ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!e->ctx) {
        free(e);
        return NULL;
    }
    fz_try(e->ctx) {
        fz_register_document_handlers(e->ctx);
    }
    fz_catch(e->ctx) {
        fz_drop_context(e->ctx);
        free(e);
        return NULL;
    }
    return e;
}

void pc_close(pc_engine *e) {
    if (!e || !e->doc) return;
    fz_drop_document(e->ctx, e->doc);
    e->doc = NULL;
}

void pc_free(pc_engine *e) {
    if (!e) return;
    pc_close(e);
    fz_drop_context(e->ctx);
    free(e);
}

const char *pc_last_error(pc_engine *e) {
    return e ? e->err : "";
}

int pc_is_open(pc_engine *e) {
    return e && e->doc != NULL;
}

int pc_open(pc_engine *e, const char *path, const char *magic) {
    fz_context *ctx = e->ctx;
    fz_stream *stream = NULL;
    fz_document *doc = NULL;
    int needs = 0;
    int failed = 0;
    pc_close(e);
    fz_var(stream);
    fz_var(doc);
    fz_try(ctx) {
        stream = fz_open_file(ctx, path);
        doc = fz_open_document_with_stream(ctx, open_magic(path, magic), stream);
        needs = fz_needs_password(ctx, doc);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stream);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        failed = 1;
    }
    if (failed) {
        if (doc) fz_drop_document(ctx, doc);
        return PC_FAIL;
    }
    e->doc = doc;
    return needs ? PC_PASSWORD : PC_OK;
}

int pc_authenticate(pc_engine *e, const char *password) {
    fz_context *ctx = e->ctx;
    int ok = 0;
    if (!e->doc) return 0;
    fz_try(ctx) {
        ok = fz_authenticate_password(ctx, e->doc, password);
    }
    fz_catch(ctx) {
        ok = 0;
    }
    return ok;
}

int pc_is_reflowable(pc_engine *e) {
    fz_context *ctx = e->ctx;
    int r = 0;
    if (!e->doc) return 0;
    fz_try(ctx) {
        r = fz_is_document_reflowable(ctx, e->doc);
    }
    fz_catch(ctx) {
        r = 0;
    }
    return r;
}

void pc_set_user_css(pc_engine *e, const char *css) {
    if (!e || !e->ctx) return;
    fz_try(e->ctx) {
        fz_set_user_css(e->ctx, css ? css : "");
    }
    fz_catch(e->ctx) {
        set_err(e, fz_caught_message(e->ctx));
    }
}

int pc_layout(pc_engine *e, float w, float h, float em) {
    fz_context *ctx = e->ctx;
    int ok = 1;
    if (!e->doc) return 0;
    fz_try(ctx) {
        fz_layout_document(ctx, e->doc, w, h, em);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        ok = 0;
    }
    return ok;
}

int pc_page_count(pc_engine *e) {
    fz_context *ctx = e->ctx;
    int n = 0;
    if (!e->doc) return 0;
    fz_try(ctx) {
        n = fz_count_pages(ctx, e->doc);
    }
    fz_catch(ctx) {
        n = 0;
    }
    return n;
}

int pc_page_bounds(pc_engine *e, int index, float out[4]) {
    fz_context *ctx = e->ctx;
    fz_page *page = NULL;
    fz_rect b;
    int ok = 1;
    fz_var(page);
    if (!e->doc) return 0;
    fz_try(ctx) {
        page = fz_load_page(ctx, e->doc, index);
        b = fz_bound_page(ctx, page);
        out[0] = b.x0;
        out[1] = b.y0;
        out[2] = b.x1;
        out[3] = b.y1;
    }
    fz_always(ctx) {
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        ok = 0;
    }
    return ok;
}

static unsigned over_white(unsigned c, unsigned a) {
    return (c * a + 255u * (255u - a)) / 255u;
}

int pc_render(pc_engine *e, int index, float scale, float e_off, float f_off, uint32_t **pixels, int *w, int *h,
              int extra_cw) {
    fz_context *ctx = e->ctx;
    fz_page *page = NULL;
    fz_pixmap *pix = NULL;
    uint32_t *out = NULL;
    int ok = 1;
    fz_var(page);
    fz_var(pix);
    fz_var(out);
    if (!e->doc) return 0;
    fz_try(ctx) {
        fz_matrix ctm = fz_scale(scale, scale);
        int pw, ph, stride, n, x, y;
        const unsigned char *samples;
        extra_cw %= 360;
        if (extra_cw < 0) extra_cw += 360;
        page = fz_load_page(ctx, e->doc, index);
        if (extra_cw) {
            fz_rect b;
            ctm = fz_pre_rotate(ctm, -(float)extra_cw);
            b = fz_transform_rect(fz_bound_page(ctx, page), ctm);
            ctm.e -= b.x0;
            ctm.f -= b.y0;
        } else {
            ctm.e = e_off;
            ctm.f = f_off;
        }
        pix = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 1);
        pw = fz_pixmap_width(ctx, pix);
        ph = fz_pixmap_height(ctx, pix);
        if (pw < 1) pw = 1;
        if (ph < 1) ph = 1;
        stride = fz_pixmap_stride(ctx, pix);
        n = fz_pixmap_components(ctx, pix);
        samples = fz_pixmap_samples(ctx, pix);
        out = (uint32_t *)malloc((size_t)pw * (size_t)ph * sizeof(uint32_t));
        if (!out) fz_throw(ctx, FZ_ERROR_SYSTEM, "out of memory");
        if (!samples) {
            for (x = 0; x < pw * ph; x++) out[x] = 0xFFFFFFFFu;
        } else if (n >= 4 && fz_pixmap_alpha(ctx, pix)) {
            for (y = 0; y < ph; y++) {
                const unsigned char *src = samples + (size_t)y * (size_t)stride;
                uint32_t *row = out + (size_t)y * (size_t)pw;
                for (x = 0; x < pw; x++, src += n) {
                    unsigned r = src[0], g = src[1], b = src[2], a = src[3];
                    if (a < 255u) {
                        r = over_white(r, a);
                        g = over_white(g, a);
                        b = over_white(b, a);
                    }
                    row[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }
        } else if (n >= 3) {
            for (y = 0; y < ph; y++) {
                const unsigned char *src = samples + (size_t)y * (size_t)stride;
                uint32_t *row = out + (size_t)y * (size_t)pw;
                for (x = 0; x < pw; x++, src += n)
                    row[x] = 0xFF000000u | ((unsigned)src[0] << 16) | ((unsigned)src[1] << 8) | src[2];
            }
        } else {
            int step = n < 1 ? 1 : n;
            for (y = 0; y < ph; y++) {
                const unsigned char *src = samples + (size_t)y * (size_t)stride;
                uint32_t *row = out + (size_t)y * (size_t)pw;
                for (x = 0; x < pw; x++, src += step)
                    row[x] = 0xFF000000u | ((unsigned)src[0] << 16) | ((unsigned)src[0] << 8) | src[0];
            }
        }
        *w = pw;
        *h = ph;
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        free(out);
        out = NULL;
        ok = 0;
    }
    *pixels = out;
    return ok;
}

void pc_free_buf(void *p) {
    free(p);
}

typedef struct {
    pc_char *chars;
    int nchars, cchars;
    pc_rect *images;
    int nimages, cimages;
} walk_state;

static int walk_blocks(fz_stext_block *b, walk_state *s) {
    for (; b; b = b->next) {
        if (b->type == FZ_STEXT_BLOCK_IMAGE) {
            if (s->nimages == s->cimages) {
                int nc = s->cimages ? s->cimages * 2 : 8;
                pc_rect *p = (pc_rect *)realloc(s->images, (size_t)nc * sizeof *p);
                if (!p) return 0;
                s->images = p;
                s->cimages = nc;
            }
            s->images[s->nimages].r[0] = b->bbox.x0;
            s->images[s->nimages].r[1] = b->bbox.y0;
            s->images[s->nimages].r[2] = b->bbox.x1;
            s->images[s->nimages].r[3] = b->bbox.y1;
            s->nimages++;
        } else if (b->type == FZ_STEXT_BLOCK_TEXT) {
            fz_stext_line *line;
            for (line = b->u.t.first_line; line; line = line->next) {
                fz_stext_char *ch;
                for (ch = line->first_char; ch; ch = ch->next) {
                    pc_char *o;
                    if (s->nchars == s->cchars) {
                        int nc = s->cchars ? s->cchars * 2 : 1024;
                        pc_char *p = (pc_char *)realloc(s->chars, (size_t)nc * sizeof *p);
                        if (!p) return 0;
                        s->chars = p;
                        s->cchars = nc;
                    }
                    o = &s->chars[s->nchars++];
                    o->c = (unsigned)ch->c;
                    o->q[0] = ch->quad.ul.x;
                    o->q[1] = ch->quad.ul.y;
                    o->q[2] = ch->quad.ur.x;
                    o->q[3] = ch->quad.ur.y;
                    o->q[4] = ch->quad.ll.x;
                    o->q[5] = ch->quad.ll.y;
                    o->q[6] = ch->quad.lr.x;
                    o->q[7] = ch->quad.lr.y;
                }
            }
        } else if (b->type == FZ_STEXT_BLOCK_STRUCT) {
            if (b->u.s.down && !walk_blocks(b->u.s.down->first_block, s)) return 0;
        }
    }
    return 1;
}

static float fminf4(float a, float b) { return a < b ? a : b; }
static float fmaxf4(float a, float b) { return a > b ? a : b; }

int pc_content_load(pc_engine *e, int index, pc_content *out) {
    fz_context *ctx = e->ctx;
    fz_page *page = NULL;
    fz_stext_page *st = NULL;
    fz_link *links = NULL, *l;
    walk_state s;
    int ok = 1;
    int nl = 0, cl = 0;
    pc_link *larr = NULL;
    fz_var(page);
    fz_var(st);
    fz_var(links);
    memset(out, 0, sizeof *out);
    memset(&s, 0, sizeof s);
    if (!e->doc) return 0;
    fz_try(ctx) {
        page = fz_load_page(ctx, e->doc, index);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        return 0;
    }
    fz_try(ctx) {
        links = fz_load_links(ctx, page);
        for (l = links; l; l = l->next) {
            pc_link *dst;
            if (!l->uri) continue;
            if (nl == cl) {
                int nc = cl ? cl * 2 : 8;
                pc_link *p = (pc_link *)realloc(larr, (size_t)nc * sizeof *p);
                if (!p) fz_throw(ctx, FZ_ERROR_SYSTEM, "out of memory");
                larr = p;
                cl = nc;
            }
            dst = &larr[nl++];
            dst->r[0] = fminf4(l->rect.x0, l->rect.x1);
            dst->r[1] = fminf4(l->rect.y0, l->rect.y1);
            dst->r[2] = fmaxf4(l->rect.x0, l->rect.x1);
            dst->r[3] = fmaxf4(l->rect.y0, l->rect.y1);
            dst->uri = pc_strdup(l->uri);
            dst->external = fz_is_external_link(ctx, l->uri);
        }
    }
    fz_always(ctx) {
        fz_drop_link(ctx, links);
    }
    fz_catch(ctx) {
    }
    fz_try(ctx) {
        fz_stext_options opts;
        fz_parse_stext_options(ctx, &opts, "preserve-images,preserve-whitespace");
        st = fz_new_stext_page_from_page(ctx, page, &opts);
        if (!walk_blocks(st->first_block, &s)) fz_throw(ctx, FZ_ERROR_SYSTEM, "out of memory");
    }
    fz_always(ctx) {
        fz_drop_stext_page(ctx, st);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        set_err(e, fz_caught_message(ctx));
        ok = 0;
    }
    if (!ok) {
        int i;
        free(s.chars);
        free(s.images);
        for (i = 0; i < nl; i++) free(larr[i].uri);
        free(larr);
        return 0;
    }
    out->chars = s.chars;
    out->nchars = s.nchars;
    out->images = s.images;
    out->nimages = s.nimages;
    out->links = larr;
    out->nlinks = nl;
    return 1;
}

void pc_content_free(pc_content *c) {
    int i;
    if (!c) return;
    for (i = 0; i < c->nlinks; i++) free(c->links[i].uri);
    free(c->chars);
    free(c->images);
    free(c->links);
    memset(c, 0, sizeof *c);
}

typedef struct {
    pc_outline_item *items;
    int n, cap;
} outline_state;

static int outline_walk(fz_context *ctx, fz_document *doc, fz_outline *o, int depth, outline_state *s) {
    for (; o; o = o->next) {
        pc_outline_item *it;
        fz_location loc;
        float y = 0;
        int has_y = 0;
        int page = -1;
        if (s->n == s->cap) {
            int nc = s->cap ? s->cap * 2 : 64;
            pc_outline_item *p = (pc_outline_item *)realloc(s->items, (size_t)nc * sizeof *p);
            if (!p) return 0;
            s->items = p;
            s->cap = nc;
        }
        loc = o->page;
        y = o->y;
        has_y = !isnan(y);
        if (o->uri && o->uri[0]) {
            fz_try(ctx) {
                fz_link_dest dest = fz_resolve_link_dest(ctx, doc, o->uri);
                loc = dest.loc;
                y = dest.y;
                has_y = !isnan(y);
            }
            fz_catch(ctx) {
            }
        }
        fz_try(ctx) {
            page = fz_page_number_from_location(ctx, doc, loc);
        }
        fz_catch(ctx) {
            page = -1;
        }
        it = &s->items[s->n++];
        it->title = pc_strdup(o->title ? o->title : "");
        it->uri = pc_strdup(o->uri);
        it->page = page;
        it->has_y = has_y;
        it->y = has_y ? y : 0.f;
        it->depth = depth;
        if (o->down && !outline_walk(ctx, doc, o->down, depth + 1, s)) return 0;
    }
    return 1;
}

int pc_outline(pc_engine *e, pc_outline_item **items, int *count) {
    fz_context *ctx = e->ctx;
    fz_outline *root = NULL;
    outline_state s;
    int ok = 1;
    fz_var(root);
    memset(&s, 0, sizeof s);
    *items = NULL;
    *count = 0;
    if (!e->doc) return 0;
    fz_try(ctx) {
        root = fz_load_outline(ctx, e->doc);
    }
    fz_catch(ctx) {
        return 1;
    }
    if (root && !outline_walk(ctx, e->doc, root, 0, &s)) ok = 0;
    fz_drop_outline(ctx, root);
    if (!ok) {
        pc_outline_free(s.items, s.n);
        return 0;
    }
    *items = s.items;
    *count = s.n;
    return 1;
}

void pc_outline_free(pc_outline_item *items, int count) {
    int i;
    for (i = 0; i < count; i++) {
        free(items[i].title);
        free(items[i].uri);
    }
    free(items);
}

int pc_is_external(pc_engine *e, const char *uri) {
    fz_context *ctx = e->ctx;
    int r = 0;
    fz_try(ctx) {
        r = fz_is_external_link(ctx, uri);
    }
    fz_catch(ctx) {
        r = 0;
    }
    return r;
}

int pc_resolve_uri(pc_engine *e, const char *uri, int *page, int *has_y, float *y) {
    fz_context *ctx = e->ctx;
    int ok = 0;
    if (!e->doc) return 0;
    fz_try(ctx) {
        fz_link_dest dest = fz_resolve_link_dest(ctx, e->doc, uri);
        int p = fz_page_number_from_location(ctx, e->doc, dest.loc);
        if (p >= 0) {
            *page = p;
            *has_y = !isnan(dest.y);
            *y = *has_y ? dest.y : 0.f;
            ok = 1;
        }
    }
    fz_catch(ctx) {
        ok = 0;
    }
    return ok;
}

static const char *kTracingRootKeys[] = {"Metadata", "Outlines", "OpenAction", "AA", "PieceInfo", "Perms", "Lang"};

static void delete_key(fz_context *ctx, pdf_obj *obj, const char *key) {
    fz_try(ctx) {
        pdf_dict_dels(ctx, obj, key);
    }
    fz_catch(ctx) {
    }
}

static void strip_tracing(fz_context *ctx, pdf_document *doc) {
    fz_try(ctx) {
        pdf_obj *trailer = pdf_trailer(ctx, doc);
        pdf_obj *root;
        size_t i;
        delete_key(ctx, trailer, "Info");
        delete_key(ctx, trailer, "ID");
        root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
        if (root && !pdf_is_null(ctx, root)) {
            for (i = 0; i < sizeof kTracingRootKeys / sizeof kTracingRootKeys[0]; i++)
                delete_key(ctx, root, kTracingRootKeys[i]);
        }
    }
    fz_catch(ctx) {
    }
}

static int open_source(fz_context *ctx, const char *path, const char *magic, float em, const char *password,
                       fz_document **out) {
    fz_stream *stream = NULL;
    fz_document *doc = NULL;
    int code = PC_OK;
    fz_var(stream);
    fz_var(doc);
    *out = NULL;
    fz_try(ctx) {
        stream = fz_open_file(ctx, path);
        doc = fz_open_document_with_stream(ctx, open_magic(path, magic), stream);
        if (fz_needs_password(ctx, doc)) {
            if (!password || !fz_authenticate_password(ctx, doc, password)) code = PC_PASSWORD;
        }
        if (code == PC_OK && fz_is_document_reflowable(ctx, doc)) {
            float w, h;
            reflow_size(path, magic, &w, &h);
            fz_layout_document(ctx, doc, w, h, em);
        }
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stream);
    }
    fz_catch(ctx) {
        code = PC_FAIL;
    }
    if (code != PC_OK) {
        if (doc) fz_drop_document(ctx, doc);
        return code;
    }
    *out = doc;
    return PC_OK;
}

static int graft_all(fz_context *ctx, pdf_document *dst, pdf_document *src) {
    int count = 0, i;
    fz_try(ctx) {
        count = pdf_count_pages(ctx, src);
        if (count <= 0) fz_throw(ctx, FZ_ERROR_ARGUMENT, "EXPORT");
        for (i = 0; i < count; i++)
            pdf_graft_page(ctx, dst, pdf_count_pages(ctx, dst), src, i);
    }
    fz_catch(ctx) {
        return 0;
    }
    return 1;
}

static int write_as_pdf(fz_context *ctx, fz_document *src, const char *dest) {
    fz_document_writer *writer = NULL;
    int ok = 1;
    fz_var(writer);
    fz_try(ctx) {
        int count = fz_count_pages(ctx, src), i;
        if (count <= 0) fz_throw(ctx, FZ_ERROR_ARGUMENT, "EXPORT");
        remove(dest);
        writer = fz_new_document_writer(ctx, dest, "pdf", "compress");
        for (i = 0; i < count; i++) {
            fz_page *page = NULL;
            fz_var(page);
            fz_try(ctx) {
                fz_rect b;
                fz_device *dev;
                page = fz_load_page(ctx, src, i);
                b = fz_bound_page(ctx, page);
                dev = fz_begin_page(ctx, writer, b);
                fz_run_page(ctx, page, dev, fz_identity, NULL);
                fz_end_page(ctx, writer);
            }
            fz_always(ctx) {
                fz_drop_page(ctx, page);
            }
            fz_catch(ctx) {
                fz_rethrow(ctx);
            }
        }
        fz_close_document_writer(ctx, writer);
    }
    fz_always(ctx) {
        fz_drop_document_writer(ctx, writer);
    }
    fz_catch(ctx) {
        ok = 0;
    }
    return ok;
}

static void apply_rotates(fz_context *ctx, pdf_document *dst, const int *rotates, int nrotates) {
    int n, i;
    if (!rotates || nrotates <= 0) return;
    n = pdf_count_pages(ctx, dst);
    for (i = 0; i < n && i < nrotates; i++) {
        int extra = rotates[i] % 360;
        pdf_obj *obj, *ro;
        int cur = 0, next;
        if (extra < 0) extra += 360;
        if (!extra) continue;
        obj = pdf_lookup_page_obj(ctx, dst, i);
        ro = pdf_dict_get(ctx, obj, PDF_NAME(Rotate));
        if (pdf_is_int(ctx, ro)) cur = pdf_to_int(ctx, ro);
        next = (cur + extra) % 360;
        if (next < 0) next += 360;
        if (next)
            pdf_dict_put_int(ctx, obj, PDF_NAME(Rotate), next);
        else
            pdf_dict_del(ctx, obj, PDF_NAME(Rotate));
    }
}

int pc_compose(const char **paths, const char **magics, int nsrc, const int *pages, int npages, float em,
               const char *password, const char *dest, const char *tmpdir, const int *rotates, int nrotates) {
    fz_context *ctx;
    pdf_document *dst = NULL;
    int code = PC_OK;
    int i;
    if (nsrc <= 0) return PC_EXPORT;
    if (pages && (nsrc != 1 || npages <= 0)) return PC_EXPORT;
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx) return PC_FAIL;
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        dst = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        fz_drop_context(ctx);
        return PC_FAIL;
    }
    for (i = 0; i < nsrc && code == PC_OK; i++) {
        fz_document *src = NULL;
        pdf_document *pdf;
        int rc = open_source(ctx, paths[i], magics[i], em, password, &src);
        if (rc != PC_OK) {
            code = rc;
            break;
        }
        pdf = pdf_specifics(ctx, src);
        if (pages) {
            int k;
            if (!pdf) {
                code = PC_NOT_PDF;
            } else {
                fz_try(ctx) {
                    for (k = 0; k < npages; k++)
                        pdf_graft_page(ctx, dst, pdf_count_pages(ctx, dst), pdf, pages[k]);
                }
                fz_catch(ctx) {
                    code = PC_EXPORT;
                }
            }
        } else if (pdf) {
            if (!graft_all(ctx, dst, pdf)) code = PC_EXPORT;
        } else {
            char tmp[4096];
            fz_document *conv = NULL;
            snprintf(tmp, sizeof tmp, "%s/papercut-conv-%d.pdf", tmpdir ? tmpdir : ".", i);
            if (!write_as_pdf(ctx, src, tmp)) {
                code = PC_EXPORT;
            } else {
                int rc2 = open_source(ctx, tmp, "application/pdf", em, NULL, &conv);
                if (rc2 != PC_OK) {
                    code = PC_EXPORT;
                } else {
                    pdf_document *cpdf = pdf_specifics(ctx, conv);
                    if (!cpdf || !graft_all(ctx, dst, cpdf)) code = PC_EXPORT;
                    fz_drop_document(ctx, conv);
                }
            }
            remove(tmp);
        }
        fz_drop_document(ctx, src);
    }
    if (code == PC_OK) {
        fz_try(ctx) {
            pdf_write_options opts;
            apply_rotates(ctx, dst, rotates, nrotates);
            strip_tracing(ctx, dst);
            remove(dest);
            pdf_parse_write_options(ctx, &opts, "garbage=2,pretty=no,compress=yes,sanitize=yes");
            pdf_save_document(ctx, dst, dest, &opts);
        }
        fz_catch(ctx) {
            code = PC_EXPORT;
        }
    }
    pdf_drop_document(ctx, dst);
    fz_drop_context(ctx);
    return code;
}
