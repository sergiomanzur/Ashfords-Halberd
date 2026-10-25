#!/usr/bin/env python3
"""Regenerate docs/dev/missing-returns.txt and docs/dev/uninitialized-locals.txt.

    python3 pc/tools/decomp_warnings.py [BUILD_DIR]   (default: build)
    python3 pc/tools/decomp_warnings.py --declarations BUILD_DIR

Compiles every decomp source of the configured build tree again (its own command line, output
to /dev/null) with -Wreturn-type -Wreturn-mismatch -Wmaybe-uninitialized instead of -w, and
lists the warnings by file and function (no line numbers, so the lists only change when the
code does). The build does not change: the decomp keeps compiling with -w (pc/CMakeLists.txt).
--declarations (the ctest decomp_declarations) lists calls to undeclared functions instead and
fails on any: GCC 14 and later stop the build on them, GCC 13 only warns and -w hides it.
"""
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
PROG = 'decomp/src/ps2/veronica/prog/'
FLAGS = ['-fpermissive', '-Wreturn-type', '-Wreturn-mismatch', '-Wmaybe-uninitialized', '-Wuninitialized']   # -fpermissive: GCC 14's return-mismatch error would stop before the uninitialized analysis

HEADERS = {
    'missing-returns.txt': (
        '# Non-void decomp functions that can fall off their end or return no value (GCC -Wreturn-type /\n'
        '# -Wreturn-mismatch), by file and function; pc/tools/decomp_warnings.py writes this file.\n'
        '# MWCC returned whatever was in v0. Fixed against retail (TARGET_PC, no longer listed): en01.c\n'
        '# bhEne01_SetLinkEnemy, adxwrap.c GetAdxStatus, en03.c bhEne03_GetWallDir, en17.c\n'
        '# bhEne17_PlayerDGCheck, en22.c bhEne22_GetDengekiColorAddr (returns NULL; its two stores check it).\n'
        '# The rest: value unused by callers, or only reached for invalid inputs (retail returns a stale register).\n'),
    'uninitialized-locals.txt': (
        '# Possible uses of uninitialized locals in game code (GCC -Wmaybe-uninitialized), by file and\n'
        '# function; pc/tools/decomp_warnings.py writes this file. The PC build zero-initializes all locals\n'
        '# (-ftrivial-auto-var-init=zero); on the PS2 these read stale register/stack values. Verify the\n'
        '# ones that matter against the retail ELF. Fixed: ps2_sg_sd.c Volume_Control (dvol of a fade up,\n'
        '# docs/ARCHITECTURE.md "Fidelity notes").\n'),
}


def split_command(line):
    if os.name != 'nt':
        return shlex.split(line)
    # Ninja emits native Windows command lines. POSIX shlex removes path
    # backslashes and misreads the apostrophe in Ashford's directory name.
    import ctypes
    argc = ctypes.c_int()
    split = ctypes.windll.shell32.CommandLineToArgvW
    split.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    split.restype = ctypes.POINTER(ctypes.c_wchar_p)
    argv = split(line, ctypes.byref(argc))
    if not argv:
        raise OSError('Cannot parse compiler command')
    try:
        return [argv[i] for i in range(argc.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(argv, ctypes.c_void_p))


def commands(build):
    out = subprocess.run(['ninja', '-C', build, '-t', 'commands', 'recvx'], check=True,
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        args = split_command(line)
        if '-c' not in args or PROG not in args[-1] or not args[-1].endswith('.c'):
            continue
        keep, skip = [], 0
        for a in args:
            if skip:
                skip -= 1
            elif a in ('-MT', '-MF', '-o'):
                skip = 1
            elif a in ('-w', '-MD', '-ftrivial-auto-var-init=zero') or a.startswith('-Werror'):
                pass
            else:
                keep.append(a)
        yield args[-1], keep[:-2] + FLAGS + ['-o', os.devnull, '-c', args[-1]]


def warnings(job):
    src, cmd = job
    result = subprocess.run(cmd, cwd=BUILD, capture_output=True, text=True)
    err = result.stderr
    func, found = None, []
    for line in err.splitlines():
        m = re.match(r'(.+?): In function [‘\'](.+?)[’\']:', line)
        if m:
            func = m.group(2) if os.path.basename(m.group(1)) == os.path.basename(src) else None
            continue
        m = re.match(r'(.+?):\d+:\d+: (?:warning|error): (.*) \[(-W[a-z-]+)\]', line)
        if m and os.path.basename(m.group(1)) == os.path.basename(src) and func:
            msg = re.sub(r'[‘’]', "'", m.group(2))
            found.append((os.path.basename(src), func, m.group(3), msg))
    if result.returncode:
        raise RuntimeError('Compiler check failed for %s:\n%s' % (src, '\n'.join(err.splitlines()[-8:])))
    return found


if __name__ == '__main__':
    decl = '--declarations' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--declarations']
    BUILD = os.path.abspath(args[0] if args else os.path.join(ROOT, 'build'))
    if decl:
        FLAGS = ['-fsyntax-only', '-Wimplicit-function-declaration']
    jobs = list(commands(BUILD))
    if not jobs:
        sys.exit('no decomp compile commands in %s (configure it first)' % BUILD)
    with ThreadPoolExecutor(min(4, os.cpu_count() or 1)) as ex:
        found = sorted(set(w for ws in ex.map(warnings, jobs) for w in ws))
    if decl:
        found = [w for w in found if w[2] == '-Wimplicit-function-declaration']
        for src, func, _, msg in found:
            print('%s %s: %s' % (src, func, msg))
        print('%d calls to undeclared functions in the decomp' % len(found))
        sys.exit(1 if found else 0)
    lists = {'missing-returns.txt': [w for w in found if w[2] in ('-Wreturn-type', '-Wreturn-mismatch')],
             'uninitialized-locals.txt': [w for w in found if 'uninitialized' in w[2]]}
    for name, ws in lists.items():
        with open(os.path.join(ROOT, 'docs', 'dev', name), 'w') as f:
            f.write(HEADERS[name])
            for src, func, _, msg in ws:
                f.write('%s %s: %s\n' % (src, func, msg))
        print('docs/dev/%s: %d entries' % (name, len(ws)))
