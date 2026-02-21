#pragma once
#include <vita2d.h>
#include <stdint.h>

/* Risoluzione PS Vita */
#define SCREEN_W 960
#define SCREEN_H 544

/* Palette tema scuro */
#define UI_COL_BG        RGBA8( 18,  18,  22, 255)
#define UI_COL_PANEL     RGBA8( 28,  28,  35, 245)
#define UI_COL_ACCENT    RGBA8(  0, 120, 220, 255)
#define UI_COL_ACCENT2   RGBA8(  0,  70, 160, 255)
#define UI_COL_SEL       RGBA8(  0,  90, 190, 200)
#define UI_COL_TEXT      RGBA8(225, 225, 225, 255)
#define UI_COL_DIM       RGBA8(130, 130, 130, 255)
#define UI_COL_BAR_BG    RGBA8( 45,  45,  55, 255)
#define UI_COL_BAR_FG    RGBA8(  0, 140, 240, 255)
#define UI_COL_ERROR     RGBA8(220,  55,  55, 255)
#define UI_COL_OVERLAY   RGBA8(  0,   0,   0, 185)

/* Layout */
#define UI_HEADER_H  40
#define UI_FOOTER_H  28
#define UI_PANEL_X   14
#define UI_PANEL_Y   (UI_HEADER_H + 26)
#define UI_PANEL_W   (SCREEN_W - 28)
#define UI_PANEL_H   (SCREEN_H - UI_PANEL_Y - UI_FOOTER_H - 6)
#define UI_ROW_H     28
#define UI_VISIBLE   ((UI_PANEL_H - 6) / UI_ROW_H)

void ui_init(void);
void ui_shutdown(void);

void ui_text(int x, int y, unsigned int col, const char *s);
void ui_title(int x, int y, unsigned int col, const char *s);
void ui_rect(int x, int y, int w, int h, unsigned int col);
void ui_progress(int x, int y, int w, int h, float p,
                 unsigned int bg, unsigned int fg);
void ui_ms_to_str(uint64_t ms, char *buf, int sz);
