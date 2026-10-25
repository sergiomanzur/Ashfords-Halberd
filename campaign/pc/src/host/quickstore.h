#ifndef ASHFORD_QUICKSTORE_H
#define ASHFORD_QUICKSTORE_H
#include "savestore.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { QUICKSTORE_BAD = -1, QUICKSTORE_IO = -2, QUICKSTORE_MISSING = 0, QUICKSTORE_OK = 1 };
/* One separate atomic slot; never shares typewriter or memory-card files.
   read leaves output unchanged on every failure. Payload is native SAVEFILE. */
int quickstore_write(const char* path, const unsigned char data[SAVE_DATA_BYTES]);
int quickstore_read(const char* path, unsigned char data[SAVE_DATA_BYTES]);
#ifdef __cplusplus
}
#endif
#endif
