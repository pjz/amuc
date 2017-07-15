#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

cairo_surface_t *cai_get_surface(BgrWin *bgr);
cairo_pattern_t *col2cai_col(uint col);
