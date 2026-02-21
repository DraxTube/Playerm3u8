#pragma once
#include <stdint.h>

#define PLAYLIST_MAX_ENTRIES 512
#define PLAYLIST_MAX_PATH    512
#define PLAYLIST_MAX_TITLE   256

typedef struct {
    char  path[PLAYLIST_MAX_PATH];
    char  title[PLAYLIST_MAX_TITLE];
    float duration;   /* secondi, -1 = sconosciuta */
} PlaylistEntry;

typedef struct {
    PlaylistEntry entries[PLAYLIST_MAX_ENTRIES];
    int           count;
} Playlist;

/**
 * Legge un file .m3u / .m3u8 con path locali.
 * I path relativi vengono risolti rispetto alla directory del file.
 * Restituisce 0 se ha trovato almeno una voce, -1 altrimenti.
 */
int  playlist_load(const char *filepath, Playlist *pl);

/** Azzera la struttura */
void playlist_clear(Playlist *pl);
