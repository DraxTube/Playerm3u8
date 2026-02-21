#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>
#include <string.h>
#include <stdio.h>

#include "m3u8_parser.h"
#include "player.h"
#include "file_browser.h"
#include "ui.h"

/* -----------------------------------------------------------------------
   Stato applicazione
   ----------------------------------------------------------------------- */
typedef enum {
    ST_BROWSER = 0,
    ST_PLAYLIST,
    ST_PLAYING,
    ST_ERROR
} AppState;

static AppState  g_state      = ST_BROWSER;
static FileBrowser g_fb;
static Playlist  g_pl;
static int       g_pl_sel     = 0;
static int       g_pl_scroll  = 0;
static char      g_errmsg[512] = "";

/* -----------------------------------------------------------------------
   Helper: avvia traccia idx
   ----------------------------------------------------------------------- */
static void play_track(int idx) {
    if (idx < 0 || idx >= g_pl.count) return;
    g_pl_sel = idx;
    int r = player_play(g_pl.entries[idx].path);
    if (r < 0) {
        snprintf(g_errmsg, sizeof(g_errmsg),
                 "Errore avvio: %d\n%s", r, g_pl.entries[idx].path);
        g_state = ST_ERROR;
    } else {
        g_state = ST_PLAYING;
    }
}

/* Avvia un file media diretto (non playlist) come playlist a 1 traccia */
static void play_single(const char *path) {
    playlist_clear(&g_pl);
    strncpy(g_pl.entries[0].path, path, PLAYLIST_MAX_PATH - 1);
    /* Titolo = nome file senza estensione */
    const char *sl  = strrchr(path, '/');
    const char *nam = sl ? sl + 1 : path;
    strncpy(g_pl.entries[0].title, nam, PLAYLIST_MAX_TITLE - 1);
    char *dot = strrchr(g_pl.entries[0].title, '.');
    if (dot) *dot = '\0';
    g_pl.entries[0].duration = -1.0f;
    g_pl.count = 1;
    play_track(0);
}

/* -----------------------------------------------------------------------
   Rendering
   ----------------------------------------------------------------------- */
static void draw_header(const char *title) {
    ui_rect(0, 0, SCREEN_W, UI_HEADER_H, UI_COL_PANEL);
    ui_rect(0, UI_HEADER_H - 2, SCREEN_W, 2, UI_COL_ACCENT);
    ui_title(14, UI_HEADER_H - 10, UI_COL_ACCENT, title);
}

static void draw_footer(const char *hints) {
    int fy = SCREEN_H - UI_FOOTER_H;
    ui_rect(0, fy, SCREEN_W, UI_FOOTER_H, UI_COL_PANEL);
    ui_rect(0, fy, SCREEN_W, 2, UI_COL_ACCENT2);
    ui_text(14, fy + 7, UI_COL_DIM, hints);
}

static void draw_row(int i, int selected, const char *text, unsigned int col) {
    int ry = UI_PANEL_Y + 3 + i * UI_ROW_H;
    if (selected)
        ui_rect(UI_PANEL_X + 2, ry, UI_PANEL_W - 4, UI_ROW_H - 2, UI_COL_SEL);
    ui_text(UI_PANEL_X + 10, ry + 6, col, text);
}

/* --- Browser --- */
static void render_browser(void) {
    draw_header("M3U8 Player  |  Seleziona file o playlist");
    ui_text(UI_PANEL_X, UI_HEADER_H + 6, UI_COL_DIM, g_fb.current_path);
    ui_rect(UI_PANEL_X, UI_PANEL_Y, UI_PANEL_W, UI_PANEL_H, UI_COL_PANEL);

    for (int i = 0; i < UI_VISIBLE; i++) {
        int idx = i + g_fb.scroll;
        if (idx >= g_fb.count) break;
        FBEntry *e   = &g_fb.entries[idx];
        int      sel = (idx == g_fb.selected);
        char     buf[FB_MAX_NAME + 10];
        if (e->is_dir)
            snprintf(buf, sizeof(buf), "[DIR]  %s", e->name);
        else
            snprintf(buf, sizeof(buf), "       %s", e->name);
        draw_row(i, sel, buf, sel ? UI_COL_TEXT : UI_COL_DIM);
    }

    /* Scrollbar minimale */
    if (g_fb.count > UI_VISIBLE) {
        int bar_h = UI_PANEL_H * UI_VISIBLE / g_fb.count;
        int bar_y = UI_PANEL_Y + UI_PANEL_H * g_fb.scroll / g_fb.count;
        ui_rect(SCREEN_W - UI_PANEL_X - 4, bar_y, 3, bar_h, UI_COL_ACCENT2);
    }

    draw_footer("[^v] Muovi  [<>] Pagina  [X] Entra/Apri  [O] Radice");
}

