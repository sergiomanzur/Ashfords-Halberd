/* Real production presentation shaders, exercised on an isolated hidden GL context. */
#include <SDL3/SDL.h>
#include "render/gl_loader.h"
#include "gs/picture_shader.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 32
static GLuint program, input_tex, output_tex, fbo, vao;
static int failures;
static unsigned char pixels[N * N * 4];
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); ++failures; } } while (0)

static GLuint shader(GLenum type, const char *source)
{
    GLuint s = glCreateShader(type);
    GLint ok;
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(s, sizeof(log), NULL, log); fprintf(stderr, "%s\n", log); exit(1); }
    return s;
}

static void render(const float *data, int size, float saturation, int aa,
                   float brightness, float contrast, float invgamma, int tone,
                   float x0, float y0, float x1, float y1)
{
    glBindTexture(GL_TEXTURE_2D, input_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, 0x881A /* GL_RGBA16F */, size, size, 0, GL_RGBA, GL_FLOAT, data);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "u_tex"), 0);
    glUniform1f(glGetUniformLocation(program, "u_saturation"), saturation);
    glUniform1i(glGetUniformLocation(program, "u_antialias"), aa);
    glUniform4f(glGetUniformLocation(program, "u_pic"), brightness, contrast, invgamma, (float)tone);
    glUniform4f(glGetUniformLocation(program, "u_rect"), x0, y0, x1, y1);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, N, N);
    glDrawArrays(0x0005, 0, 4);
    glReadPixels(0, 0, N, N, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    CHECK(glGetError() == 0, "presentation produces no GL errors");
}

static void fill(float *data, int size, float r, float g, float b)
{
    for (int i = 0; i < size * size; ++i) {
        data[4*i] = r; data[4*i+1] = g; data[4*i+2] = b; data[4*i+3] = .25f;
    }
}

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static void expect_color(float r, float g, float b, const char *name)
{
    float expected[3] = {r, g, b};
    for (int i = 0; i < N*N; ++i) for (int c = 0; c < 4; ++c) {
        int want = c == 3 ? 255 : (int)roundf(clamp01(expected[c]) * 255);
        if (abs((int)pixels[4*i+c] - want) > 1) { CHECK(0, name); return; }
    }
}

static void colors(void)
{
    float data[N*N*4], r=.8f, g=.4f, b=.2f;
    float y=.2126f*r+.7152f*g+.0722f*b;
    fill(data, N, r, g, b);
    render(data,N,1,0,0,1,1,0,0,0,1,1);
    expect_color(r,g,b,"neutral saturation preserves RGB and presentation alpha");
    render(data,N,1,1,0,1,1,0,0,0,1,1);
    expect_color(r,g,b,"AA preserves flat colored regions");
    render(data,N,0,0,0,1,1,0,0,0,1,1);
    expect_color(y,y,y,"zero saturation uses Rec.709 gray");
    render(data,N,2,0,0,1,1,0,0,0,1,1);
    expect_color(2*r-y,2*g-y,2*b-y,"200 percent saturation increases chroma and clamps");
    render(data,N,0,0,.1f,1.2f,.5f,1,0,0,1,1);
    r=clamp01((sqrtf(r)-.5f)*1.2f+.6f);
    g=clamp01((sqrtf(g)-.5f)*1.2f+.6f);
    b=clamp01((sqrtf(b)-.5f)*1.2f+.6f);
    y=.2126f*r+.7152f*g+.0722f*b;
    expect_color(y,y,y,"saturation follows existing tone correction");
    render(data,N,1,0,0,1,1,0,0,0,1,1);
    expect_color(.8f,.4f,.2f,"reset restores original picture");
    fill(data,N,.375f,.375f,.375f);
    for (int aa=0; aa<=1; ++aa) for (int sat=0; sat<=2; ++sat) {
        render(data,N,(float)sat,aa,0,1,1,0,0,0,1,1);
        expect_color(.375f,.375f,.375f,"gray and flat regions remain unchanged");
    }
    fill(data,N,1.5f,-.5f,.5f);
    render(data,N,1,0,0,1,1,0,0,0,1,1);
    expect_color(1,0,.5f,"neutral settings retain existing HDR clamp");
}

