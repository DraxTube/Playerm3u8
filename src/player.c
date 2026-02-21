#include "player.h"
#include "ui.h"
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <vita2d.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
   Limiti e costanti
   ----------------------------------------------------------------------- */
#define MAX_GPU_ALLOCS   32
#define AUDIO_GRAIN      1024

/* -----------------------------------------------------------------------
   Allocazioni GPU
   ----------------------------------------------------------------------- */
typedef struct { SceUID uid; void *ptr; } GpuBlock;
static GpuBlock  g_gpu[MAX_GPU_ALLOCS];
static int       g_gpu_n = 0;

/* -----------------------------------------------------------------------
   Stato globale
   ----------------------------------------------------------------------- */
static SceAvPlayerHandle g_player     = 0;
static int               g_audio_port = -1;
static SceUID            g_audio_thid = -1;
static volatile int      g_audio_run  = 0;
static PlayerStatus      g_status;

/* Video */
static vita2d_texture   *g_vtex   = NULL;
static int               g_vtex_w = 0;
static int               g_vtex_h = 0;
static int               g_has_video = 0;

/* -----------------------------------------------------------------------
   Callback memoria CPU (heap normale)
   ----------------------------------------------------------------------- */
static void *cb_alloc(void *p, uint32_t align, uint32_t size) {
    (void)p;
    if (align < 4) align = 4;
    size = (size + align - 1) & ~(align - 1);
    void *ptr = memalign(align, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}
static void cb_free(void *p, void *ptr) { (void)p; free(ptr); }

/* -----------------------------------------------------------------------
   Callback memoria GPU (CDRAM per texture decodifica)
   ----------------------------------------------------------------------- */
static void *cb_gpu_alloc(void *p, uint32_t align, uint32_t size) {
    (void)p;
    if (g_gpu_n >= MAX_GPU_ALLOCS) return NULL;

    /* SceAvPlayer richiede allineamento minimo 256KB per CDRAM */
    const uint32_t CDRAM_ALIGN = 256 * 1024;
    if (align < CDRAM_ALIGN) align = CDRAM_ALIGN;
    size = (size + align - 1) & ~(align - 1);

    SceUID uid = sceKernelAllocMemBlock("avp_gpu",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
    if (uid < 0) return NULL;

    void *ptr = NULL;
    if (sceKernelGetMemBlockBase(uid, &ptr) < 0) {
        sceKernelFreeMemBlock(uid);
        return NULL;
    }
    g_gpu[g_gpu_n].uid = uid;
    g_gpu[g_gpu_n].ptr = ptr;
    g_gpu_n++;
    return ptr;
}
static void cb_gpu_free(void *p, void *ptr) {
    (void)p;
    for (int i = 0; i < g_gpu_n; i++) {
        if (g_gpu[i].ptr == ptr) {
            sceKernelFreeMemBlock(g_gpu[i].uid);
            g_gpu[i] = g_gpu[g_gpu_n - 1];
            g_gpu_n--;
            return;
        }
    }
}

/* -----------------------------------------------------------------------
   Event callback (obbligatorio, altrimenti init fallisce)
   ----------------------------------------------------------------------- */
static void cb_event(void *p, int32_t evId, int32_t srcId, void *evData) {
    (void)p; (void)srcId; (void)evData;
    /*
     * SCE_AVPLAYER_STATE_BUFFERING = 1
     * SCE_AVPLAYER_STATE_READY     = 2
     * SCE_AVPLAYER_STATE_PLAY      = 3
     * SCE_AVPLAYER_STATE_PAUSE     = 4
     * SCE_AVPLAYER_STATE_STOP      = 5
     * SCE_AVPLAYER_STATE_ERROR     = 6
     */
    if (evId == 6) {
        g_status.state = PLAYER_ERROR;
    }
}

/* -----------------------------------------------------------------------
   Thread audio: estrae frame PCM e li invia all'hardware
   ----------------------------------------------------------------------- */
static int audio_thread(SceSize args, void *argp) {
    (void)args; (void)argp;

    while (g_audio_run) {
        if (!g_player) { sceKernelDelayThread(8000); continue; }

        SceAvPlayerFrameInfo fi;
        memset(&fi, 0, sizeof(fi));

        if (sceAvPlayerGetAudioData(g_player, &fi) == SCE_TRUE) {
            if (!fi.pData || fi.details.audio.sampleRate == 0) continue;

            /* Apri porta audio al primo frame valido */
            if (g_audio_port < 0) {
                int ch = (int)fi.details.audio.channelCount;
                if (ch < 1 || ch > 8) ch = 2;
                int mode = (ch == 1) ? SCE_AUDIO_OUT_MODE_MONO
                                     : SCE_AUDIO_OUT_MODE_STEREO;
                g_audio_port = sceAudioOutOpenPort(
                    SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                    AUDIO_GRAIN,
                    (int)fi.details.audio.sampleRate,
                    mode);
            }
            if (g_audio_port >= 0)
                sceAudioOutOutput(g_audio_port, fi.pData);
        } else {
            sceKernelDelayThread(4000);
        }
    }

    if (g_audio_port >= 0) {
        sceAudioOutReleasePort(g_audio_port);
        g_audio_port = -1;
    }
    return sceKernelExitDeleteThread(0);
}

/* -----------------------------------------------------------------------
   Helpers interni
   ----------------------------------------------------------------------- */
static void destroy_vtex(void) {
    if (g_vtex) { vita2d_free_texture(g_vtex); g_vtex = NULL; }
    g_vtex_w = g_vtex_h = 0;
}

static void close_player(void) {
    if (g_player) {
        sceAvPlayerStop(g_player);
        sceAvPlayerClose(g_player);
        g_player = 0;
    }
    if (g_audio_port >= 0) {
        sceAudioOutReleasePort(g_audio_port);
        g_audio_port = -1;
    }
    destroy_vtex();
    g_has_video = 0;
}

/* -----------------------------------------------------------------------
   API pubblica
   ----------------------------------------------------------------------- */
int player_init(void) {
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = PLAYER_STOPPED;

    g_audio_run  = 1;
    g_audio_thid = sceKernelCreateThread("avp_audio",
        audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    if (g_audio_thid < 0) return g_audio_thid;
    sceKernelStartThread(g_audio_thid, 0, NULL);
    return 0;
}

int player_play(const char *filepath) {
    close_player();

    SceAvPlayerInitData id;
    memset(&id, 0, sizeof(id));

    id.memoryReplacement.objectPointer     = NULL;
    id.memoryReplacement.allocate          = cb_alloc;
    id.memoryReplacement.deallocate        = cb_free;
    id.memoryReplacement.allocateTexture   = cb_gpu_alloc;
    id.memoryReplacement.deallocateTexture = cb_gpu_free;

    id.eventReplacement.objectPointer      = NULL;
    id.eventReplacement.eventCallback      = cb_event;

    id.basePriority               = 0xA0;
    id.numOutputVideoFrameBuffers = 2;
    id.autoStart                  = SCE_TRUE;

    g_player = sceAvPlayerInit(&id);
    if (g_player <= 0) {
        int err = (int)g_player;
        g_player = 0;
        g_status.state = PLAYER_ERROR;
        return err ? err : -1;
    }

    int ret = sceAvPlayerAddSource(g_player, filepath);
    if (ret < 0) {
        sceAvPlayerClose(g_player);
        g_player = 0;
        g_status.state = PLAYER_ERROR;
        return ret;
    }

    g_status.state       = PLAYER_PLAYING;
    g_status.position_ms = 0;
    g_status.duration_ms = 0;
    return 0;
}

void player_toggle_pause(void) {
    if (!g_player) return;
    if (g_status.state == PLAYER_PLAYING) {
        sceAvPlayerPause(g_player);
        g_status.state = PLAYER_PAUSED;
    } else if (g_status.state == PLAYER_PAUSED) {
        sceAvPlayerResume(g_player);
        g_status.state = PLAYER_PLAYING;
    }
}

void player_stop(void) {
    close_player();
    g_status.state       = PLAYER_STOPPED;
    g_status.position_ms = 0;
    g_status.duration_ms = 0;
}

PlayerState player_update(void) {
    if (!g_player) return g_status.state;
    if (g_status.state == PLAYER_ERROR) return PLAYER_ERROR;

    if (sceAvPlayerIsActive(g_player) == SCE_FALSE) {
        g_status.state = PLAYER_FINISHED;
        return PLAYER_FINISHED;
    }
    g_status.position_ms = (uint64_t)sceAvPlayerCurrentTime(g_player);
    return g_status.state;
}

void player_render_frame(void) {
    if (!g_player) return;

    SceAvPlayerFrameInfo fi;
    memset(&fi, 0, sizeof(fi));
    if (sceAvPlayerGetVideoData(g_player, &fi) != SCE_TRUE) return;
    if (!fi.pData) return;

    g_has_video = 1;
    int fw = (int)fi.details.video.width;
    int fh = (int)fi.details.video.height;
    if (fw <= 0 || fh <= 0) return;

    /* Ricrea texture se necessario */
    if (!g_vtex || g_vtex_w != fw || g_vtex_h != fh) {
        destroy_vtex();
        g_vtex = vita2d_create_empty_texture(fw, fh);
        if (!g_vtex) return;
        g_vtex_w = fw;
        g_vtex_h = fh;
    }

    /* YUV420 NV12 -> RGBA8888 (software) */
    uint32_t *dst = (uint32_t *)vita2d_texture_get_datap(g_vtex);
    if (!dst) return;

    const uint8_t *Y  = (const uint8_t *)fi.pData;
    const uint8_t *UV = Y + fw * fh;

#define C8(v) ((v) < 0 ? 0 : (v) > 255 ? 255 : (v))
    for (int y = 0; y < fh; y++) {
        for (int x = 0; x < fw; x++) {
            int luma = Y[y * fw + x];
            int cb   = UV[(y >> 1) * fw + (x & ~1)]     - 128;
            int cr   = UV[(y >> 1) * fw + (x & ~1) + 1] - 128;
            int r = luma + (cr * 1402) / 1000;
            int g = luma - (cb * 344)  / 1000 - (cr * 714) / 1000;
            int b = luma + (cb * 1772) / 1000;
            dst[y * fw + x] = RGBA8(C8(r), C8(g), C8(b), 255);
        }
    }
#undef C8

    float sx = (float)SCREEN_W / (float)fw;
    float sy = (float)SCREEN_H / (float)fh;
    vita2d_draw_texture_scale(g_vtex, 0.0f, 0.0f, sx, sy);
}

PlayerStatus player_get_status(void) { return g_status; }

void player_shutdown(void) {
    close_player();
    g_audio_run = 0;
    if (g_audio_thid >= 0) {
        sceKernelWaitThreadEnd(g_audio_thid, NULL, NULL);
        g_audio_thid = -1;
    }
    for (int i = 0; i < g_gpu_n; i++)
        sceKernelFreeMemBlock(g_gpu[i].uid);
    g_gpu_n = 0;
}