/* --- Playlist --- */
static void render_playlist(void) {
    draw_header("Playlist");
    char info[64];
    snprintf(info, sizeof(info), "%d tracce", g_pl.count);
    ui_text(UI_PANEL_X, UI_HEADER_H + 6, UI_COL_DIM, info);
    ui_rect(UI_PANEL_X, UI_PANEL_Y, UI_PANEL_W, UI_PANEL_H, UI_COL_PANEL);

    for (int i = 0; i < UI_VISIBLE; i++) {
        int idx = i + g_pl_scroll;
        if (idx >= g_pl.count) break;
        PlaylistEntry *e   = &g_pl.entries[idx];
        int            sel = (idx == g_pl_sel);

        char dur[12] = "--:--";
        if (e->duration > 0)
            ui_ms_to_str((uint64_t)(e->duration * 1000.f), dur, sizeof(dur));

        char buf[PLAYLIST_MAX_TITLE + 24];
        snprintf(buf, sizeof(buf), "%3d.  %-42s  %s", idx + 1, e->title, dur);
        draw_row(i, sel, buf, sel ? UI_COL_TEXT : UI_COL_DIM);
    }

    if (g_pl.count > UI_VISIBLE) {
        int bar_h = UI_PANEL_H * UI_VISIBLE / g_pl.count;
        int bar_y = UI_PANEL_Y + UI_PANEL_H * g_pl_scroll / g_pl.count;
        ui_rect(SCREEN_W - UI_PANEL_X - 4, bar_y, 3, bar_h, UI_COL_ACCENT2);
    }

    draw_footer("[^v] Muovi  [X] Riproduci  [Q] Indietro");
}

/* --- Playing --- */
static void render_playing(void) {
    player_render_frame();   /* video o sfondo nero */

    /* Overlay basso */
    int ov_h = 108;
    int ov_y = SCREEN_H - UI_FOOTER_H - ov_h;
    ui_rect(0, ov_y, SCREEN_W, ov_h, UI_COL_OVERLAY);

    PlaylistEntry *e  = &g_pl.entries[g_pl_sel];
    PlayerStatus   st = player_get_status();

    /* Titolo */
    ui_text(16, ov_y + 8, UI_COL_TEXT, e->title);

    /* Barra progresso */
    float prog = (st.duration_ms > 0)
                 ? (float)st.position_ms / (float)st.duration_ms : 0.0f;
    ui_progress(16, ov_y + 38, SCREEN_W - 32, 5,
                prog, UI_COL_BAR_BG, UI_COL_BAR_FG);

    /* Tempo */
    char pos[12], dur[12];
    ui_ms_to_str(st.position_ms, pos, sizeof(pos));
    ui_ms_to_str(st.duration_ms, dur, sizeof(dur));
    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "%s / %s", pos, dur);
    ui_text(16, ov_y + 52, UI_COL_DIM, time_buf);

    /* Stato play/pausa */
    const char *stlbl = (st.state == PLAYER_PAUSED) ? "|| PAUSA" : ">  PLAY";
    ui_text(SCREEN_W / 2 - 28, ov_y + 52, UI_COL_ACCENT, stlbl);

    /* Traccia N/M */
    char trk[20];
    snprintf(trk, sizeof(trk), "%d / %d", g_pl_sel + 1, g_pl.count);
    ui_text(SCREEN_W - 80, ov_y + 52, UI_COL_DIM, trk);

    draw_footer("[X] Pausa  [Q] Stop  [L] Prec  [R] Prox");
}

/* --- Errore --- */
static void render_error(void) {
    draw_header("Errore");
    ui_rect(UI_PANEL_X, UI_PANEL_Y, UI_PANEL_W, UI_PANEL_H, UI_COL_PANEL);
    ui_text(UI_PANEL_X + 14, UI_PANEL_Y + 18, UI_COL_ERROR,
            "Impossibile riprodurre il file:");

    /* Spezza il messaggio sulle newline */
    char tmp[512];
    strncpy(tmp, g_errmsg, sizeof(tmp) - 1);
    int yy = UI_PANEL_Y + 46;
    char *line = tmp;
    while (*line && yy < UI_PANEL_Y + UI_PANEL_H - 20) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        ui_text(UI_PANEL_X + 14, yy, UI_COL_TEXT, line);
        yy += 22;
        if (!nl) break;
        line = nl + 1;
    }

    draw_footer("[O] Indietro");
}

/* -----------------------------------------------------------------------
   Input
   ----------------------------------------------------------------------- */
static SceCtrlData g_prev;

static int pressed(SceCtrlData *c, unsigned int btn) {
    return (c->buttons & btn) && !(g_prev.buttons & btn);
}