static void diagonal(void)
{
    float data[N*N*4];
    unsigned char off[N*N*4];
    double error_off=0, error_on=0;
    int intermediate=0, changed=0;
    for (int y=0; y<N; ++y) for (int x=0; x<N; ++x) {
        float c=(y+.5f > .6f*(x+.5f)+4.13f) ? 1.f : 0.f;
        int i=4*(y*N+x); data[i]=data[i+1]=data[i+2]=c; data[i+3]=1;
    }
    render(data,N,1,0,0,1,1,0,0,0,1,1);
    memcpy(off,pixels,sizeof(off));
    /* The vertex shader flips vertical texture coordinates. */
    for (int y=0; y<N; ++y) for (int x=0; x<N; ++x)
        CHECK(off[4*(y*N+x)] == (unsigned char)(255*data[4*((N-1-y)*N+x)]), "AA off preserves every source pixel");
    render(data,N,1,1,0,1,1,0,0,0,1,1);
    for (int y=2; y<N-2; ++y) for (int x=2; x<N-2; ++x) {
        int i=4*(y*N+x), coverage=0;
        for (int sy=0; sy<16; ++sy) for (int sx=0; sx<16; ++sx)
            coverage += (N-1-y+(sy+.5f)/16 > .6f*(x+(sx+.5f)/16)+4.13f);
        double ref=coverage/256.0, a=off[i]/255.0, b=pixels[i]/255.0;
        error_off+=(a-ref)*(a-ref); error_on+=(b-ref)*(b-ref);
        intermediate += pixels[i]>0 && pixels[i]<255;
        changed += pixels[i]!=off[i];
        if (fabs((N-1-y+.5)-.6*(x+.5)-4.13)>3)
            CHECK(pixels[i]==off[i], "AA leaves distant flat regions untouched");
    }
    CHECK(changed>0 && intermediate>0,"AA smooths diagonal stair steps");
    CHECK(error_on<error_off,"AA reduces diagonal coverage error against supersampled reference");
    printf("diagonal coverage error: off %.6f, on %.6f (%d fractional pixels)\n",error_off,error_on,intermediate);
}

static void crop(void)
{
    float data[64*64*4];
    float small[N*N*4];
    unsigned char reference[N*N*4];
    fill(data,64,1,0,1);
    for (int y=16; y<48; ++y) for (int x=16; x<48; ++x) {
        int i=4*(y*64+x); data[i]=data[i+1]=data[i+2]=.375f;
    }
    render(data,64,1,1,0,1,1,0,.25f,.25f,.75f,.75f);
    expect_color(.375f,.375f,.375f,"crop prevents neighboring texture pixels bleeding into AA");
    /* Half-size viewport leaves the cleared letterbox intact. */
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(4,4,N-8,N-8); glDrawArrays(0x0005,0,4);
    glReadPixels(0,0,N,N,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for (int y=0; y<N; ++y) for (int x=0; x<N; ++x)
        if (x<4 || y<4 || x>=N-4 || y>=N-4)
            CHECK(pixels[4*(y*N+x)]==0 && pixels[4*(y*N+x)+1]==0 && pixels[4*(y*N+x)+2]==0,"letterbox stays black");
    /* The same 32 physical texels inside a larger RT must have the same AA.
       This catches using virtual display dimensions instead of textureSize. */
    for (int y=0; y<N; ++y) for (int x=0; x<N; ++x) {
        float c=(y+.5f > .6f*(x+.5f)+4.13f) ? 1.f : 0.f;
        for (int channel=0; channel<4; ++channel) {
            small[4*(y*N+x)+channel]=channel==3 ? 1 : c;
            data[4*((y+16)*64+x+16)+channel]=channel==3 ? 1 : c;
        }
    }
    render(small,N,1,1,0,1,1,0,0,0,1,1);
    memcpy(reference,pixels,sizeof(reference));
    render(data,64,1,1,0,1,1,0,.25f,.25f,.75f,.75f);
    CHECK(memcmp(reference,pixels,sizeof(reference))==0,"AA uses physical texels and respects cropped edges");
}

int main(void)
{
    SDL_Window *window;
    SDL_GLContext context;
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    window=SDL_CreateWindow("Picture shader test",N,N,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    context=window ? SDL_GL_CreateContext(window) : NULL;
    if (!context || !gl_load((void *(*)(const char *))SDL_GL_GetProcAddress)) {
        fprintf(stderr,"OpenGL 3.3 required: %s\n",SDL_GetError()); SDL_Quit(); return 1;
    }
    program=glCreateProgram();
    GLuint vs=shader(GL_VERTEX_SHADER,present_vs), fs=shader(GL_FRAGMENT_SHADER,present_fs);
    glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
    GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked); CHECK(linked,"production shader links");
    CHECK(glGetUniformLocation(program,"u_saturation")>=0,"production saturation uniform exists");
    CHECK(glGetUniformLocation(program,"u_antialias")>=0,"production AA uniform exists");
    if (!failures) {
        glGenVertexArrays(1,&vao); glBindVertexArray(vao);
        glGenTextures(1,&output_tex); glBindTexture(GL_TEXTURE_2D,output_tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,N,N,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output_tex,0);
        CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"test framebuffer complete");
        glGenTextures(1,&input_tex); glBindTexture(GL_TEXTURE_2D,input_tex);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        colors(); diagonal(); crop();
        glDeleteTextures(1,&input_tex); glDeleteTextures(1,&output_tex);
        glDeleteFramebuffers(1,&fbo);
    }
    CHECK(glGetError()==0,"no final GL errors");
    glDeleteShader(vs); glDeleteShader(fs); /* Remaining context objects die below. */
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit();
    if (!failures) puts("picture GL: all checks passed");
    return failures ? 1 : 0;
}
