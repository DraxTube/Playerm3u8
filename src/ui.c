#include "ui.h"
#include <stdio.h>
#include <string.h>

static vita2d_pgf *g_font = NULL;

void ui_init(void) {
    g_font = vita2d_load_default_pgf();
}

void ui_shutdown(void) {
    if (g_font) { vita2d_free_pgf(g_font); g_font = NULL; }
}

void ui_text(int x, int y, unsigned int col, const char *s) {
    if (!g_font || !s) return;
    vita2d_pgf_draw_text(g_font, x + 1, y + 1, RGBA8(0, 0, 0, 160), 1.0f, s);
    vita2d_pgf_draw_text(g_font, x,     y,     col,                  1.0f, s);
}

void ui_title(int x, int y, unsigned int col, const char *s) {
    if (!g_font || !s) return;
    vita2d_pgf_draw_text(g_font, x + 1, y + 1, RGBA8(0, 0, 0, 180), 1.2f, s);
    vita2d_pgf_draw_text(g_font, x,     y,     col,                  1.2f, s);
}

void ui_rect(int x, int y, int w, int h, unsigned int col) {
    vita2d_draw_rectangle((float)x, (float)y, (float)w, (float)h, col);
}

void ui_progress(int x, int y, int w, int h, float p,
                 unsigned int bg, unsigned int fg) {
    ui_rect(x, y, w, h, bg);
    if (p > 0.0f && p <= 1.0f)
        ui_rect(x, y, (int)(w * p), h, fg);
}

void ui_ms_to_str(uint64_t ms, char *buf, int sz) {
    uint64_t s = ms / 1000;
    snprintf(buf, sz, "%02llu:%02llu",
             (unsigned long long)(s / 60),
             (unsigned long long)(s % 60));
}
