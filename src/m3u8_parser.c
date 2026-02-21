#include "m3u8_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void trim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' '))
        s[--len] = '\0';
}

static void get_dir(const char *path, char *out, int out_sz) {
    strncpy(out, path, out_sz - 1);
    out[out_sz - 1] = '\0';
    char *sl = strrchr(out, '/');
    if (sl) *sl = '\0';
    else    out[0] = '\0';
}

/* Risolve un path relativo rispetto a base_dir.
   Se entry_path contiene ':' è già assoluto (es. ux0:/...). */
static void resolve(const char *base_dir, const char *entry, char *out, int sz) {
    if (strchr(entry, ':') != NULL) {
        strncpy(out, entry, sz - 1);
        out[sz - 1] = '\0';
    } else if (entry[0] == '/') {
        /* Path assoluto POSIX-style senza device: non comune sulla Vita,
           prendilo così com'è */
        strncpy(out, entry, sz - 1);
        out[sz - 1] = '\0';
    } else {
        snprintf(out, sz, "%s/%s", base_dir, entry);
    }
}

/* Controlla se l'estensione è un formato media supportato da SceAvPlayer */
static int is_media(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 1; /* senza estensione: proviamo */
    /* Formati locali supportati da SceAvPlayer */
    if (strcasecmp(ext, ".mp4")  == 0) return 1;
    if (strcasecmp(ext, ".m4v")  == 0) return 1;
    if (strcasecmp(ext, ".m4a")  == 0) return 1;
    if (strcasecmp(ext, ".avc")  == 0) return 1;
    if (strcasecmp(ext, ".aac")  == 0) return 1;
    if (strcasecmp(ext, ".mp3")  == 0) return 1;
    if (strcasecmp(ext, ".at9")  == 0) return 1;
    if (strcasecmp(ext, ".wav")  == 0) return 1;
    /* Segmenti TS remoti: NON supportati, scartati */
    if (strcasecmp(ext, ".ts")   == 0) return 0;
    if (strcasecmp(ext, ".m3u8") == 0) return 0; /* playlist annidata */
    return 1;
}

int playlist_load(const char *filepath, Playlist *pl) {
    if (!filepath || !pl) return -1;
    playlist_clear(pl);

    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    char base_dir[PLAYLIST_MAX_PATH];
    get_dir(filepath, base_dir, sizeof(base_dir));

    char line[PLAYLIST_MAX_PATH * 2];
    char pending_title[PLAYLIST_MAX_TITLE] = "";
    float pending_dur = -1.0f;
    int first = 1;

    while (fgets(line, sizeof(line), f) && pl->count < PLAYLIST_MAX_ENTRIES) {
        trim(line);
        if (line[0] == '\0') continue;

        if (first) {
            first = 0;
            /* Prima riga: potrebbe essere #EXTM3U, continuiamo */
            if (strncmp(line, "#EXTM3U", 7) == 0) continue;
            /* Se non è un commento è già una entry, cade nel ramo sotto */
            if (line[0] == '#') continue;
        }

        if (line[0] == '#') {
            if (strncmp(line, "#EXTINF:", 8) == 0) {
                pending_dur = (float)atof(line + 8);
                char *comma = strchr(line + 8, ',');
                if (comma && *(comma + 1) != '\0') {
                    strncpy(pending_title, comma + 1, PLAYLIST_MAX_TITLE - 1);
                    pending_title[PLAYLIST_MAX_TITLE - 1] = '\0';
                }
            }
            /* Tutti gli altri tag HLS (#EXT-X-*) vengono ignorati */
            continue;
        }

        /* È un path */
        if (!is_media(line)) {
            /* Scarta silenziosamente segmenti .ts remoti o playlist annidate */
            pending_title[0] = '\0';
            pending_dur = -1.0f;
            continue;
        }

        PlaylistEntry *e = &pl->entries[pl->count];

        resolve(base_dir, line, e->path, PLAYLIST_MAX_PATH);

        if (pending_title[0] != '\0') {
            strncpy(e->title, pending_title, PLAYLIST_MAX_TITLE - 1);
            e->title[PLAYLIST_MAX_TITLE - 1] = '\0';
        } else {
            /* Nome file come titolo */
            const char *sl = strrchr(line, '/');
            const char *fname = sl ? sl + 1 : line;
            /* Rimuovi estensione dal titolo */
            strncpy(e->title, fname, PLAYLIST_MAX_TITLE - 1);
            e->title[PLAYLIST_MAX_TITLE - 1] = '\0';
            char *dot = strrchr(e->title, '.');
            if (dot) *dot = '\0';
        }

        e->duration = pending_dur;
        pending_title[0] = '\0';
        pending_dur = -1.0f;
        pl->count++;
    }

    fclose(f);
    return (pl->count > 0) ? 0 : -1;
}

void playlist_clear(Playlist *pl) {
    if (pl) memset(pl, 0, sizeof(Playlist));
}
