#include "file_browser.h"
#include <psp2/io/dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Estensioni selezionabili come playlist */
static int is_playlist(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return (strcasecmp(ext, ".m3u8") == 0 || strcasecmp(ext, ".m3u") == 0);
}

/* Estensioni selezionabili come file media diretto */
static int is_media(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    if (strcasecmp(ext, ".mp4")  == 0) return 1;
    if (strcasecmp(ext, ".m4v")  == 0) return 1;
    if (strcasecmp(ext, ".m4a")  == 0) return 1;
    if (strcasecmp(ext, ".aac")  == 0) return 1;
    if (strcasecmp(ext, ".mp3")  == 0) return 1;
    if (strcasecmp(ext, ".at9")  == 0) return 1;
    return 0;
}

static int cmp_entries(const void *a, const void *b) {
    const FBEntry *ea = (const FBEntry *)a;
    const FBEntry *eb = (const FBEntry *)b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcasecmp(ea->name, eb->name);
}

static void fb_load(FileBrowser *fb) {
    fb->count    = 0;
    fb->selected = 0;
    fb->scroll   = 0;

    /* Voce ".." se siamo in una sotto-directory (path contiene '/') */
    int has_parent = (strchr(fb->current_path, '/') != NULL);
    if (has_parent) {
        FBEntry *up = &fb->entries[fb->count++];
        strncpy(up->name, "..", FB_MAX_NAME - 1);
        up->name[FB_MAX_NAME - 1] = '\0';
        strncpy(up->full_path, fb->current_path, FB_MAX_PATH - 1);
        up->full_path[FB_MAX_PATH - 1] = '\0';
        char *sl = strrchr(up->full_path, '/');
        if (sl) *sl = '\0';
        up->is_dir = 1;
    }

    SceUID dfd = sceIoDopen(fb->current_path);
    if (dfd < 0) return;

    SceIoDirent dir;
    while (sceIoDread(dfd, &dir) > 0 && fb->count < FB_MAX_ENTRIES) {
        if (dir.d_name[0] == '.') continue;

        int is_dir = SCE_S_ISDIR(dir.d_stat.st_mode);
        if (!is_dir && !is_playlist(dir.d_name) && !is_media(dir.d_name))
            continue;

        FBEntry *e = &fb->entries[fb->count++];
        strncpy(e->name, dir.d_name, FB_MAX_NAME - 1);
        e->name[FB_MAX_NAME - 1] = '\0';
        snprintf(e->full_path, FB_MAX_PATH, "%s/%s",
                 fb->current_path, dir.d_name);
        e->is_dir = is_dir;
    }
    sceIoDclose(dfd);

    /* Ordina tutto tranne la voce ".." che sta in testa */
    int off = has_parent ? 1 : 0;
    if (fb->count - off > 1)
        qsort(&fb->entries[off], fb->count - off, sizeof(FBEntry), cmp_entries);
}

void fb_init(FileBrowser *fb, const char *root_path) {
    memset(fb, 0, sizeof(FileBrowser));
    strncpy(fb->current_path, root_path, FB_MAX_PATH - 1);
    fb_load(fb);
}

int fb_enter(FileBrowser *fb) {
    if (fb->count == 0) return 0;
    FBEntry *e = &fb->entries[fb->selected];
    if (e->is_dir) {
        strncpy(fb->current_path, e->full_path, FB_MAX_PATH - 1);
        fb_load(fb);
        return 0;
    }
    return 1; /* file selezionato */
}

void fb_move(FileBrowser *fb, int delta, int visible_rows) {
    fb->selected += delta;
    if (fb->selected < 0)            fb->selected = 0;
    if (fb->selected >= fb->count)   fb->selected = fb->count - 1;
    if (fb->selected < fb->scroll)   fb->scroll   = fb->selected;
    if (fb->selected >= fb->scroll + visible_rows)
        fb->scroll = fb->selected - visible_rows + 1;
}

const char *fb_selected_path(const FileBrowser *fb) {
    if (fb->count == 0) return NULL;
    return fb->entries[fb->selected].full_path;
}

int fb_selected_is_dir(const FileBrowser *fb) {
    if (fb->count == 0) return 0;
    return fb->entries[fb->selected].is_dir;
}
