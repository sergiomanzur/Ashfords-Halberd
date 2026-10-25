/* Production camera routines with a small engine fixture. */
#include "../src/host/tps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { float x,y,z; } NJS_POINT3;
typedef struct { float mtx[16]; } Node;
typedef struct { Node* owP; unsigned obj_num; } Model;
typedef struct { Model* mlwP; float px,py,pz; int mode0,mode1,mode2; unsigned flg; } Player;
static struct { unsigned st_flg,gm_flg,cb_flg,sp_flg; } state,*sys=&state;
static Player player,*plp=&player;
static Node nodes[10];static Model model={nodes,10};
TpsConfig g_tps;
static int tps_on,tps_cam_active,fol_move,no_cam_walls,wall,wall_calls;
static float yaw,pitch,cur_dist,scale=1;
static NJS_POINT3 fol;
#define RAD(a) ((a)*(3.14159265f/32768.0f))
static float actor_cam_scale(void) {return scale;}
static void njCalcPoint(float (*m)[16],const NJS_POINT3* a,NJS_POINT3* b)
{b->x=(*m)[12]+a->x;b->y=(*m)[13]+a->y;b->z=(*m)[14]+a->z;}
static void clear_of_walls(const NJS_POINT3* a,NJS_POINT3* b)
{wall_calls++;if(wall)*b=*a;}
#include "first_person_test.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
#define CLOSE(a,b) (fabsf((a)-(b))<0.001f)
int main(void)
{
    NJS_POINT3 eye,dir,aim;
    plp->mlwP=&model;plp->px=10;plp->py=2;plp->pz=20;plp->mode0=1;
    nodes[5].mtx[13]=17;sys->sp_flg=0x41;
    g_tps.on=tps_on=2;g_tps.height=165;g_tps.shoulder=35;cur_dist=14;tps_cam_active=1;
    CHECK(camera_allowed() && tps_first_person_active());
    model.obj_num=5;CHECK(!camera_allowed());model.obj_num=10;
    model.owP=0;CHECK(!camera_allowed());model.owP=nodes;
    view(&eye,&dir,&aim);
    CHECK(CLOSE(eye.x,10) && CLOSE(eye.y,18.5f) && eye.z<20);
    CHECK(CLOSE(eye.x,aim.x) && CLOSE(eye.y,aim.y) && CLOSE(eye.z,aim.z));
    CHECK(CLOSE(dir.x,0) && CLOSE(dir.y,0) && CLOSE(dir.z,-1));
    yaw=16384;view(&eye,&dir,&aim);CHECK(eye.x<10 && CLOSE(eye.z,20));
    wall=1;view(&eye,&dir,&aim);CHECK(CLOSE(eye.x,10) && CLOSE(eye.z,20) && wall_calls>0);wall=0;
    plp->mode2=14;CHECK(!camera_allowed());plp->mode2=0;
    plp->mode0=2;CHECK(!camera_allowed());plp->mode0=1;
    plp->flg=0x4000000;CHECK(!camera_allowed());plp->flg=0;
    sys->sp_flg=0x40;CHECK(!camera_allowed());sys->sp_flg=0x41;
    sys->cb_flg=1;CHECK(!camera_allowed());sys->cb_flg=0;
    sys->st_flg=0x800000;CHECK(!camera_allowed());sys->st_flg=0;
    sys->gm_flg=0x40;CHECK(!camera_allowed());sys->gm_flg=0;
    plp->mode1=1;plp->mode2=65;CHECK(camera_allowed());
    tps_cam_active=0;CHECK(!tps_first_person_active());tps_cam_active=1;
    tps_on=1;plp->mode1=0;plp->mode2=14;CHECK(camera_allowed() && !tps_first_person_active());
    yaw=0;view(&eye,&dir,&aim);CHECK(CLOSE(eye.x,13.5f) && CLOSE(eye.z,34));
    puts("first-person eye, aim origin, collision, body gate and scripted fallbacks passed");return 0;
}
