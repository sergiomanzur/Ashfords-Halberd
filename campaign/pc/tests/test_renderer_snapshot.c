/* Isolated GPU fixture uses production implementation to inspect private RTs;
   this avoids adding test-only exports to the game. No assets or visible window. */
#include <stdarg.h>
#include "../src/gs/gs_gl.c"
#include "../src/ps2/vu0.h"
SDL_Window *g_window;
HostConfig g_config;
void host_log(const char *fmt, ...) { (void)fmt; }
void host_fatal(const char *fmt, ...) { va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);abort(); }
void prof_push(int id) { (void)id; } void prof_pop(void) {}
int hdpack_active(void) { return 0; }
void hdpack_note(unsigned int t,uint64_t a,uint64_t b,int f) { (void)t;(void)a;(void)b;(void)f; }
unsigned int hdpack_bind(unsigned int t,float *s) { (void)s;return t; }
void hdpack_forget(unsigned int t) { (void)t; } void hdpack_frame(int f) { (void)f; }
void pc_view_changed(void) {} int pc_frame_fade(void) { return 0; }
uint64_t pc_frame_view(float v[6],float c[STREAM_CAM]) { memset(v,0,6*sizeof(float));memset(c,0,STREAM_CAM*sizeof(float));return 0; }
void glue_debug_frame(uint64_t f) { (void)f; }
void overlay_render(void) {}
void replay_check_compare(uint64_t f,uint8_t *a,uint8_t *b,int w,int h) { (void)f;(void)a;(void)b;(void)w;(void)h; }
const uint8_t *movie_lock_frame(int i,uint32_t *s,int *w,int *h,int *stride) { (void)i;(void)s;(void)w;(void)h;(void)stride;return NULL; }
void movie_unlock_frame(void) {}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static GLuint fixture_texture(int w,int h,int depth,float value)
{
    GLuint t;float p[8*8*4];for(int i=0;i<w*h*(depth?1:4);++i)p[i]=value;
    glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,depth?GL_DEPTH_COMPONENT32F:GL_RGBA16F,w,h,0,depth?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,p);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);return t;
}
static void expect_texture(GLuint t,int depth,float value)
{
    float p[8*8*4]={0};CHECK(snapshot_gl_functions());glBindTexture(GL_TEXTURE_2D,t);
    snapshot_tex_image(GL_TEXTURE_2D,0,depth?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,p);
    CHECK(glGetError()==0);CHECK(p[0]==value && p[1]==value);
}
static void seed(void)
{
    gs_reset();for(unsigned i=0;i<GS_MEM_SIZE;++i)gs_mem[i]=(unsigned char)(i*17+9);
    gs.rgbaq=0x123456789abcdef0ull;gs.texa=0x80;gs.disp_fbp=7;gs.disp_fbw=1;
    g_vu0.vf[9].u[3]=0x7f123456;g_vu1.mem[777].u[2]=0x10203040;
    uint32_t row[]={0x30000000u,0x11111111u,0x22222222u};ps2_vif1_transfer_words(row,3);
    uint32_t tag[]={1,0x10000000u,0xe,0};gs_gif_transfer(3,tag,1);
    RenderTarget *rt=&rts[0];memset(rt,0,sizeof(*rt));rt->used=1;rt->fbp=7;rt->fbw=1;
    rt->vw=rt->vh=rt->pw=rt->ph=8;rt->zbp=4;rt->has_box=1;rt->bx1=rt->by1=7;rt->tex=fixture_texture(8,8,0,.625f);
    glGenFramebuffers(1,&rt->fbo);glBindFramebuffer(GL_FRAMEBUFFER,rt->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,rt->tex,0);
    dz[0].zbp=4;dz[0].pw=dz[0].ph=8;dz[0].last=42;dz[0].tex=fixture_texture(8,8,1,.375f);dz_clock=42;frame_no=99;
    img[0].tex=fixture_texture(8,8,0,.5f);img[0].pw=img[0].ph=8;
    glGenFramebuffers(1,&img[0].fbo);glBindFramebuffer(GL_FRAMEBUFFER,img[0].fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,img[0].tex,0);final_vw=final_vh=8;fin=0;
    uint32_t data[4]={0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};
    ntex_define_begin(0x10001,32,64,1,NTEX_KEEP);
    ntex_define_reg(0x50,((uint64_t)32<<32)|((uint64_t)1<<48));ntex_define_reg(0x51,0);
    ntex_define_reg(0x52,2|((uint64_t)2<<32));ntex_define_reg(0x53,0);
    ntex_define_image((unsigned char*)data,sizeof(data));ntex_define_end();ntex_bind(0x10001,128,160);
    ntex_define_capture(0x10002,fixture_texture(4,4,0,.75f),2,2);ntex_bind(0x10002,256,288);
    glBindFramebuffer(GL_FRAMEBUFFER,0);CHECK(glGetError()==0);
}
static void roundtrip(void)
{
    SnapshotBlob blob={0};GsState saved=gs;RenderWriter memory_before={0};gs_memory_snapshot_write(&memory_before);
    CHECK(gs_snapshot_capture(&blob));CHECK(gs_snapshot_validate(&blob));
    SnapshotBlob truncated=blob;truncated.size--;CHECK(!gs_snapshot_validate(&truncated));CHECK(!gs_snapshot_restore(&truncated));
    CHECK(!memcmp(&gs,&saved,sizeof(gs)));
    uint32_t original;memcpy(&original,blob.data,4);memset(blob.data,0,4);CHECK(!gs_snapshot_restore(&blob));memcpy(blob.data,&original,4);
    RenderReader r={blob.data,blob.size,sizeof(RendererDisk)+sizeof(GsState),0};
    gs_memory_snapshot_read(&r,0);gs_parser_snapshot_read(&r,0);vif_snapshot_read(&r,0);stream_snapshot_read(&r,0);
    uint32_t native_offset=r.pos;memcpy(&original,blob.data+native_offset,4);memset(blob.data+native_offset,0xff,4);
    CHECK(!gs_snapshot_validate(&blob));memcpy(blob.data+native_offset,&original,4);CHECK(ntex_snapshot_read(&r,0)!=NULL);
    uint32_t target_offset=r.pos,invalid=123;memcpy(blob.data+target_offset+offsetof(RenderTarget,tex),&invalid,4);
    CHECK(!gs_snapshot_validate(&blob));invalid=0;memcpy(blob.data+target_offset+offsetof(RenderTarget,tex),&invalid,4);CHECK(gs_snapshot_validate(&blob));
    expect_texture(rts[0].tex,0,.625f);
    ntex_forget(0x10001);ntex_forget(0x10002);
    ntex_define_capture(0x20002,fixture_texture(4,4,0,.125f),2,2);ntex_bind(0x20002,256,288);
    uint32_t wrong_payload[]={0,0,1,0};gs_gif_transfer(3,wrong_payload,1);
    stream_submit();Frame *discard=stream_pop(0);CHECK(discard);stream_exec(discard);stream_release(discard);
    memset(gs_mem,0,GS_MEM_SIZE);memset(&gs,0,sizeof(gs));g_vu0.vf[9].u[3]=0;g_vu1.mem[777].u[2]=0;
    uint8_t wrong_clut[GS_CLUT_STATE_SIZE];memset(wrong_clut,0x5a,sizeof(wrong_clut));gs_clut_state_restore(wrong_clut);
    uint32_t wrong_row[]={0xaaaaaaaa,0xbbbbbbbb};ps2_vif1_transfer_words(wrong_row,2);
    uint32_t more_row[]={0x33333333,0x44444444};
    g_config.brightness=37;g_config.contrast=148;g_config.gamma=13;g_config.saturation=170;g_config.antialias=1;
    snapshot_valid=start_valid=batch_native.valid=ncache_on_gpu=1;
    CHECK(gs_snapshot_restore(&blob));CHECK(!memcmp(&gs,&saved,sizeof(gs)));
    CHECK(gs_mem[0]==9 && gs_mem[991]==(unsigned char)(991*17+9));
    RenderWriter memory_after={0};gs_memory_snapshot_write(&memory_after);
    CHECK(memory_before.size==memory_after.size && !memcmp(memory_before.data,memory_after.data,memory_before.size));
    free(memory_before.data);free(memory_after.data);
    CHECK(g_vu0.vf[9].u[3]==0x7f123456 && g_vu1.mem[777].u[2]==0x10203040);
    CHECK(g_config.brightness==37 && g_config.contrast==148 && g_config.gamma==13 && g_config.saturation==170 && g_config.antialias==1);
    CHECK(!snapshot_valid && !start_valid && !batch_native.valid && !ncache_on_gpu);CHECK(frame_no==99 && dz_clock==42);
    expect_texture(rts[0].tex,0,.625f);expect_texture(dz[0].tex,1,.375f);expect_texture(img[0].tex,0,.5f);
    GLuint t=0;int w=0,h=0;uint64_t captured=256|((uint64_t)1<<14)|((uint64_t)1<<26)|((uint64_t)1<<30);
    CHECK(ntex_lookup(captured,gs.texa,&t,&w,&h)==2);CHECK(w==2 && h==2);expect_texture(t,0,.75f);
    uint64_t native=128|((uint64_t)1<<14)|((uint64_t)1<<26)|((uint64_t)1<<30);
    CHECK(ntex_lookup(native,gs.texa,&t,&w,&h)==1);CHECK(w==2 && h==2);
    unsigned char rgba[16];glBindTexture(GL_TEXTURE_2D,t);snapshot_tex_image(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    CHECK(rgba[0]==255 && rgba[1]==0 && rgba[2]==0 && rgba[4]==0 && rgba[5]==255);
    ps2_vif1_transfer_words(more_row,2);
    uint32_t unpack[]={0x05000001,0x01000101,0x6c010020,0,0,0,0};ps2_vif1_transfer_words(unpack,7);
    CHECK(g_vu1.mem[32].u[0]==0x11111111 && g_vu1.mem[32].u[1]==0x22222222 &&
          g_vu1.mem[32].u[2]==0x33333333 && g_vu1.mem[32].u[3]==0x44444444);
    uint32_t payload[]={0x11223344,0x55667788,1,0};gs_gif_transfer(3,payload,1);
    stream_submit();Frame *frame=stream_pop(0);CHECK(frame);stream_exec(frame);stream_release(frame);
    CHECK(gs.rgbaq==0x5566778811223344ull);free(blob.data);CHECK(glGetError()==0);
}
static SDL_AtomicInt barrier_done;
static SnapshotBlob barrier_blob;
static int barrier_result, barrier_op;
static int SDLCALL barrier_game_thread(void *unused)
{
    (void)unused;
    barrier_result=barrier_op==1 ? snapshot_render_capture(&barrier_blob) : snapshot_render_restore(&barrier_blob);
    SDL_SetAtomicInt(&barrier_done,1);return 0;
}
static void service_barrier(int op)
{
    barrier_op=op;SDL_SetAtomicInt(&barrier_done,0);
    SDL_Thread *thread=SDL_CreateThread(barrier_game_thread,"snapshot game fixture",NULL);CHECK(thread);
    uint64_t deadline=SDL_GetTicks()+10000;int restored=0;
    while (!SDL_GetAtomicInt(&barrier_done)) {
        Frame *f=stream_pop(0);
        if (f) { gs_frame_begin();stream_exec(f);if (!stream_is_partial(f))gs_frame_executed(0);stream_release(f); }
        restored|=stream_snapshot_service()==2;
        CHECK(SDL_GetTicks()<deadline);SDL_Delay(1);
    }
    SDL_WaitThread(thread,NULL);CHECK(barrier_result);CHECK(op!=2 || restored);
}
static void barriers(void)
{
    /* A full queued game frame plus the following partial draw environment must
       both execute before capture acknowledges the synchronous game request. */
    stream_reg(0x3b,777);stream_submit();stream_reg(0x3b,0x1234);
    service_barrier(1);CHECK(gs.texa==0x1234);CHECK(snapshot_render_validate(&barrier_blob));
    gs.texa=99;stream_reg(0x3b,666);service_barrier(2);CHECK(gs.texa==0x1234);
    CHECK(!stream_queued());CHECK(!snapshot_valid);free(barrier_blob.data);
    SnapshotBlob rejected={0};ntex_define_begin(0x30003,0,0,1,0);
    CHECK(!gs_snapshot_capture(&rejected));CHECK(!rejected.data);
    ntex_define_end();ntex_forget(0x30003);
    NativeVertex tri[3];memset(tri,0,sizeof(tri));native_defer(3,tri,3,1,0);
    CHECK(!gs_snapshot_capture(&rejected));CHECK(!rejected.data);
    stream_reg(0x3b,gs.texa);stream_submit();Frame *f=stream_pop(0);CHECK(f);stream_release(f);
    CHECK(glGetError()==0);
}
int main(void)
{
    CHECK(SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    g_window=SDL_CreateWindow("Renderer snapshot test",8,8,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(g_window);
    SDL_GLContext context=SDL_GL_CreateContext(g_window);CHECK(context);
    g_config.scale=1;g_config.contrast=100;g_config.gamma=10;g_config.saturation=100;
    gs_renderer_init();stream_init();seed();roundtrip();barriers();
    SDL_GL_DestroyContext(context);SDL_DestroyWindow(g_window);SDL_Quit();
    puts("renderer snapshot: real GPU/native-definition/parser roundtrip passed");return 0;
}
