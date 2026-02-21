/* Stub per sceSharedFb* referenziate da vita2d internamente.
   Non usiamo la shared framebuffer (LiveArea overlay), quindi
   forniamo implementazioni vuote per soddisfare il linker. */

int sceSharedFbClose(int handle)
    { (void)handle; return 0; }
int _sceSharedFbOpen(int id, int index, void *info, int size)
    { (void)id; (void)index; (void)info; (void)size; return 0; }
int sceSharedFbGetInfo(int handle, void *info)
    { (void)handle; (void)info; return 0; }
int sceSharedFbEnd(int handle)
    { (void)handle; return 0; }
int sceSharedFbBegin(int handle, void *info)
    { (void)handle; (void)info; return 0; }
