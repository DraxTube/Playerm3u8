#pragma once

#define FB_MAX_ENTRIES 256
#define FB_MAX_PATH    512
#define FB_MAX_NAME    256

typedef struct {
    char name[FB_MAX_NAME];
    char full_path[FB_MAX_PATH];
    int  is_dir;
} FBEntry;

typedef struct {
    FBEntry entries[FB_MAX_ENTRIES];
    int     count;
    int     selected;
    int     scroll;
    char    current_path[FB_MAX_PATH];
} FileBrowser;

/** Inizializza e carica la directory radice (es. "ux0:") */
void fb_init(FileBrowser *fb, const char *root_path);

/** Entra nella dir selezionata. Restituisce 1 se è stato selezionato un file. */
int  fb_enter(FileBrowser *fb);

/** Sposta la selezione (delta = ±1, ±page) */
void fb_move(FileBrowser *fb, int delta, int visible_rows);

/** Path dell'entry selezionata (NULL se vuoto) */
const char *fb_selected_path(const FileBrowser *fb);

/** 1 se l'entry selezionata è una directory */
int fb_selected_is_dir(const FileBrowser *fb);
