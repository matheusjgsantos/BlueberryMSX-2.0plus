#include <stdlib.h>
void ym2413Create() {}
void ym2413Destroy() {}
void ym2413Reset() {}
void ym2413WriteAddress(int a, int b) {}
void ym2413WriteData(int a, int b) {}
void ym2413LoadState() {}
void ym2413SaveState() {}
void ym2413GetDebugInfo() {}
void moonsoundReset() {}
void moonsoundGetDebugInfo() {}
void moonsoundPeek() {}
void moonsoundCreate() {}
void moonsoundDestroy() {}
int romMapperMoonsoundCreate(const char* filename, unsigned char* romData, int size, int sramSize) { free(romData); return 1; }
int romMapperMsxMusicCreate(const char* filename, unsigned char* romData, int size, int slot, int subslot, int startPage) { return 1; }
