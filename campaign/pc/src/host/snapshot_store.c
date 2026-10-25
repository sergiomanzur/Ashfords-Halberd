/* Full room snapshot container. Stored IDs describe sections, never addresses. */
#include "room_snapshot.h"
#include "fileio.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

enum { HEADER_BYTES = 64, ENTRY_BYTES = 24, MAX_PARTS = 8 };
#define MAX_BYTES UINT32_C(536870912)
static const unsigned char magic[8] = {'A','H','R','O','O','M','0','1'};
static uint32_t rd(const unsigned char* p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void wr(unsigned char* p,uint32_t v) { p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24); }
static void crc_table(uint32_t t[256]) {
    for(unsigned i=0;i<256;++i) { uint32_t c=i;for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?UINT32_C(0xEDB88320):0);t[i]=c; }
}
static uint32_t crc(const unsigned char* p,size_t n,const uint32_t t[256]) {
    uint32_t c=UINT32_C(0xFFFFFFFF);while(n--)c=t[(c^*p++)&255]^(c>>8);return ~c;
}
static int valid_image(const SnapshotImage* image) {
    uint32_t total=0,seen=0;
    if(!image || !image->count || image->count>MAX_PARTS)return 0;
    for(uint32_t i=0;i<image->count;++i) {
        const SnapshotPart* p=&image->parts[i];
        if(p->id<SNAP_CORE || p->id>SNAP_GLUE || (seen&(1u<<p->id)) || !p->blob.data || !p->blob.size || p->blob.size>MAX_BYTES-total)return 0;
        seen|=1u<<p->id;total+=p->blob.size;
    }
    return 1;
}
void snapshot_image_free(SnapshotImage* image) {
    if(!image)return;
    for(uint32_t i=0;i<image->count && i<MAX_PARTS;++i)free(image->parts[i].blob.data);
    memset(image,0,sizeof(*image));
}
/* Encoding 1: [tag:u8, length:le32, literal bytes if tag=1]. Zero runs
   shorter than 32 bytes remain literal; encoding 0 is a raw fallback. */
