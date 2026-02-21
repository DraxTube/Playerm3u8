#pragma once
#include <vita2d.h>
#include <stdint.h>

typedef enum {
    PLAYER_STOPPED = 0,
    PLAYER_PLAYING,
    PLAYER_PAUSED,
    PLAYER_FINISHED,
    PLAYER_ERROR
} PlayerState;

typedef struct {
    PlayerState state;
    uint64_t    duration_ms;
    uint64_t    position_ms;
} PlayerStatus;

/** Inizializza il player (thread audio, risorse).
 *  Chiamare una volta all'avvio. */
int  player_init(void);

/** Carica e avvia un file locale (MP4, H.264, AAC, MP3).
 *  @return 0 ok, <0 codice errore SceAvPlayer */
int  player_play(const char *filepath);

/** Pausa / riprendi */
void player_toggle_pause(void);

/** Ferma la traccia corrente e libera le risorse */
void player_stop(void);

/** Da chiamare ogni frame: aggiorna posizione e rileva fine.
 *  @return stato corrente */
PlayerState player_update(void);

/** Renderizza il frame video corrente. No-op se solo audio. */
void player_render_frame(void);

/** Restituisce lo stato corrente */
PlayerStatus player_get_status(void);

/** Chiude tutto (da chiamare prima di sceKernelExitProcess) */
void player_shutdown(void);