static void input_browser(SceCtrlData *c) {
    if (pressed(c, SCE_CTRL_UP))    fb_move(&g_fb, -1,       UI_VISIBLE);
    if (pressed(c, SCE_CTRL_DOWN))  fb_move(&g_fb,  1,       UI_VISIBLE);
    if (pressed(c, SCE_CTRL_LEFT))  fb_move(&g_fb, -UI_VISIBLE, UI_VISIBLE);
    if (pressed(c, SCE_CTRL_RIGHT)) fb_move(&g_fb,  UI_VISIBLE, UI_VISIBLE);

    if (pressed(c, SCE_CTRL_CROSS)) {
        if (fb_selected_is_dir(&g_fb)) {
            fb_enter(&g_fb);
        } else {
            const char *path = fb_selected_path(&g_fb);
            if (!path) return;
            /* Playlist o media diretto? */
            const char *ext = strrchr(path, '.');
            if (ext && (strcasecmp(ext, ".m3u8") == 0 ||
                        strcasecmp(ext, ".m3u")  == 0)) {
                int r = playlist_load(path, &g_pl);
                if (r < 0) {
                    snprintf(g_errmsg, sizeof(g_errmsg),
                             "Impossibile leggere la playlist:\n%s", path);
                    g_state = ST_ERROR;
                } else {
                    g_pl_sel = 0; g_pl_scroll = 0;
                    g_state = ST_PLAYLIST;
                }
            } else {
                /* File media diretto */
                play_single(path);
            }
        }
    }
    if (pressed(c, SCE_CTRL_CIRCLE))
        fb_init(&g_fb, "ux0:");
}

static void input_playlist(SceCtrlData *c) {
    if (pressed(c, SCE_CTRL_UP)) {
        if (g_pl_sel > 0) g_pl_sel--;
        if (g_pl_sel < g_pl_scroll) g_pl_scroll = g_pl_sel;
    }
    if (pressed(c, SCE_CTRL_DOWN)) {
        if (g_pl_sel < g_pl.count - 1) g_pl_sel++;
        if (g_pl_sel >= g_pl_scroll + UI_VISIBLE)
            g_pl_scroll = g_pl_sel - UI_VISIBLE + 1;
    }
    if (pressed(c, SCE_CTRL_CROSS))  play_track(g_pl_sel);
    if (pressed(c, SCE_CTRL_SQUARE)) {
        playlist_clear(&g_pl);
        g_state = ST_BROWSER;
    }
}

static void input_playing(SceCtrlData *c) {
    if (pressed(c, SCE_CTRL_CROSS))     player_toggle_pause();
    if (pressed(c, SCE_CTRL_SQUARE))  { player_stop(); g_state = ST_PLAYLIST; }
    if (pressed(c, SCE_CTRL_LTRIGGER)) { player_stop(); play_track(g_pl_sel - 1); }
    if (pressed(c, SCE_CTRL_RTRIGGER)) { player_stop(); play_track(g_pl_sel + 1); }
}

static void input_error(SceCtrlData *c) {
    if (pressed(c, SCE_CTRL_CIRCLE))
        g_state = (g_pl.count > 0) ? ST_PLAYLIST : ST_BROWSER;
}

/* -----------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------- */
int main(void) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);

    vita2d_init();
    vita2d_set_clear_color(UI_COL_BG);
    ui_init();

    if (player_init() < 0) {
        /* Errore grave raro, mostriamo schermata di errore */
        snprintf(g_errmsg, sizeof(g_errmsg), "player_init() fallito");
        g_state = ST_ERROR;
    } else {
        fb_init(&g_fb, "ux0:");
    }

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    memset(&g_prev, 0, sizeof(g_prev));

    while (1) {
        SceCtrlData ctrl;
        sceCtrlPeekBufferPositive(0, &ctrl, 1);

        switch (g_state) {
        case ST_BROWSER:  input_browser(&ctrl);  break;
        case ST_PLAYLIST: input_playlist(&ctrl); break;
        case ST_PLAYING:  input_playing(&ctrl);  break;
        case ST_ERROR:    input_error(&ctrl);    break;
        }
        g_prev = ctrl;

        /* Aggiornamento player */
        if (g_state == ST_PLAYING) {
            PlayerState ps = player_update();
            if (ps == PLAYER_FINISHED) {
                player_stop();
                if (g_pl_sel + 1 < g_pl.count)
                    play_track(g_pl_sel + 1);
                else
                    g_state = ST_PLAYLIST;
            } else if (ps == PLAYER_ERROR) {
                snprintf(g_errmsg, sizeof(g_errmsg),
                         "Errore durante la riproduzione.\n%s",
                         g_pl.entries[g_pl_sel].path);
                player_stop();
                g_state = ST_ERROR;
            }
        }

        /* Rendering */
        vita2d_start_drawing();
        vita2d_clear_screen();
        switch (g_state) {
        case ST_BROWSER:  render_browser();  break;
        case ST_PLAYLIST: render_playlist(); break;
        case ST_PLAYING:  render_playing();  break;
        case ST_ERROR:    render_error();    break;
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    player_shutdown();
    ui_shutdown();
    vita2d_fini();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER);
    sceKernelExitProcess(0);
    return 0;
}