static uint32_t pack(const unsigned char* in,uint32_t n,unsigned char* out) {
    uint32_t at=0,literal=0,used=0;
    while(at<n) {
        if(in[at]) { ++at;continue; }
        uint32_t end=at;while(end<n && !in[end])++end;
        if(end-at<32) { at=end;continue; }
        if(at>literal) {
            if(out) { out[used]=1;wr(out+used+1,at-literal);memcpy(out+used+5,in+literal,at-literal); }
            used+=5+at-literal;
        }
        if(out) { out[used]=0;wr(out+used+1,end-at); }
        used+=5;at=literal=end;
    }
    if(at>literal) {
        if(out) { out[used]=1;wr(out+used+1,at-literal);memcpy(out+used+5,in+literal,at-literal); }
        used+=5+at-literal;
    }
    return used;
}
static int unpack(const unsigned char* in,uint32_t stored,unsigned char* out,uint32_t size) {
    uint32_t at=0,used=0;
    while(at<stored) {
        if(stored-at<5)return 0;
        unsigned tag=in[at];uint32_t n=rd(in+at+1);at+=5;
        if(!n || n>size-used || tag>1)return 0;
        if(tag) { if(n>stored-at)return 0;memcpy(out+used,in+at,n);at+=n; }
        else memset(out+used,0,n);
        used+=n;
    }
    return used==size;
}
typedef struct WriteContext {
    unsigned char metadata[HEADER_BYTES+MAX_PARTS*ENTRY_BYTES];
    const unsigned char* payload[MAX_PARTS];
    unsigned char* owned[MAX_PARTS];
    uint32_t size[MAX_PARTS],count;
} WriteContext;
static int write_file(FILE* f,void* ud) {
    const WriteContext* w=ud;size_t n=HEADER_BYTES+w->count*ENTRY_BYTES;
    if(fwrite(w->metadata,1,n,f)!=n)return -1;
    for(uint32_t i=0;i<w->count;++i)if(fwrite(w->payload[i],1,w->size[i],f)!=w->size[i])return -1;
    return 0;
}
int snapshot_store_write(const char* path,const SnapshotImage* image) {
    WriteContext w={0};uint32_t table[256],total=0;int result=SNAP_IO_ERROR;
    if(!path || !valid_image(image))return SNAP_INVALID;
    crc_table(table);w.count=image->count;memcpy(w.metadata,magic,8);wr(w.metadata+8,1);wr(w.metadata+12,w.count);memcpy(w.metadata+16,image->build,32);
    for(uint32_t i=0;i<w.count;++i) {
        const SnapshotPart* p=&image->parts[i];unsigned char* e=w.metadata+HEADER_BYTES+i*ENTRY_BYTES;
        uint32_t packed=pack(p->blob.data,p->blob.size,NULL),encoding=0;
        w.payload[i]=p->blob.data;w.size[i]=p->blob.size;
        if(packed<p->blob.size) {
            w.owned[i]=malloc(packed);if(!w.owned[i])goto done;
            pack(p->blob.data,p->blob.size,w.owned[i]);w.payload[i]=w.owned[i];w.size[i]=packed;encoding=1;
        }
        wr(e,p->id);wr(e+4,p->blob.size);wr(e+8,w.size[i]);wr(e+12,crc(p->blob.data,p->blob.size,table));wr(e+16,crc(w.payload[i],w.size[i],table));wr(e+20,encoding);total+=p->blob.size;
    }
    wr(w.metadata+48,total);wr(w.metadata+52,crc(w.metadata,HEADER_BYTES+w.count*ENTRY_BYTES,table));
    result=pc_save_file(path,"wb",write_file,&w)==0?SNAP_OK:SNAP_IO_ERROR;
done:
    for(uint32_t i=0;i<w.count;++i)free(w.owned[i]);
    return result;
}
int snapshot_store_read(const char* path,const unsigned char build[32],SnapshotImage* out) {
    unsigned char metadata[HEADER_BYTES+MAX_PARTS*ENTRY_BYTES];uint32_t table[256],count,total=0,stored_total=0,seen=0;SnapshotImage staged={0};FILE* f;int result=SNAP_INVALID;
    if(!path || !build || !out)return SNAP_INVALID;
    errno=0;f=pc_fopen(path,"rb");if(!f)return errno==ENOENT?SNAP_MISSING:SNAP_IO_ERROR;
    if(fread(metadata,1,HEADER_BYTES,f)!=HEADER_BYTES)goto done;
    count=rd(metadata+12);
    if(memcmp(metadata,magic,8) || rd(metadata+8)!=1 || !count || count>MAX_PARTS || rd(metadata+56) || rd(metadata+60))goto done;
    if(fread(metadata+HEADER_BYTES,ENTRY_BYTES,count,f)!=count)goto done;
    crc_table(table);
    uint32_t expected=rd(metadata+52);wr(metadata+52,0);
    if(crc(metadata,HEADER_BYTES+count*ENTRY_BYTES,table)!=expected)goto done;
    for(uint32_t i=0;i<count;++i) {
        const unsigned char* e=metadata+HEADER_BYTES+i*ENTRY_BYTES;uint32_t id=rd(e),raw=rd(e+4),stored=rd(e+8),encoding=rd(e+20);
        if(id<SNAP_CORE || id>SNAP_GLUE || (seen&(1u<<id)) || !raw || raw>MAX_BYTES-total || !stored || stored>raw || encoding>1 || (!encoding && stored!=raw))goto done;
        seen|=1u<<id;total+=raw;stored_total+=stored;
    }
    if(total!=rd(metadata+48))goto done;
    /* Reject impossible/truncated lengths before allocating section buffers. */
    if(fseek(f,0,SEEK_END)!=0) { result=SNAP_IO_ERROR;goto done; }
    long file_size=ftell(f);
    if(file_size<0) { result=SNAP_IO_ERROR;goto done; }
    if((unsigned long)file_size!=HEADER_BYTES+count*ENTRY_BYTES+stored_total)goto done;
    if(fseek(f,HEADER_BYTES+count*ENTRY_BYTES,SEEK_SET)!=0) { result=SNAP_IO_ERROR;goto done; }
    staged.count=count;memcpy(staged.build,metadata+16,32);
    for(uint32_t i=0;i<count;++i) {
        const unsigned char* e=metadata+HEADER_BYTES+i*ENTRY_BYTES;SnapshotPart* p=&staged.parts[i];uint32_t stored=rd(e+8);unsigned char* packed=NULL;
        p->id=rd(e);p->blob.size=rd(e+4);p->blob.data=malloc(p->blob.size);if(!p->blob.data) { result=SNAP_IO_ERROR;goto done; }
        if(rd(e+20)) { packed=malloc(stored);if(!packed) { result=SNAP_IO_ERROR;goto done; } }
        unsigned char* bytes=packed?packed:p->blob.data;
        int ok=fread(bytes,1,stored,f)==stored && crc(bytes,stored,table)==rd(e+16);
        if(ok && packed)ok=unpack(packed,stored,p->blob.data,p->blob.size);
        free(packed);
        if(!ok || crc(p->blob.data,p->blob.size,table)!=rd(e+12))goto done;
    }
    if(fgetc(f)!=EOF)goto done;
    result=memcmp(build,staged.build,32)?SNAP_WRONG_BUILD:SNAP_OK;
done:
    if(ferror(f))result=SNAP_IO_ERROR;
    if(fclose(f)!=0)result=SNAP_IO_ERROR;
    if(result==SNAP_OK) { *out=staged;memset(&staged,0,sizeof(staged)); }
    snapshot_image_free(&staged);return result;
}

