#include "../src/host/snapshot_core.h"
#include "../src/host/snapshot_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} }while(0)
#ifdef SNAPSHOT_TEST_PATCHED
volatile const SnapshotManifest ah_snapshot_manifest={.magic={'A','H','S','M','A','P','0','2'},.version=2,.count=2,.total_bytes=32,.descriptor_crc=0x0f6236fc,.regions={{0x31000000,16},{0x31000020,16}}};
#endif
int main(void) {
    SnapshotBlob blob={0};
#ifndef SNAPSHOT_TEST_PATCHED
    CHECK(!snapshot_core_available());CHECK(!snapshot_core_signature());CHECK(!snapshot_core_capture(&blob));CHECK(!blob.data&&!blob.size);puts("snapshot_core: unpatched manifest refuses capture");
#else
    unsigned char* memory;
#ifdef _WIN32
    memory=VirtualAlloc((void*)(uintptr_t)0x31000000,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    memory=mmap((void*)(uintptr_t)0x31000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
#endif
    CHECK(memory==(unsigned char*)(uintptr_t)0x31000000);memset(memory,0x4a,4096);
    CHECK(snapshot_core_available());CHECK(snapshot_core_signature()==0x0f6236fc);CHECK(snapshot_core_capture(&blob));CHECK(blob.size==56);CHECK(snapshot_core_validate(&blob));
    for(unsigned i=0;i<24;++i) {blob.data[i]^=1;CHECK(!snapshot_core_validate(&blob));memset(memory,0x88,4096);CHECK(!snapshot_core_restore(&blob));for(unsigned j=0;j<4096;++j)CHECK(memory[j]==0x88);blob.data[i]^=1;}
    --blob.size;CHECK(!snapshot_core_restore(&blob));++blob.size;
    CHECK(snapshot_core_restore(&blob));for(unsigned i=0;i<4096;++i)CHECK(memory[i]==((i<16 || (i>=32&&i<48))?0x4a:0x88));
    free(blob.data);
#ifdef _WIN32
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
#else
    CHECK(!munmap(memory,4096));
#endif
    puts("snapshot_core: fixed descriptor capture, preflight rejection, gap preservation verified");
#endif
    return 0;
}
