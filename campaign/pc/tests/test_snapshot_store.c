#include "../src/host/room_snapshot.h"
#include "../src/host/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p,0700)
#define RMDIR(p) rmdir(p)
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int raw(const char* path,const unsigned char* p,size_t n) { FILE* f=pc_fopen(path,"wb"); if(!f)return 0; int ok=fwrite(p,1,n,f)==n; return fclose(f)==0&&ok; }
static unsigned rd(const unsigned char* p) { return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24; }
static void wr(unsigned char* p,unsigned v) { for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
static unsigned crc(const unsigned char* p,size_t n) { unsigned c=~0u;while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xEDB88320u:0);}return ~c; }
static void metadata_crc(unsigned char* p) { wr(p+52,0);wr(p+52,crc(p,64+rd(p+12)*24)); }
int main(int argc,char** argv) {
    const char* path="snapshot_store_test.ahs"; const char* bad="snapshot_store_bad.ahs";
    SnapshotImage image={0},out={0},sentinel; unsigned char digest[32],digest2[32];
    remove(path);remove(bad);
    CHECK(snapshot_executable_hash(digest)); CHECK(snapshot_executable_hash(digest2)); CHECK(!memcmp(digest,digest2,32));
    if(argc>1 && !strcmp(argv[1],"--hash")) { for(unsigned i=0;i<32;++i)printf("%02x",digest[i]);puts("");return 0; }
    memcpy(image.build,digest,32);image.count=3;
    for(unsigned i=0;i<3;++i) { image.parts[i].id=i+1;image.parts[i].blob.size=1024*1024+i*127;image.parts[i].blob.data=calloc(1,image.parts[i].blob.size);CHECK(image.parts[i].blob.data); }
    for(unsigned i=0;i<image.parts[1].blob.size;++i)image.parts[1].blob.data[i]=(unsigned char)(i*13+i/257);
    image.parts[2].blob.data[1000]=42;
    memset(&sentinel,0xDD,sizeof(sentinel));out=sentinel;
    CHECK(snapshot_store_read(path,digest,&out)==SNAP_MISSING);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    CHECK(snapshot_store_write(path,&image)==SNAP_OK);
    memset(&out,0,sizeof(out));CHECK(snapshot_store_read(path,digest,&out)==SNAP_OK);CHECK(out.count==3);
    for(unsigned i=0;i<3;++i) {CHECK(out.parts[i].id==i+1);CHECK(out.parts[i].blob.size==image.parts[i].blob.size);CHECK(!memcmp(out.parts[i].blob.data,image.parts[i].blob.data,image.parts[i].blob.size));}
    snapshot_image_free(&out);CHECK(!out.count);
    FILE* f=pc_fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&n<2*1024*1024);rewind(f);
    unsigned char* bytes=malloc((size_t)n+1);CHECK(bytes);CHECK(fread(bytes,1,(size_t)n,f)==(size_t)n);CHECK(!fclose(f));
    /* Every metadata byte and representative bytes in each stored section are protected. */
    for(long i=0;i<n;i+=(i<136?1:7919)) { bytes[i]^=1;CHECK(raw(bad,bytes,(size_t)n));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));bytes[i]^=1; }
    size_t offset=136;
    for(unsigned i=0;i<3;++i) {
        unsigned stored=rd(bytes+64+i*24+8);
        bytes[offset+stored/2]^=1;CHECK(raw(bad,bytes,(size_t)n));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));bytes[offset+stored/2]^=1;
        offset+=stored;
    }
    /* Recompute metadata CRC so semantic bounds checks must reject forged fields. */
    const unsigned fields[]={88,68,72,84,48};
    const unsigned values[]={1,0xFFFFFFFFu,0xFFFFFFFFu,2,1};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        unsigned old=rd(bytes+fields[i]);wr(bytes+fields[i],values[i]);metadata_crc(bytes);
        CHECK(raw(bad,bytes,(size_t)n));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
        wr(bytes+fields[i],old);metadata_crc(bytes);
    }
    /* A checksum-consistent malformed zero run cannot overrun its output buffer. */
    unsigned old_run=rd(bytes+137);wr(bytes+137,image.parts[0].blob.size+1);wr(bytes+80,crc(bytes+136,rd(bytes+72)));metadata_crc(bytes);
    CHECK(raw(bad,bytes,(size_t)n));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    wr(bytes+137,old_run);wr(bytes+80,crc(bytes+136,rd(bytes+72)));metadata_crc(bytes);
    CHECK(raw(bad,bytes,(size_t)n-1));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    bytes[n]=0;CHECK(raw(bad,bytes,(size_t)n+1));CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);
    digest2[0]^=1;CHECK(snapshot_store_read(path,digest2,&out)==SNAP_WRONG_BUILD);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    image.parts[1].id=1;CHECK(snapshot_store_write(path,&image)==SNAP_INVALID);image.parts[1].id=2;
    unsigned saved=image.parts[0].blob.size;image.parts[0].blob.size=0xFFFFFFFF;CHECK(snapshot_store_write(path,&image)==SNAP_INVALID);image.parts[0].blob.size=saved;
    CHECK(MKDIR("snapshot_store_dir")==0);CHECK(snapshot_store_write("snapshot_store_dir",&image)==SNAP_IO_ERROR);CHECK(RMDIR("snapshot_store_dir")==0);
    memset(&out,0,sizeof(out));CHECK(snapshot_store_read(path,digest,&out)==SNAP_OK);snapshot_image_free(&out);
    CHECK(raw(bad,(const unsigned char*)"AHQUICK1",8));out=sentinel;CHECK(snapshot_store_read(bad,digest,&out)==SNAP_INVALID);CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    snapshot_image_free(&image);free(bytes);remove(path);remove(bad);
    puts("snapshot_store: atomic multi-part compressed container, bounds, integrity, build gate verified");return 0;
}
