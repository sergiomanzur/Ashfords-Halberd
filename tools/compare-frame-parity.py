#!/usr/bin/env python3
"""compare-frame-parity.py - Layer 2 tolerant raster comparison for RECVX_DUMP_FRAMES.

Plan 2 Task 7 verifies the Android arm64/GLES port's rendering against the
Windows PC build. Cross-GPU byte-exact frame dumps are not attainable (the
determinism notes in ARCHITECTURE.md already record retail stale reads that
shift with layout), so parity is proven in two layers (see
docs/superpowers/specs/2026-11-24-android-port-design.md, "Testing and
acceptance"):

  Layer 1 - GS command-stream equality (PRIMARY automated gate). The game's GS
            command stream (gs/stream.c) is recorded on PC and on Android; the
            opening-scene streams must match after removing stream-embedded
            host addresses. Divergence triggers an arm64 stale-read hunt.
  Layer 2 - Raster comparison with tolerance (THIS TOOL).
            RECVX_DUMP_FRAMES=<dir> writes BMPs on PC and device. Per-pixel
            deltas and perceptual / histogram metrics must stay within a
            recorded tolerance across all video settings (scale, FXAA/SMAA,
            dithering/deband, picture controls, aspect, VSync/FPS limit);
            transition frames may legitimately differ, hence a maximum failing
            frame fraction. The comparison tool and its thresholds are the
            deliverable, along with a recorded tolerance.

This tool implements Layer 2 and also carries a DOCUMENTED-OPTIONAL Layer 1
mode (--gs-stream). Layer 1 is currently BLOCKED-ON-PRODUCER: no code path on
either platform serializes the GS command stream (gs/stream.c records only in
memory, gs_gl.c dumps BMPs only, and no RECVX_*STREAM* writer exists). The
--gs-stream mode therefore only does a byte-exact compare of two *already
existing* files plus a first-difference report; it requires a GS command-stream
producer that does not exist yet (see docs/android-backend.md). If either file
is missing it prints SKIP and continues. It is not acceptance-tested beyond a
trivial self-check and the Layer 1 gate is recorded UNMET at closeout.

Dump format (EXACT - the reader is built to this, not a generic BMP reader).
gs_gl.c writes BMPs via SDL3 SDL_SaveBMP from SDL_PIXELFORMAT_ABGR8888
surfaces. Filename families in a dump dir:

  frame_NNNNNN.bmp    final presented image (1920x1080 on the device)
  interp_NNNNNN.bmp   frame-interpolation image
  win_NNNNNN.bmp      window / overlay image
  rt<fbp>_NNNNNN.bmp  render targets, e.g. rt0_000030.bmp (1708x1024)

Windows PC dumps use the same naming. Files are matched by (family, index)
across the two dirs. By default only the frame_ family is compared; --prefix
all compares every family and --prefix rt0 (etc.) selects one. Both platform
dumps use the same 32-bit/24-bit uncompressed BMP encoding; both bottom-up
(positive height) and top-down (negative height) BMPs are handled.

A frame present in only one dir (or a requested --checkpoints index with no
file anywhere) is a MISSING frame: it is reported in the summary with A-only /
B-only / both-absent counts and, by default, is counted as a failed frame so a
truncated or partial device pull cannot pass the gate. --allow-missing <f>
(default 0.0) is the escape hatch that tolerates up to that fraction of missing
frames for legitimate partial overlaps; it does not relax --max-fail-fraction
on the frames that were actually compared.

Channel-mapping ASSUMPTION - validated on a synthetic 32-bit BMP with known
colors in parity_selftest/, do NOT take it on faith. SDL_SaveBMP converts an
ABGR8888 surface to SDL_PIXELFORMAT_BGRA32, which on little-endian hosts is
ARGB8888, so each 32bpp pixel is stored in the file as B,G,R,A bytes,
bottom-up, with a BITMAPV4HEADER / BI_BITFIELDS masks R=0x00ff0000,
G=0x0000ff00, B=0x000000ff, A=0xff000000 (or a legacy 40-byte BI_RGB header
when SDL_HINT_BMP_SAVE_LEGACY_FORMAT is set). 24bpp BMPs store B,G,R bytes.
The reader therefore maps R=byte2, G=byte1, B=byte0 (alpha ignored), honours
explicit BI_BITFIELDS masks when present, and handles both orientations.

Stdlib only (argparse, os, struct, sys). No numpy / PIL: the repo tooling runs
on a bare Python install. Honest PASS/FAIL/SKIP reporting in the style of
tools/run-android-ctest.py; per-frame mean abs diff in the style of
campaign/pc/tools/compare_frames.py.
"""
import argparse
import os
import struct
import sys

