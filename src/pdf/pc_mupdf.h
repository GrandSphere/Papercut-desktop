#ifndef PC_MUPDF_H
#define PC_MUPDF_H

/* MuPDF setjmp/longjmp is confined to pc_mupdf.c. */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct pc_engine pc_engine;

enum {
    PC_OK = 0,
    PC_PASSWORD = 1,
    PC_NOT_PDF = 2,
    PC_EXPORT = 3,
    PC_FAIL = 4
};

typedef struct {
    unsigned int c;
    float q[8]; /* ulx uly urx ury llx lly lrx lry */
} pc_char;

typedef struct {
    float r[4]; /* left top right bottom (normalised) */
} pc_rect;

typedef struct {
    float r[4];
    char *uri;
    int external;
} pc_link;

typedef struct {
    pc_char *chars;
    int nchars;
    pc_rect *images;
    int nimages;
    pc_link *links;
    int nlinks;
} pc_content;

typedef struct {
    char *title;
    char *uri;
    int page; /* -1 when unresolved */
    int has_y;
    float y;
    int depth;
} pc_outline_item;

pc_engine *pc_new(void);
void pc_free(pc_engine *e);

/* Returns PC_OK, PC_PASSWORD, or PC_FAIL. */
int pc_open(pc_engine *e, const char *path, const char *magic);
int pc_authenticate(pc_engine *e, const char *password);
void pc_close(pc_engine *e);
int pc_is_open(pc_engine *e);
int pc_is_reflowable(pc_engine *e);
void pc_set_user_css(pc_engine *e, const char *css);
int pc_layout(pc_engine *e, float w, float h, float em);
int pc_page_count(pc_engine *e);
/* out = x0 y0 x1 y1 */
int pc_page_bounds(pc_engine *e, int index, float out[4]);

/* Opaque ARGB; caller frees with pc_free_buf. */
int pc_render(pc_engine *e, int index, float scale, float e_off, float f_off, uint32_t **pixels, int *w, int *h,
              int extra_cw);
void pc_free_buf(void *p);

int pc_content_load(pc_engine *e, int index, pc_content *out);
void pc_content_free(pc_content *c);

int pc_outline(pc_engine *e, pc_outline_item **items, int *count);
void pc_outline_free(pc_outline_item *items, int count);

/* Internal link: 1 on success; page 0-based, optional y. */
int pc_resolve_uri(pc_engine *e, const char *uri, int *page, int *has_y, float *y);
int pc_is_external(pc_engine *e, const char *uri);

const char *pc_last_error(pc_engine *e);

/* pages NULL: append all; else single-PDF extract. Returns PC_* status. */
int pc_compose(const char **paths, const char **magics, int nsrc, const int *pages, int npages,
               float em, const char *password, const char *dest, const char *tmpdir, const int *rotates,
               int nrotates);

#ifdef __cplusplus
}
#endif

#endif
