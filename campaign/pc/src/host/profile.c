/*
 * RECVX_PROFILE=<file>: sampling profiler for builds without perf. A timer per thread (the
 * main/render thread from profile_start, the game thread from profile_thread) on that
 * thread's own CPU clock records the interrupted instruction pointer at 1 kHz of its CPU
 * time; at exit the raw addresses are written to <file>, one per line with the thread
 * ("r" render, "g" game), for symbolisation with pc/tools/profile.py. (A process-wide
 * ITIMER_PROF signal lands on any thread, mostly ones asleep in a syscall.)
 */
#ifndef _WIN32
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <ucontext.h>
#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id _sigev_un._tid
#endif

#define MAX_SAMPLES (1 << 20)
static uint32_t* samples;      /* malloc'd only with RECVX_PROFILE: not in the PS2 address window */
static char* sample_thread;
static volatile unsigned nsamples;
static const char* out_path;
static __thread char thread_tag;

static void on_prof(int sig, siginfo_t* si, void* ctx)
{
    ucontext_t* uc = ctx;
    unsigned i;
    (void)sig; (void)si;
    if (!thread_tag || (i = __atomic_fetch_add(&nsamples, 1, __ATOMIC_RELAXED)) >= MAX_SAMPLES) return;
#if defined(__i386__)
    samples[i] = (uint32_t)uc->uc_mcontext.gregs[REG_EIP];
#else
    samples[i] = (uint32_t)uc->uc_mcontext.gregs[REG_RIP];
#endif
    sample_thread[i] = thread_tag;
}

static void dump(void)
{
    FILE* f = fopen(out_path, "w");
    unsigned n_s = nsamples < MAX_SAMPLES ? nsamples : MAX_SAMPLES;
    if (!f) return;
    for (unsigned i = 0; i < n_s; i++) fprintf(f, "%x %c\n", samples[i], sample_thread[i] ? sample_thread[i] : '?');
    fclose(f);
    {   /* <file>.maps: which library a sample outside the executable fell in (profile.py) */
        char path[1024], buf[4096];
        size_t n;
        FILE *in = fopen("/proc/self/maps", "r"), *out;
        snprintf(path, sizeof(path), "%s.maps", out_path);
        out = in ? fopen(path, "w") : NULL;
        while (out && (n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
        if (out) fclose(out);
        if (in) fclose(in);
    }
}

/* sample the calling thread (tag 'r', 'g') */
void profile_thread(char tag)
{
    struct sigevent ev = { 0 };
    struct itimerspec it = { { 0, 1000000 }, { 0, 1000000 } };   /* 1 kHz of this thread's CPU time */
    timer_t t;
    sigset_t set;
    if (!out_path) return;
    thread_tag = tag;
    sigemptyset(&set);
    sigaddset(&set, SIGPROF);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);   /* SDL's threads start with SIGPROF blocked */
    ev.sigev_notify = SIGEV_THREAD_ID;
    ev.sigev_signo = SIGPROF;
    ev.sigev_notify_thread_id = (int)syscall(SYS_gettid);
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &ev, &t) == 0) timer_settime(t, 0, &it, NULL);
}

void profile_start(void)
{
    struct sigaction sa = { 0 };
    out_path = getenv("RECVX_PROFILE");
    if (!out_path) return;
    samples = malloc(MAX_SAMPLES * sizeof(*samples));
    sample_thread = malloc(MAX_SAMPLES);
    if (!samples || !sample_thread) { out_path = NULL; return; }
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
    profile_thread('r');
    atexit(dump);
}
#else
void profile_start(void) {}
void profile_thread(char tag) { (void)tag; }
#endif
