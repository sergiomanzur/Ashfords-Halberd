/* Exercise production policy/lifecycle; model only external decoder and fade services. */
#define TARGET_PC 1
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef struct { int sSizeX,sSizeY,dPosX,dPosY,dSizeX,dSizeY,DispType; } MOV_DEF;
typedef struct { int ftype,dtype,max_bps,max_width,max_height,nfrm_pool_wk,wksize; } MWS_PLY_CPRM_SFD;
static struct {
    int ExecMovieSystemFlag,MovieSystemLastError,MovieCancelFlag,MovieFadeFlag,MovieFadeMode,FrameCnt;
    void* mmp; float Vol,VolSpeed,Fade,FadeSpeed;
} MovieInfo;
static struct { unsigned cb_flg; float fade_an; } state,*sys=&state;
static int PcAnyKeyMovie,OpenDriveTrayFlag,AdxDef,skip,decoding,stops,fade_calls,skip_time,failures,checks;
static char arena[16];
static int mwPlyCalcWorkSofdec(int a,int b,int c,int d,int e) {return a+b+c+d+e;}
static void* bhGetFreeMemory(int a,size_t b) {(void)a;(void)b;return arena;}
static void StopBgm(int a) {(void)a;} static void StopVoice(int a) {(void)a;}
static void SleepAdxStream(void) {}
static void PlayMw2(const char* n,int a,void* p,MWS_PLY_CPRM_SFD* c,int pause) {(void)n;(void)a;(void)p;(void)c;(void)pause;}
static void SetSfdDislpaySize(int a,int b,int c,int d) {(void)a;(void)b;(void)c;(void)d;}
static int pc_movie_skip_pressed(int start) {if(start)skip=0;return 0;}
static int pc_intro_pressed(int start) {(void)start;return 0;}
static float GetSfdFadeRate(void) {return 1;}
static void StopMw(void) {stops++;}
static void bhReleaseFreeMemory(void* p) {(void)p;}
static void WakeupAdxStream(int a) {(void)a;}
static int CheckSoftResetKeyFlag(int a) {(void)a;return 0;}
static void mwPlyStartFrame(void) {}
static int PcMovieCancelPressed(void) {int r=skip;skip=0;return r;}
static void bhSetScreenFade(unsigned color,int frames) {(void)color;(void)frames;fade_calls++;sys->cb_flg|=2;}
static void bhControlScreenFade(void) {sys->fade_an=255;sys->cb_flg&=~2u;}
static void bhDrawScreenFade(void) {}
static void SetMwVolume(float volume) {(void)volume;}
static int PlayMwMain(void) {return decoding;}
static void PcMovieSkipTime(void) {skip_time++;}
#include "movie_return_test.inc"
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
static void reset(void)
{memset(&MovieInfo,0,sizeof(MovieInfo));memset(sys,0,sizeof(*sys));skip=stops=fade_calls=skip_time=0;decoding=1;}
static int cancel(void)
{int result;skip=1;result=PlayMovieMain();for(int i=0;i<32&&MovieInfo.ExecMovieSystemFlag;i++)result=PlayMovieMain();return result;}
int main(void)
{
    /* Graveyard MV_001 resumes directly: the script has no matching fade-in. */
    reset();CHECK(PlayStartMovieEx(1,0,0)==0);CHECK(MovieInfo.MovieCancelFlag);
    CHECK(cancel()==2 && stops==1 && !MovieInfo.ExecMovieSystemFlag);
    CHECK(sys->fade_an==0 && fade_calls==0);
    /* Preserve each native movie's fade contract, plus the explicit endings. */
    for(int id=0;id<22;id++) {
        reset();CHECK(PlayStartMovieEx(id,0,0)==0);
        int expects_fade=(MovieTypeDef[id]&0x40)!=0 || id==15 || id==21;
        CHECK(MovieInfo.MovieFadeFlag==expects_fade && MovieInfo.MovieCancelFlag==1);
        CHECK(cancel()==2 && stops==1);
        CHECK(fade_calls==expects_fade && sys->fade_an==(expects_fade?255:0));
    }
    /* Caller-requested fades still belong to the scene; never clear them globally. */
    reset();CHECK(PlayStartMovieEx(1,2,0)==0);CHECK(cancel()==2 && fade_calls==1 && sys->fade_an==255);
    reset();CHECK(PlayStartMovieEx(1,0,0)==0);sys->fade_an=128;decoding=0;
    CHECK(PlayMovieMain()==1 && stops==1 && sys->fade_an==128 && fade_calls==0);
    reset();CHECK(PlayStartMovieEx(1,0,0)==0);decoding=0;
    CHECK(PlayMovieMain()==1 && sys->fade_an==0 && fade_calls==0);
    fprintf(stderr,"%d movie return checks, %d failures\n",checks,failures);return failures!=0;
}