/* SHA-256, streaming over the actual executable file. No timestamps, build paths,
   module addresses or partial-file identity are accepted as a build gate. */
typedef struct Sha256 { uint32_t h[8];uint64_t bytes;unsigned char block[64];size_t used; } Sha256;
static uint32_t ror(uint32_t x,unsigned n) { return (x>>n)|(x<<(32-n)); }
static void sha_block(Sha256* s,const unsigned char* p) {
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];for(unsigned i=0;i<16;++i)w[i]=(uint32_t)p[i*4]<<24|(uint32_t)p[i*4+1]<<16|(uint32_t)p[i*4+2]<<8|p[i*4+3];
    for(unsigned i=16;i<64;++i) { uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(ror(a,7)^ror(a,18)^(a>>3))+w[i-7]+(ror(b,17)^ror(b,19)^(b>>10)); }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;++i) { uint32_t t=h+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^(~e&g))+k[i]+w[i],u=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u; }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha_update(Sha256* s,const unsigned char* p,size_t n) {
    s->bytes+=n;
    while(n) { size_t k=64-s->used;if(k>n)k=n;memcpy(s->block+s->used,p,k);s->used+=k;p+=k;n-=k;if(s->used==64) { sha_block(s,s->block);s->used=0; } }
}
static void sha_finish(Sha256* s,unsigned char out[32]) {
    uint64_t bits=s->bytes*8;unsigned char tail[128]={0x80};size_t n=s->used<56?56-s->used:120-s->used;
    for(unsigned i=0;i<8;++i)tail[n+i]=(unsigned char)(bits>>(56-i*8));
    sha_update(s,tail,n+8);
    for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=(unsigned char)(s->h[i]>>(24-j*8));
}
int snapshot_executable_hash(unsigned char out[32]) {
    FILE* f=NULL;unsigned char bytes[65536],digest[32];size_t n;
    Sha256 s={{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19},0,{0},0};
    if(!out)return 0;
#ifdef _WIN32
    wchar_t* path=malloc(32768*sizeof(wchar_t));if(!path)return 0;
    DWORD len=GetModuleFileNameW(NULL,path,32768);
    if(len && len<32768)f=_wfopen(path,L"rb");
    free(path);
#else
    f=fopen("/proc/self/exe","rb");
#endif
    if(!f)return 0;
    while((n=fread(bytes,1,sizeof(bytes),f))>0)sha_update(&s,bytes,n);
    int ok=!ferror(f);if(fclose(f)!=0)ok=0;if(!ok)return 0;
    sha_finish(&s,digest);memcpy(out,digest,sizeof(digest));return 1;
}
