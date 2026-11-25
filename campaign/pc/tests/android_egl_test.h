#pragma once
#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    EGLDisplay dpy;
    EGLConfig cfg;
    EGLSurface surf;
    EGLContext ctx;
    void* gles_so;
} AndroidEglTest;

static void* android_egl_getproc(const char* name)
{
    void* p = (void*)eglGetProcAddress(name);
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
}

static int android_egl_create_context(AndroidEglTest* e)
{
    static const EGLint ctx_attr[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    e->ctx = eglCreateContext(e->dpy, e->cfg, EGL_NO_CONTEXT, ctx_attr);
    if (e->ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "EGL: eglCreateContext(GLES 3.2) failed: 0x%x\n", eglGetError());
        return 0;
    }
    if (!eglMakeCurrent(e->dpy, e->surf, e->surf, e->ctx)) {
        fprintf(stderr, "EGL: eglMakeCurrent failed: 0x%x\n", eglGetError());
        eglDestroyContext(e->dpy, e->ctx);
        e->ctx = EGL_NO_CONTEXT;
        return 0;
    }
    return 1;
}

static int android_egl_init(AndroidEglTest* e, int w, int h)
{
    static const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, 0x0040 /* EGL_OPENGL_ES3_BIT */,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLint pb_attr[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE };
    EGLint ncfg = 0;
    e->dpy = EGL_NO_DISPLAY; e->surf = EGL_NO_SURFACE; e->ctx = EGL_NO_CONTEXT;
    e->gles_so = dlopen("libGLESv3.so", RTLD_NOW | RTLD_GLOBAL);
    if (!e->gles_so) e->gles_so = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
    e->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (e->dpy == EGL_NO_DISPLAY || !eglInitialize(e->dpy, NULL, NULL)) {
        fprintf(stderr, "EGL: eglInitialize failed: 0x%x\n", eglGetError());
        return 0;
    }
    if (!eglChooseConfig(e->dpy, cfg_attr, &e->cfg, 1, &ncfg) || ncfg < 1) {
        fprintf(stderr, "EGL: eglChooseConfig(GLES3 pbuffer) failed: 0x%x\n", eglGetError());
        return 0;
    }
    e->surf = eglCreatePbufferSurface(e->dpy, e->cfg, pb_attr);
    if (e->surf == EGL_NO_SURFACE) {
        fprintf(stderr, "EGL: eglCreatePbufferSurface failed: 0x%x\n", eglGetError());
        return 0;
    }
    return android_egl_create_context(e);
}

static void android_egl_destroy_context(AndroidEglTest* e)
{
    if (e->dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->ctx != EGL_NO_CONTEXT) { eglDestroyContext(e->dpy, e->ctx); e->ctx = EGL_NO_CONTEXT; }
    }
}

static void android_egl_shutdown(AndroidEglTest* e)
{
    android_egl_destroy_context(e);
    if (e->dpy != EGL_NO_DISPLAY) {
        if (e->surf != EGL_NO_SURFACE) { eglDestroySurface(e->dpy, e->surf); e->surf = EGL_NO_SURFACE; }
        eglTerminate(e->dpy);
        e->dpy = EGL_NO_DISPLAY;
    }
}
#endif