# ----------------------------------------------------------------------------
# Defaults (all CLI-overridable), per the Task 7 brief.
# ----------------------------------------------------------------------------
DEFAULT_MEAN_TOL = 6.0        # mean abs per-channel difference, of 255
DEFAULT_HIST_THRESHOLD = 0.80  # per-channel 16-bin histogram intersection
DEFAULT_MAX_FAIL_FRACTION = 0.10
DEFAULT_ALLOW_MISSING = 0.0   # fraction of missing frames tolerated (0 = none)
DEFAULT_EVERY = 30            # index sampling: keep indices divisible by this
DEFAULT_PREFIX = "frame"      # default dump family
DEFAULT_CHECKPOINTS = None     # e.g. "30,120,210"

# DIB compression constants.
BI_RGB = 0
BI_BITFIELDS = 3


# ----------------------------------------------------------------------------
# Minimal BMP reader (24/32-bpp, uncompressed, both orientations).
# ----------------------------------------------------------------------------
class Image(object):
    """Decoded RGB image. rgb is a flat bytearray of R,G,B per pixel, row-major,
    top-to-bottom."""
    __slots__ = ("w", "h", "rgb")

    def __init__(self, w, h, rgb):
        self.w = w
        self.h = h
        self.rgb = rgb


def _chan(word, mask):
    """Extract an 8-bit channel value from a little-endian pixel word using a
    channel mask. Normalises non-8-bit masks (e.g. 565) to 0..255."""
    if mask == 0:
        return 0
    shift = (mask & -mask).bit_length() - 1
    maxv = mask >> shift
    v = (word & mask) >> shift
    if maxv == 0:
        return 0
    return (v * 255) // maxv


