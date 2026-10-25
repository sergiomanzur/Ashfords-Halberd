#include "gs/snapshot_codec.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void)
{
    RenderWriter w = {0}; SnapshotBlob blob = {0};
    rs_u32(&w, 0x1234); rs_write(&w,"abc",3);
    CHECK(rs_publish(&w,&blob));
    RenderReader r = {blob.data,blob.size,0,0}; char s[3];
    CHECK(rs_get_u32(&r)==0x1234); rs_read(&r,s,3);
    CHECK(!memcmp(s,"abc",3)); CHECK(rs_finish(&r));
    CHECK(rs_take(&r,1)==NULL); CHECK(!rs_finish(&r));
    r=(RenderReader){blob.data,blob.size,0,0};
    CHECK(rs_take(&r,UINT32_MAX)==NULL); CHECK(r.pos==0);
    free(blob.data);
    w=(RenderWriter){0}; w.size=RENDER_SNAPSHOT_MAX;
    rs_write(&w,"x",1); CHECK(w.bad); CHECK(!rs_publish(&w,&blob));
    puts("renderer snapshot codec: passed"); return 0;
}
