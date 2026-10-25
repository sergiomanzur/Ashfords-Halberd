/* Original Ashford campaign launcher. No game data is opened here. */
#include "ashford_launcher.h"
#include "ps2_platform.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

enum { PLAY, CHOOSE, DISPLAY, MUSIC, QUIETER, LOUDER, QUIT, BUTTON_COUNT };
typedef struct {
    SDL_Mutex *lock;
    int pending, selected, error;
    char path[1024];
} DiscDialog;

static void SDLCALL disc_chosen(void *userdata, const char * const *files, int filter)
{
    DiscDialog *dialog = userdata;
    (void)filter;
    SDL_LockMutex(dialog->lock);
    dialog->error = files == NULL;
    if (files && files[0]) {
        if (SDL_strlen(files[0]) >= sizeof dialog->path) dialog->error = 2;
        else { SDL_strlcpy(dialog->path, files[0], sizeof dialog->path); dialog->selected = 1; }
    }
    dialog->pending = 0;
    SDL_UnlockMutex(dialog->lock);
}

static void color(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
{ SDL_SetRenderDrawColor(r, red, green, blue, alpha); }

static void box(SDL_Renderer *r, float x, float y, float w, float h,
                Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
{
    SDL_FRect rect = {x,y,w,h};
    color(r,red,green,blue,alpha);
    SDL_RenderFillRect(r,&rect);
}

static void text(SDL_Renderer *r, float x, float y, float scale, const char *s,
                 Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_SetRenderScale(r,scale,scale);
    color(r,red,green,blue,255);
    SDL_RenderDebugText(r,x/scale,y/scale,s);
    SDL_SetRenderScale(r,1,1);
}

static const SDL_FRect buttons[BUTTON_COUNT] = {
    {64,277,404,60}, {64,349,404,42}, {64,421,404,40},
    {64,479,272,40}, {348,479,54,40}, {414,479,54,40}, {64,581,404,40}
};

static int inside(float x,float y,const SDL_FRect *rect)
{ return x >= rect->x && x < rect->x+rect->w && y >= rect->y && y < rect->y+rect->h; }

static void button(SDL_Renderer *r,int id,int focus,int hover,int enabled,const char *label)
{
    SDL_FRect rect=buttons[id];
    int active=id==focus || id==hover;
    box(r,rect.x,rect.y,rect.w,rect.h,id==PLAY?86:24,id==PLAY?24:25,id==PLAY?30:29,240);
    color(r,enabled?(active?214:126):63,enabled?(active?178:105):61,enabled?(active?113:76):57,255);
    SDL_RenderRect(r,&rect);
    if(active && enabled) box(r,rect.x,rect.y,3,rect.h,204,168,103,255);
    text(r,rect.x+18,rect.y+(rect.h-16)/2,2,label,enabled?225:97,enabled?213:94,enabled?185:86);
}

static void asset_path(char *out,size_t capacity,const char *name)
{
    const char *base=SDL_GetBasePath();
    SDL_snprintf(out,capacity,"%sassets/branding/%s",base?base:"",name);
}

int ashford_launcher(char *iso,size_t capacity)
{
    SDL_Window *window=NULL;
    SDL_Renderer *renderer=NULL;
    SDL_Texture *art=NULL;
    SDL_AudioStream *music=NULL;
    Uint8 *wave=NULL;
    Uint32 wave_length=0;
    SDL_AudioSpec spec;
    SDL_Surface *surface;
    DiscDialog dialog={0};
    SDL_Event event;
    int running=1, result=0, focus=CHOOSE, muted=0, pending=0, exit_requested=0;
    int dragging=0;
    unsigned frame=0;
    const char *capture=SDL_getenv("ASHFORD_LAUNCHER_CAPTURE");
    const char *test_action=SDL_getenv("ASHFORD_LAUNCHER_TEST_ACTION");
    char path[2048], label[128], status[160]="Select your game disc to begin the campaign.";
    const SDL_DialogFileFilter filters[]={{"PlayStation 2 disc image","iso"}};
    if (!iso || capacity == 0) return 0;
    focus=iso[0]?PLAY:CHOOSE;
    window=SDL_CreateWindow("Ashford's Halberd | Campaign",1100,720,SDL_WINDOW_RESIZABLE);
    if(!window) goto cleanup;
    SDL_SetWindowMinimumSize(window,825,540);
    renderer=SDL_CreateRenderer(window,NULL);
    if(!renderer) goto cleanup;
    SDL_SetRenderVSync(renderer,1);
    SDL_SetRenderLogicalPresentation(renderer,1100,720,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    asset_path(path,sizeof path,"ashford-icon.bmp");
    surface=SDL_LoadBMP(path);
    if(surface) { SDL_SetWindowIcon(window,surface); SDL_DestroySurface(surface); }
    asset_path(path,sizeof path,"launcher.bmp");
    surface=SDL_LoadBMP(path);
    if(surface) { art=SDL_CreateTextureFromSurface(renderer,surface); SDL_DestroySurface(surface); }
    asset_path(path,sizeof path,"launcher.wav");
    if(SDL_LoadWAV(path,&spec,&wave,&wave_length)) {
        music=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
        if(music) {
            SDL_SetAudioStreamGain(music,(float)g_config.music_volume/100.0f);
            SDL_PutAudioStreamData(music,wave,(int)wave_length);
            SDL_ResumeAudioStreamDevice(music);
        }
    }
    dialog.lock=SDL_CreateMutex();
    if(!dialog.lock) goto cleanup;
    if(iso[0]) SDL_strlcpy(status,"Disc selected. Ready to begin.",sizeof status);
    while(running) {
        int action=-1, hover=-1;
        float mx,my,lx,ly;
        ++frame;
        SDL_LockMutex(dialog.lock);
        pending=dialog.pending;
        if(dialog.selected) {
            if(SDL_strlen(dialog.path)>=capacity) SDL_strlcpy(status,"The selected path is too long. Choose a shorter location.",sizeof status);
            else { SDL_strlcpy(iso,dialog.path,capacity); SDL_strlcpy(status,"Disc selected. Ready to begin.",sizeof status); focus=PLAY; }
            dialog.selected=0;
        }
        if(dialog.error) { SDL_strlcpy(status,dialog.error==2?"The selected path is too long. Choose a shorter location.":"The file picker could not open. Please try again.",sizeof status); dialog.error=0; }
        SDL_UnlockMutex(dialog.lock);
        if(exit_requested && !pending) break;
        while(SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(renderer,&event);
            if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID==SDL_GetWindowID(window))) exit_requested=1;
            if(pending) continue;
            if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch(event.key.key) {
                case SDLK_ESCAPE: exit_requested=1; break;
                case SDLK_TAB: focus=(focus+((event.key.mod & SDL_KMOD_SHIFT)?BUTTON_COUNT-1:1))%BUTTON_COUNT; break;
                case SDLK_DOWN: focus=(focus+1)%BUTTON_COUNT; break;
                case SDLK_UP: focus=(focus+BUTTON_COUNT-1)%BUTTON_COUNT; break;
                case SDLK_RETURN: case SDLK_SPACE: action=focus; break;
                case SDLK_LEFT: action=QUIETER; break;
                case SDLK_RIGHT: action=LOUDER; break;
                default: break;
                }
            }
            if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
                int i;
                for(i=0;i<BUTTON_COUNT;i++) if(inside(event.button.x,event.button.y,&buttons[i])) { focus=i; action=i; }
                if(event.button.x>=64 && event.button.x<=468 && event.button.y>=534 && event.button.y<=558) dragging=1;
            }
            if(event.type==SDL_EVENT_MOUSE_BUTTON_UP) dragging=0;
        }
        SDL_GetMouseState(&mx,&my);
        SDL_RenderCoordinatesFromWindow(renderer,mx,my,&lx,&ly);
        if(!pending) {
            int i;
            for(i=0;i<BUTTON_COUNT;i++) if(inside(lx,ly,&buttons[i])) hover=i;
            if(dragging) { g_config.music_volume=(int)((lx-64)*100/404); if(g_config.music_volume<0)g_config.music_volume=0; if(g_config.music_volume>100)g_config.music_volume=100; }
        }
        if(frame==15 && test_action && !pending) {
            if(!strcmp(test_action,"play")) action=PLAY;
            else if(!strcmp(test_action,"quit")) action=QUIT;
        }
        if(!exit_requested) switch(action) {
        case PLAY: if(iso[0]) {result=1;running=0;} break;
        case CHOOSE:
            SDL_LockMutex(dialog.lock); dialog.pending=1; SDL_UnlockMutex(dialog.lock);
            pending=1;
            SDL_ShowOpenFileDialog(disc_chosen,&dialog,window,filters,1,iso[0]?iso:NULL,false);
            break;
        case DISPLAY: g_config.fullscreen=!g_config.fullscreen; break;
        case MUSIC: muted=!muted; break;
        case QUIETER: g_config.music_volume=SDL_max(0,g_config.music_volume-10); break;
        case LOUDER: g_config.music_volume=SDL_min(100,g_config.music_volume+10); break;
        case QUIT: exit_requested=1; break;
        default: break;
        }
        if(music) {
            SDL_SetAudioStreamGain(music,muted?0.0f:(float)g_config.music_volume/100.0f);
            if(SDL_GetAudioStreamQueued(music)<(int)wave_length/2) SDL_PutAudioStreamData(music,wave,(int)wave_length);
        }
        color(renderer,10,12,15,255); SDL_RenderClear(renderer);
        if(art) { SDL_FRect dest={0,0,1100,720}; SDL_RenderTexture(renderer,art,NULL,&dest); }
        box(renderer,0,0,520,720,9,12,16,232);
        box(renderer,520,0,580,720,6,9,13,35);
        box(renderer,28,28,1044,1,153,124,76,255);
        box(renderer,28,691,1044,1,153,124,76,255);
        box(renderer,28,28,1,664,153,124,76,255);
        box(renderer,1071,28,1,664,153,124,76,255);
        box(renderer,520,28,1,664,100,83,53,180);
        text(renderer,64,65,1,"A S H F O R D   A R C H I V E S",173,145,96);
        text(renderer,64,112,4,"ASHFORD'S",232,220,191);
        text(renderer,64,153,4,"HALBERD",232,220,191);
        box(renderer,64,204,84,2,159,51,51,255);
        text(renderer,64,227,1,"THE CAMPAIGN  /  WINDOWS EDITION",174,169,156);
        button(renderer,PLAY,focus,hover,!pending&&iso[0],"PLAY CAMPAIGN");
        button(renderer,CHOOSE,focus,hover,!pending,pending?"CHOOSING DISC...":"CHOOSE ISO");
        SDL_snprintf(label,sizeof label,"DISPLAY: %s",g_config.fullscreen?"FULLSCREEN":"WINDOWED");
        button(renderer,DISPLAY,focus,hover,!pending,label);
        button(renderer,MUSIC,focus,hover,!pending,muted?"MUSIC: OFF":"MUSIC: ON");
        button(renderer,QUIETER,focus,hover,!pending,"-");
        button(renderer,LOUDER,focus,hover,!pending,"+");
        box(renderer,64,543,404,3,68,62,51,255);
        box(renderer,64,543,404*(float)g_config.music_volume/100,3,181,148,90,255);
        SDL_snprintf(label,sizeof label,"VOLUME %d%%",g_config.music_volume);
        text(renderer,64,557,1,label,167,156,132);
        button(renderer,QUIT,focus,hover,!pending,"QUIT");
        {
            char short_name[49];
            const char *name=strrchr(iso,'/'), *back=strrchr(iso,'\\');
            if(back && (!name || back>name)) name=back;
            name=name?name+1:iso;
            SDL_strlcpy(short_name,name,sizeof short_name);
            text(renderer,64,399,1,iso[0]?short_name:"No ISO selected",156,151,137);
        }
        /* Long status is wrapped explicitly, independent of UTF-8 path length. */
        text(renderer,64,641,1,pending?"Waiting for the disc picker...":(iso[0]?"Ready. Enter to launch; Tab to navigate.":"Choose a legally owned disc image to begin."),167,156,132);
        if(strstr(status,"long") || strstr(status,"could")) text(renderer,64,658,1,status,223,143,126);
        text(renderer,560,648,1,"AN ORIGINAL FAN PROJECT",198,179,142);
        text(renderer,560,667,1,"A quiet threshold. A familiar nightmare.",166,160,148);
        if(frame==5 && capture && capture[0]) {
            SDL_Surface *screenshot=SDL_RenderReadPixels(renderer,NULL);
            if(screenshot) { SDL_SaveBMP(screenshot,capture); SDL_DestroySurface(screenshot); }
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(12);
    }
cleanup:
    if(dialog.lock) SDL_DestroyMutex(dialog.lock);
    if(music) SDL_DestroyAudioStream(music);
    SDL_free(wave);
    if(art) SDL_DestroyTexture(art);
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    return result;
}