def read_bmp(path):
    """Read a 24- or 32-bpp uncompressed BMP written by SDL_SaveBMP for an
    ABGR8888 surface (or a plain 24-bpp BMP). Returns an Image.

    File bytes per pixel are B,G,R,(A); R=byte2, G=byte1, B=byte0. Both
    bottom-up (+height) and top-down (-height) BMPs are accepted. Explicit
    BI_BITFIELDS masks are honoured when present; otherwise the BMP/SDL
    defaults R=0x00ff0000, G=0x0000ff00, B=0x000000ff are used.
    """
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 14 + 40 or data[:2] != b"BM":
        raise ValueError("not a BMP (missing BM file header)")
    bf_off_bits = struct.unpack_from("<I", data, 10)[0]
    dib = 14
    bi_size = struct.unpack_from("<I", data, dib)[0]

    if bi_size == 12:  # OS/2 BITMAPCOREHEADER (defensive)
        w = struct.unpack_from("<H", data, dib + 4)[0]
        h = struct.unpack_from("<H", data, dib + 6)[0]
        bpp = struct.unpack_from("<H", data, dib + 10)[0]
        compression = BI_RGB
    else:  # BITMAPINFOHEADER and its V2/V3/V4/V5 extensions
        w = struct.unpack_from("<i", data, dib + 4)[0]
        h = struct.unpack_from("<i", data, dib + 8)[0]
        bpp = struct.unpack_from("<H", data, dib + 14)[0]
        compression = struct.unpack_from("<I", data, dib + 16)[0]

    top_down = h < 0
    w = abs(w)
    h = abs(h)
    if bpp not in (24, 32):
        raise ValueError("unsupported BMP bit depth %d (only 24/32)" % bpp)
    bytes_pp = bpp // 8

    # Channel masks. For BI_BITFIELDS the masks sit at file offsets 54/58/62
    # (and 66 for alpha) whether they are the V2+ header fields or the
    # bmiColors gap after a 40-byte INFOHEADER. SDL3 writes a V4 header with
    # exactly these masks.
    if compression == BI_BITFIELDS:
        rmask = struct.unpack_from("<I", data, 54)[0]
        gmask = struct.unpack_from("<I", data, 58)[0]
        bmask = struct.unpack_from("<I", data, 62)[0]
    else:  # BI_RGB (and anything else we accept): standard defaults
        rmask, gmask, bmask = 0x00FF0000, 0x0000FF00, 0x000000FF

    stride = ((w * bpp + 31) // 32) * 4  # rows are padded to 4-byte boundary
    if bf_off_bits + stride * h > len(data):
        raise ValueError("truncated BMP pixel data")

    rgb = bytearray(w * h * 3)
    pos = 0
    for row in range(h):
        src_row = row if top_down else (h - 1 - row)
        off = bf_off_bits + src_row * stride
        for x in range(w):
            p = off + x * bytes_pp
            word = int.from_bytes(data[p:p + bytes_pp], "little")
            rgb[pos] = _chan(word, rmask)
            rgb[pos + 1] = _chan(word, gmask)
            rgb[pos + 2] = _chan(word, bmask)
            pos += 3
    return Image(w, h, rgb)


# ----------------------------------------------------------------------------
# Alignment helpers (pure Python, stdlib only).
# ----------------------------------------------------------------------------
def bilinear_scale(img, nw, nh):
    """Bilinearly scale img to (nw, nh). Pure Python, no numpy."""
    out = bytearray(nw * nh * 3)
    sw, sh = img.w, img.h
    src = img.rgb
    for j in range(nh):
        sy = (j + 0.5) * sh / nh - 0.5
        if sy < 0.0:
            sy = 0.0
        if sy > sh - 1:
            sy = float(sh - 1)
        y0 = int(sy)
        y1 = y0 + 1 if y0 + 1 < sh else y0
        wy = sy - y0
        for i in range(nw):
            sx = (i + 0.5) * sw / nw - 0.5
            if sx < 0.0:
                sx = 0.0
            if sx > sw - 1:
                sx = float(sw - 1)
            x0 = int(sx)
            x1 = x0 + 1 if x0 + 1 < sw else x0
            wx = sx - x0
            b00 = (y0 * sw + x0) * 3
            b01 = (y0 * sw + x1) * 3
            b10 = (y1 * sw + x0) * 3
            b11 = (y1 * sw + x1) * 3
            o = (j * nw + i) * 3
            for c in range(3):
                top = src[b00 + c] * (1.0 - wx) + src[b01 + c] * wx
                bot = src[b10 + c] * (1.0 - wx) + src[b11 + c] * wx
                v = top * (1.0 - wy) + bot * wy
                iv = int(v + 0.5)
                out[o + c] = 0 if iv < 0 else (255 if iv > 255 else iv)
    return Image(nw, nh, out)


def crop(img, W, H, X, Y):
    """Crop a W x H sub-rectangle at (X, Y)."""
    if X < 0 or Y < 0 or X + W > img.w or Y + H > img.h:
        raise ValueError("crop-rect %dx%d+%d+%d outside %dx%d image"
                         % (W, H, X, Y, img.w, img.h))
    out = bytearray(W * H * 3)
    for j in range(H):
        s = ((Y + j) * img.w + X) * 3
        d = (j * W) * 3
        out[d:d + W * 3] = img.rgb[s:s + W * 3]
    return Image(W, H, out)


def parse_crop_rect(text):
    """Parse 'WxH+X+Y' -> (W, H, X, Y)."""
    parts = text.split("+")
    if len(parts) != 3:
        raise ValueError("crop-rect must be WxH+X+Y, got %r" % text)
    wh = parts[0].split("x")
    if len(wh) != 2:
        raise ValueError("crop-rect must be WxH+X+Y, got %r" % text)
    W, H, X, Y = int(wh[0]), int(wh[1]), int(parts[1]), int(parts[2])
    if W <= 0 or H <= 0 or X < 0 or Y < 0:
        raise ValueError("crop-rect dimensions/offsets invalid: %r" % text)
    return (W, H, X, Y)


# ----------------------------------------------------------------------------
# Frame metrics.
# ----------------------------------------------------------------------------
def compare(a, b):
    """Return (mean_abs_per_channel_diff, histogram_intersection) for two
    equal-sized Images. The histogram score is the mean of the three per-
    channel 16-bin normalised histogram intersections, in [0, 1]."""
    rgb_a = a.rgb
    rgb_b = b.rgb
    n = len(rgb_a)  # w*h*3 channel values
    pixels = n // 3
    total = 0
    h_a = [[0] * 16 for _ in range(3)]
    h_b = [[0] * 16 for _ in range(3)]
    for idx in range(0, n, 3):
        for c in range(3):
            ca = rgb_a[idx + c]
            cb = rgb_b[idx + c]
            total += ca - cb if ca >= cb else cb - ca
            bin_a = ca >> 4
            bin_b = cb >> 4
            h_a[c][bin_a] += 1
            h_b[c][bin_b] += 1
    mean = total / n if n else 0.0
    hist = 0.0
    for c in range(3):
        inter = 0
        for k in range(16):
            x = h_a[c][k]
            y = h_b[c][k]
            inter += x if x < y else y
        hist += inter / pixels if pixels else 0.0
    hist /= 3.0
    return mean, hist


# ----------------------------------------------------------------------------
# Dump-dir matching.
# ----------------------------------------------------------------------------
def parse_dump_name(name):
    """Return (family, index) for e.g. frame_000030.bmp / rt150_000030.bmp,
    or None. Index is parsed as an int so zero-padding width is irrelevant."""
    if not name.endswith(".bmp"):
        return None
    stem = name[:-4]
    u = stem.rfind("_")
    if u <= 0:
        return None
    fam = stem[:u]
    num = stem[u + 1:]
    if not fam or not fam[0].isalpha() or not num.isdigit():
        return None
    return fam, int(num)


def scan_dir(path):
    """Return {(family, index): full_path} for dump BMPs in path."""
    out = {}
    for name in os.listdir(path):
        parsed = parse_dump_name(name)
        if parsed is not None:
            out[parsed] = os.path.join(path, name)
    return out


def family_selected(fam, prefix):
    return prefix == "all" or fam == prefix


# ----------------------------------------------------------------------------
# Layer 1 (documented-optional): byte-exact GS command-stream compare.
# ----------------------------------------------------------------------------
def run_gs_stream(file_a, file_b, quiet):
    """Byte-exact compare of two GS command-stream files. Returns an exit code:
    0 identical / SKIP (missing), 1 differ. Prints a result line (and, for a
    difference, a first-difference line). SKIP is never a failure so the caller
    can 'continue' to Layer 2."""
    prompt = ("requires a GS command-stream producer that does not exist yet; "
              "see docs/android-backend.md")
    if not os.path.isfile(file_a) or not os.path.isfile(file_b):
        missing = file_a if not os.path.isfile(file_a) else file_b
        print("SKIP gs-stream: file not found: %s (%s)" % (missing, prompt))
        return 0
    with open(file_a, "rb") as f:
        a = f.read()
    with open(file_b, "rb") as f:
        b = f.read()
    n = min(len(a), len(b))
    first = None
    for i in range(n):
        if a[i] != b[i]:
            first = i
            break
    if first is None and len(a) != len(b):
        first = n
    if first is None:
        print("gs-stream IDENTICAL (%d bytes) compare=PASS" % len(a))
        return 0
    av = a[first] if first < len(a) else None
    bv = b[first] if first < len(b) else None
    fa = "0x%02x" % av if av is not None else "EOF"
    fb = "0x%02x" % bv if bv is not None else "EOF"
    extra = ""
    if len(a) != len(b):
        extra = " (length mismatch A=%d B=%d)" % (len(a), len(b))
    print("gs-stream DIFFER compare=FAIL")
    print("  first-difference at byte %d (A=%s B=%s)%s"
          % (first, fa, fb, extra))
    return 1


# ----------------------------------------------------------------------------
# Layer 2 driver.
# ----------------------------------------------------------------------------
def run_layer2(args):
    if not os.path.isdir(args.dirA) or not os.path.isdir(args.dirB):
        print("ERROR: dump directory not found: %s / %s"
              % (args.dirA, args.dirB))
        return 1
    a_scan = scan_dir(args.dirA)
    b_scan = scan_dir(args.dirB)
    a_keys = set(a_scan)
    b_keys = set(b_scan)
    all_keys = a_keys | b_keys

    checkpoints = None
    if args.checkpoints:
        try:
            checkpoints = set(int(x) for x in args.checkpoints.split(",") if x.strip())
        except ValueError:
            print("ERROR: --checkpoints must be comma-separated integers, got %r"
                  % args.checkpoints)
            return 1
    every = args.every if args.every and args.every >= 1 else 1

    # Build the expected (family, index) frame slots from the UNION of the two
    # dirs (not the intersection), so a frame present in only one dir is NOT
    # silently dropped. Requested --checkpoints indices are expected even when
    # no file exists for them anywhere.
    if checkpoints is not None:
        expected = set()
        for idx in checkpoints:
            at_idx = set(k for k in all_keys
                         if k[1] == idx and family_selected(k[0], args.prefix))
            if at_idx:
                expected |= at_idx
            else:
                # no file at this index anywhere -> a missing checkpoint slot
                fam = args.prefix if args.prefix != "all" else "checkpoint"
                expected.add((fam, idx))
    else:
        expected = set(k for k in all_keys
                       if family_selected(k[0], args.prefix) and k[1] % every == 0)
    expected = sorted(expected, key=lambda k: (k[1], k[0]))

    crop_rect = None
    if args.crop_rect:
        try:
            crop_rect = parse_crop_rect(args.crop_rect)
        except ValueError as e:
            print("ERROR: %s" % e)
            return 1

    if not expected:
        print("No frames selected (prefix=%s, dirs=%s %s)"
              % (args.prefix, args.dirA, args.dirB))
        print("Summary: 0 expected (0 compared), 0 passed, 0 failed "
              "(fraction=0.00), missing: 0 (A-only: 0, B-only: 0) | "
              "mean-tol=%.2f hist>=%.2f max-fail-fraction=%.2f "
              "allow-missing=%.2f | RESULT=FAIL exit=1 (nothing to compare)"
              % (args.mean_tol, args.hist_threshold, args.max_fail_fraction,
                 args.allow_missing))
        return 1

    # Split expected slots into matched pairs and MISSING slots. A missing slot
    # is a frame present in only one dir (or a checkpoint with no file at all).
    pairs = []            # (label, pa, pb)
    missing_slots = []    # (idx, pa, pb, is_checkpoint)
    for (fam, idx) in expected:
        pa = a_scan.get((fam, idx))
        pb = b_scan.get((fam, idx))
        if pa is not None and pb is not None:
            label = os.path.splitext(os.path.basename(pa))[0]
            pairs.append((label, pa, pb))
        else:
            missing_slots.append((idx, pa, pb, checkpoints is not None))

    results = []  # (label, mean|None, hist|None, status, reason)
    for (label, pa, pb) in pairs:
        mean = hist = None
        status = "PASS"
        reason = ""
        try:
            A = read_bmp(pa)
            B = read_bmp(pb)
            if args.scale:
                A = bilinear_scale(A, B.w, B.h)
            if crop_rect:
                W, H, X, Y = crop_rect
                A = crop(A, W, H, X, Y)
                B = crop(B, W, H, X, Y)
            if A.w != B.w or A.h != B.h:
                status = "FAIL"
                reason = "size mismatch %dx%d vs %dx%d" % (A.w, A.h, B.w, B.h)
            else:
                mean, hist = compare(A, B)
                why = []
                if mean > args.mean_tol:
                    why.append("mean")
                if hist < args.hist_threshold:
                    why.append("hist")
                if why:
                    status = "FAIL"
                    reason = ",".join(why)
        except Exception as e:  # honest: a broken dump frame is a failure
            status = "FAIL"
            reason = "read/align error: %s" % e
        results.append((label, mean, hist, status, reason))

    # Missing accounting: a frame present in only one dir (or a requested
    # checkpoint index with no file anywhere) is a MISSING frame. Nothing is
    # silently dropped - the counts and the failing fraction carry them.
    a_only = sum(1 for s in missing_slots if s[1] is not None and s[2] is None)
    b_only = sum(1 for s in missing_slots if s[1] is None and s[2] is not None)
    both_absent = sum(1 for s in missing_slots if s[1] is None and s[2] is None)
    missing = len(missing_slots)

    if not args.quiet:
        for (label, mean, hist, status, reason) in results:
            if mean is None:
                print("%s %s mean=n/a hist=n/a" % (label, status))
            else:
                print("%s %s mean=%.2f hist=%.2f" % (label, status, mean, hist))
        # Requested-but-absent checkpoints are always reported as FAIL lines.
        # Other missing frames are carried by the summary counts (not dropped).
        for (idx, pa, pb, is_checkpoint) in missing_slots:
            if is_checkpoint:
                print("checkpoint %d FAIL missing" % idx)
        # Worst offenders: failing compared frames (capped), else the highest-
        # mean few.
        failing = [r for r in results if r[3] == "FAIL"]
        if failing:
            failing.sort(key=lambda r: (r[1] if r[1] is not None else 1e9),
                         reverse=True)
            offenders = failing[:10]
        else:
            ranked = sorted(results, key=lambda r: (r[1] if r[1] is not None else -1.0),
                            reverse=True)
            offenders = ranked[:3]
        if offenders:
            print("Worst offenders (by mean diff):")
            for (label, mean, hist, status, reason) in offenders:
                if mean is None:
                    print("  %s %s mean=n/a hist=n/a [%s]"
                          % (label, status, reason))
                else:
                    print("  %s %s mean=%.2f hist=%.2f [%s]"
                          % (label, status, mean, hist, reason or "-"))

    compared = len(results)
    passed = sum(1 for r in results if r[3] == "PASS")
    matched_failures = compared - passed
    expected_count = compared + missing
    # A missing frame is a failed frame: it is included in `failed` and in the
    # reported failing fraction.
    failed = matched_failures + missing
    frac = failed / expected_count if expected_count else 0.0
    metric_frac = matched_failures / compared if compared else 0.0
    missing_frac = missing / expected_count if expected_count else 0.0
    metric_ok = metric_frac <= args.max_fail_fraction
    missing_ok = missing_frac <= args.allow_missing
    overall = "PASS" if (metric_ok and missing_ok) else "FAIL"
    ec = 0 if overall == "PASS" else 1

    miss_info = "missing: %d (A-only: %d, B-only: %d" % (missing, a_only, b_only)
    if both_absent:
        miss_info += ", both-absent: %d" % both_absent
    miss_info += ")"
    reason_suffix = ""
    if overall == "FAIL" and not missing_ok:
        reason_suffix = " (missing frames exceed --allow-missing)"
    elif overall == "FAIL" and not metric_ok:
        reason_suffix = " (failing fraction exceeds --max-fail-fraction)"
    elif overall == "PASS" and missing:
        reason_suffix = " (missing within --allow-missing)"

    print("Summary: %d expected (%d compared), %d passed, %d failed "
          "(fraction=%.2f), %s | mean-tol=%.2f hist>=%.2f "
          "max-fail-fraction=%.2f allow-missing=%.2f | "
          "metric-frac=%.2f missing-frac=%.2f | RESULT=%s exit=%d%s"
          % (expected_count, compared, passed, failed, frac, miss_info,
             args.mean_tol, args.hist_threshold, args.max_fail_fraction,
             args.allow_missing, metric_frac, missing_frac, overall, ec,
             reason_suffix))
    return ec


# ----------------------------------------------------------------------------
# CLI.
# ----------------------------------------------------------------------------
DESCRIPTION = """\
Layer 2 tolerant raster/histogram comparison of RECVX_DUMP_FRAMES BMP dumps
(Plan 2 Task 7 Android-vs-PC parity), plus a documented-optional Layer 1
--gs-stream byte-compare mode. Matches dump files by index across <dirA> and
<dirB>, computes a mean absolute per-channel difference and a per-channel
16-bin histogram-intersection score per frame, and gates on mean/histogram
tolerances plus a maximum failing frame fraction. Exit 0 = within tolerances,
1 = out.
"""

EPILOG = """\
Defaults: mean-tol=6.0 (of 255), hist-threshold=0.80, max-fail-fraction=0.10,
allow-missing=0.0, every=30, prefix=frame. One line per frame looks like
'frame_000030 PASS mean=2.31 hist=0.94'; worst offenders and a summary follow.

Dump families: frame_NNNNNN.bmp (presented), interp_NNNNNN.bmp, win_NNNNNN.bmp,
rt<fbp>_NNNNNN.bmp (render targets). --prefix all compares every family,
--prefix frame (default) only the presented frames, --prefix rt0 one family.
--every N keeps frames whose index is divisible by N (30 matches the default
dump cadence; use 1 for every frame). --checkpoints 30,120,210 restricts to
explicit frame indices and overrides --every. --scale bilinearly rescales A to
B's dimensions; --crop-rect WxH+X+Y compares only a sub-rect (letterboxed
content). --quiet prints only the summary.

Missing frames: the expected frame set is the UNION of both dirs (not the
intersection), so a frame present in only one dir is reported as MISSING, e.g.
'missing: 37 (A-only: 37, B-only: 0)'. A requested --checkpoints index with no
file anywhere prints 'checkpoint 999 FAIL missing'. Missing frames are counted
as failed frames; by default (--allow-missing 0.0) ANY missing frame fails the
gate, so a truncated device pull cannot pass. --allow-missing <fraction> (escape
hatch for legitimate partial overlaps) tolerates up to that fraction of missing
frames; the summary still reports the count. --max-fail-fraction keeps gating
the frames that were actually compared.

Channel mapping: SDL_SaveBMP writes ABGR8888 surfaces as B,G,R,A 32bpp bytes,
bottom-up (V4 BI_BITFIELDS masks), or B,G,R 24bpp; the reader maps
R=byte2,G=byte1,B=byte0, honours BI_BITFIELDS masks and both orientations, and
was validated on a synthetic 32-bit BMP with known colors.

Layer 1 mode (--gs-stream) is DOCUMENTED-OPTIONAL and BLOCKED-ON-PRODUCER: it
requires a GS command-stream producer that does not exist yet (no code path
serializes the GS stream; gs/stream.c records in memory only; see
docs/android-backend.md). Given two files it does a byte-exact compare and
prints the first difference; if either file is missing it prints SKIP and
continues. It is not acceptance-tested beyond a trivial self-check; the Layer 1
gate is recorded UNMET / BLOCKED-ON-PRODUCER at closeout.
"""


def build_parser():
    p = argparse.ArgumentParser(
        prog="compare-frame-parity.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=DESCRIPTION,
        epilog=EPILOG,
    )
    p.add_argument("dirA", nargs="?",
                   help="Layer 2: first RECVX_DUMP_FRAMES dir (e.g. PC golden)")
    p.add_argument("dirB", nargs="?",
                   help="Layer 2: second RECVX_DUMP_FRAMES dir (e.g. Android)")
    p.add_argument("--gs-stream", nargs=2, metavar=("FILE_A", "FILE_B"),
                   help="Layer 1 (documented-optional, BLOCKED-ON-PRODUCER): "
                        "byte-exact compare of two GS command-stream files; "
                        "prints SKIP and continues if either is missing. "
                        "Requires a stream producer that does not exist yet "
                        "(see docs/android-backend.md).")
    p.add_argument("--prefix", default=DEFAULT_PREFIX,
                   help="dump family to compare: frame (default), all, or a "
                        "specific family such as rt0")
    p.add_argument("--mean-tol", type=float, default=DEFAULT_MEAN_TOL,
                   help="max mean abs per-channel diff (of 255), default 6.0")
    p.add_argument("--hist-threshold", type=float, default=DEFAULT_HIST_THRESHOLD,
                   help="min per-channel 16-bin histogram intersection, "
                        "default 0.80")
    p.add_argument("--max-fail-fraction", type=float,
                   default=DEFAULT_MAX_FAIL_FRACTION,
                   help="max fraction of frames allowed to fail, default 0.10")
    p.add_argument("--allow-missing", type=float, default=DEFAULT_ALLOW_MISSING,
                   help="fraction of missing frames tolerated before the run "
                        "fails, default 0.0 (any missing frame fails). Escape "
                        "hatch for legitimate partial overlaps / truncated "
                        "pulls; does not relax --max-fail-fraction on compared "
                        "frames")
    p.add_argument("--every", type=int, default=DEFAULT_EVERY,
                   help="index stride: keep frame indices divisible by N, "
                        "default 30 (use 1 for all)")
    p.add_argument("--checkpoints", default=DEFAULT_CHECKPOINTS,
                   help="comma-separated frame indices to restrict to, e.g. "
                        "30,120,210 (overrides --every)")
    p.add_argument("--scale", action="store_true",
                   help="bilinearly scale A to B's dimensions before comparing")
    p.add_argument("--crop-rect", default=None, metavar="WxH+X+Y",
                   help="compare only a WxH sub-rect at +X+Y, e.g. 8x16+0+0")
    p.add_argument("--quiet", action="store_true",
                   help="print only the summary")
    return p


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    if (args.dirA is None) != (args.dirB is None):
        parser.error("provide both <dirA> and <dirB>")
    have_dirs = args.dirA is not None and args.dirB is not None
    if not have_dirs and not args.gs_stream:
        parser.error("provide <dirA> <dirB> for Layer 2, and/or "
                     "--gs-stream FILE_A FILE_B for Layer 1")

    gs_ec = None
    if args.gs_stream:
        gs_ec = run_gs_stream(args.gs_stream[0], args.gs_stream[1], args.quiet)

    l2_ec = 0
    if have_dirs:
        l2_ec = run_layer2(args)

    if gs_ec is not None and not have_dirs:
        # gs-stream-only mode: SKIP (0) is not a failure; continue gracefully.
        return gs_ec
    return 1 if (gs_ec == 1 or l2_ec == 1) else 0


if __name__ == "__main__":
    sys.exit(main())
