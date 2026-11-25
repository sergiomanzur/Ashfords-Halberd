"""On-device ctest runner for Android arm64 campaign tests.

Runs the curated portable test roster on a connected adb device, parses the
echoed device-side exit code, and reports PASS/FAIL/SKIP honestly.
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path


# Curated 35-test portable roster from task-1.3-input.md.
PORTABLE_TESTS = [
    "vu",
    "vu_portable",
    "bindings",
    "pad_weapon_switch",
    "pad_quicksave",
    "modern",
    "presets",
    "pacing",
    "rumble",
    "ntex",
    "gsmem",
    "aa_tables",
    "parsers",
    "appear",
    "variants",
    "fileio",
    "savestore",
    "quickstore",
    "snapshot_store",
    "snapshot_core",
    "snapshot_unpatched",
    "game_rng",
    "cheat_room_snapshot",
    "first_person",
    "renderer_snapshot_codec",
    "cast",
    "placer",
    "biorand_opts",
    "spoiler",
    "import",
    "lang",
    "ashford_panel",
    "test_audio_snapshot",
    "test_audio_snapshot_decoder",
    "test_audio_snapshot_synth",
]

# Exit code that mirrors ctest's SKIP_RETURN_CODE semantics.
SKIP_RETURN_CODE = 77

DEFAULT_TIMEOUT = 120

# Tests that dynamically load librecvx_game.so on device (pushed alongside the
# test binary and removed before pulling test artifacts).
GAME_SO_TESTS = {
    "window_load",
}

# Full-build tests appended automatically when librecvx_game.so is present in build_dir.
FULL_BUILD_TESTS = [
    "window_load",
    "rules",
    "modern_glue",
    "tps_pad",
    "reload",
    "quicksave",
    "weapon_switch",
    "movie_return",
    "options_mouse",
]

# Per-test arguments that ctest registers in campaign/pc/CMakeLists.txt.
# The runner must invoke each test exactly as ctest would, with device-side
# substitutions for host-only paths.
TEST_ARGS = {
    # import expects a writable directory for generated fixtures; on device that
    # is the test's working directory (ctest passes the host binary dir).
    "import": ["${DEVICE_CWD}"],
}

# Tests that are not runnable in a headless adb shell because they need the
# Android Java lifecycle / SDL initialization (joystick, audio). Plan 2 owns
# the app shell; these are recorded as SKIP with a disposition instead of FAIL.
DISPOSITIONS = {
    "pad_weapon_switch": "needs Android Java lifecycle: SDL_Init(SDL_INIT_GAMEPAD) (Plan 2 app shell)",
    "pad_quicksave": "needs Android Java lifecycle: SDL_Init(SDL_INIT_GAMEPAD) (Plan 2 app shell)",
    "rumble": "needs Android Java lifecycle: SDL main bootstrap (Plan 2 app shell)",
    "test_audio_snapshot": "needs Android Java lifecycle: SDL_Init(SDL_INIT_AUDIO) (Plan 2 app shell)",
    "test_audio_snapshot_decoder": "needs Android Java lifecycle: SDL_Init(SDL_INIT_AUDIO) (Plan 2 app shell)",
    "test_audio_snapshot_synth": "needs Android Java lifecycle: SDL_Init(0) (Plan 2 app shell)",
}


def substitute_args(args):
    """Expand CMake-style placeholders for the device environment."""
    out = []
    for a in args:
        if a == "${DEVICE_CWD}":
            out.append(".")
        else:
            out.append(a)
    return out


def find_adb():
    env = os.environ.get("ADB")
    if env:
        return Path(env)
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        candidate = Path(local_app_data) / "Android" / "Sdk" / "platform-tools" / "adb.exe"
        if candidate.exists():
            return candidate
    which = shutil.which("adb")
    if which:
        return Path(which)
    return Path("adb")


def run_adb(adb, serial, cmd, **kwargs):
    """Run an adb command and return a CompletedProcess."""
    prefix = [str(adb)]
    if serial:
        prefix += ["-s", serial]
    full = prefix + cmd
    if kwargs.get("text"):
        kwargs.setdefault("encoding", "utf-8")
        kwargs.setdefault("errors", "replace")
    return subprocess.run(full, **kwargs)


def ensure_device_ready(adb, serial):
    result = run_adb(adb, serial, ["shell", "echo", "ok"], capture_output=True, text=True)
    if result.returncode != 0 or "ok" not in (result.stdout or ""):
        raise SystemExit(f"adb device not ready (serial={serial}): {result.stderr or result.stdout}")


def shell_command(test_dir, binary_name, timeout, args):
    # Use the device-side timeout utility and echo the real exit code.
    arg_str = ""
    if args:
        arg_str = " " + " ".join(shlex.quote(a) for a in args)
    inner = f"cd {test_dir} && timeout {timeout} ./{binary_name}{arg_str}; echo EXIT_CODE:$?"
    return ["shell", inner]


def parse_exit_code(stdout):
    """Extract the echoed EXIT_CODE from adb shell stdout."""
    if stdout is None:
        return None
    # Find the last EXIT_CODE line; trailing CRLF from adb is normal.
    matches = re.findall(r"EXIT_CODE:(\d+)", stdout)
    if not matches:
        return None
    return int(matches[-1])


def push_binary(adb, serial, local_path, remote_dir, binary_name):
    run_adb(adb, serial, ["shell", "mkdir", "-p", remote_dir], capture_output=True)
    remote_path = f"{remote_dir}/{binary_name}"
    push = run_adb(adb, serial, ["push", str(local_path), remote_path], capture_output=True, text=True)
    if push.returncode != 0:
        raise SystemExit(f"adb push failed for {local_path}: {push.stderr or push.stdout}")
    chmod = run_adb(adb, serial, ["shell", "chmod", "755", remote_path], capture_output=True, text=True)
    if chmod.returncode != 0:
        raise SystemExit(f"adb chmod failed for {remote_path}: {chmod.stderr or chmod.stdout}")


def pull_results(adb, serial, remote_dir, local_dir):
    """Pull any files the test wrote from its device cwd."""
    if local_dir.exists():
        shutil.rmtree(local_dir)
    local_dir.mkdir(parents=True, exist_ok=True)
    list_result = run_adb(adb, serial, ["shell", f"ls -A {remote_dir}"], capture_output=True, text=True)
    if list_result.returncode != 0:
        return
    entries = [e for e in (list_result.stdout or "").splitlines() if e.strip()]
    if not entries:
        return
    run_adb(adb, serial, ["pull", remote_dir, str(local_dir)], capture_output=True, text=True)
    # adb pull of a directory puts the directory inside local_dir; flatten once.
    nested = local_dir / Path(remote_dir).name
    if nested.exists() and nested.is_dir():
        for child in nested.iterdir():
            dest = local_dir / child.name
            if dest.exists():
                if dest.is_dir():
                    shutil.rmtree(dest)
                else:
                    dest.unlink()
            shutil.move(str(child), str(local_dir))
        nested.rmdir()


def resolve_binary(build_dir, name):
    """Map a ctest name to the built ELF binary.

    Most tests are named `vu` -> `test_vu`; a few CMake targets already carry
    the `test_` prefix (e.g. `test_audio_snapshot`).
    """
    candidates = [build_dir / "campaign" / "pc" / f"test_{name}"]
    if name.startswith("test_"):
        candidates.append(build_dir / "campaign" / "pc" / name)
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def run_test(adb, serial, build_dir, name, timeout):
    local_binary = resolve_binary(build_dir, name)
    if not local_binary.exists():
        return "FAIL", f"binary not found: {local_binary}"

    remote_dir = "/data/local/tmp/ashford-tests"
    binary_name = local_binary.name
    remote_path = f"{remote_dir}/{binary_name}"

    # Give the test a clean writable cwd.
    run_adb(adb, serial, ["shell", f"rm -rf {remote_dir} && mkdir -p {remote_dir}"], capture_output=True)
    push_binary(adb, serial, local_binary, remote_dir, binary_name)

    pushed_extras = []
    if name in GAME_SO_TESTS:
        game_so = build_dir / "campaign" / "pc" / "librecvx_game.so"
        if not game_so.exists():
            return "FAIL", f"game library not found: {game_so}"
        push_binary(adb, serial, game_so, remote_dir, "librecvx_game.so")
        pushed_extras.append(f"{remote_dir}/librecvx_game.so")

    test_args = substitute_args(TEST_ARGS.get(name, []))

    start = time.monotonic()
    try:
        result = run_adb(
            adb,
            serial,
            shell_command(remote_dir, binary_name, timeout, test_args),
            capture_output=True,
            text=True,
            timeout=timeout + 30,  # extra slack for adb itself
        )
    except subprocess.TimeoutExpired:
        rm_targets = " ".join(shlex.quote(p) for p in [remote_path] + pushed_extras)
        run_adb(adb, serial, ["shell", f"rm -f {rm_targets}"], capture_output=True)
        return "FAIL", f"adb timeout ({timeout}s)"
    elapsed = time.monotonic() - start

    stdout = result.stdout or ""
    exit_code = parse_exit_code(stdout)

    # Drop the pushed binary (and any pushed shared libraries) so the pull
    # captures only files the test wrote.
    rm_targets = " ".join(shlex.quote(p) for p in [remote_path] + pushed_extras)
    run_adb(adb, serial, ["shell", f"rm -f {rm_targets}"], capture_output=True)

    # Pull whatever the test wrote before interpreting results.
    results_dir = build_dir / "device-results" / name
    pull_results(adb, serial, remote_dir, results_dir)

    output_tail = "\n".join(line for line in stdout.splitlines() if "EXIT_CODE:" not in line).strip()
    if exit_code is None:
        detail = "could not parse device exit code"
        if output_tail:
            detail += f"\n{output_tail}"
        if result.stderr:
            detail += f"\n{result.stderr}"
        return "FAIL", detail
    if exit_code == 124:
        detail = f"timeout ({timeout}s)"
        if output_tail:
            detail += f"\n{output_tail}"
        if result.stderr:
            detail += f"\n{result.stderr}"
        return "FAIL", detail
    if exit_code == SKIP_RETURN_CODE:
        reason = "ISO-gated (RECVX_ISO not set)" if name == "movies" else "skip return code"
        return "SKIP", reason
    if exit_code != 0:
        detail = f"exit code {exit_code}"
        if output_tail:
            detail += f"\n{output_tail}"
        if result.stderr:
            detail += f"\n{result.stderr}"
        return "FAIL", detail
    return "PASS", f"{elapsed:.1f}s"


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run Android arm64 campaign tests on device.")
    parser.add_argument("--build-dir", default="build/campaign-android", help="Campaign build directory")
    parser.add_argument("--serial", default=None, help="adb serial (default: first device)")
    parser.add_argument("--test", action="append", dest="tests", help="Specific test name(s) to run")
    parser.add_argument("--list", action="store_true", help="List selected tests and exit")
    parser.add_argument("--platform-tests-only", action="store_true",
                        help="Run only PORTABLE_TESTS (ignore any stale librecvx_game.so in build-dir)")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT, help="Per-test timeout in seconds")
    args = parser.parse_args(argv)

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = repo_root / build_dir

    if args.tests:
        selected = args.tests
    else:
        selected = list(PORTABLE_TESTS)
        if not args.platform_tests_only and (build_dir / "campaign" / "pc" / "librecvx_game.so").exists():
            for name in FULL_BUILD_TESTS:
                if name not in selected:
                    selected.append(name)

    if args.list:
        for name in selected:
            print(name)
        return 0

    if not selected:
        print("No tests selected.", file=sys.stderr)
        return 1

    adb = find_adb()
    if not adb.exists():
        raise SystemExit(f"adb not found: {adb}")

    ensure_device_ready(adb, args.serial)

    passed = failed = skipped = 0
    outcomes = []
    for name in selected:
        if name in DISPOSITIONS:
            status, detail = "SKIP", DISPOSITIONS[name]
        else:
            status, detail = run_test(adb, args.serial, build_dir, name, args.timeout)
        outcomes.append((name, status, detail))
        if status == "PASS":
            passed += 1
        elif status == "FAIL":
            failed += 1
        elif status == "SKIP":
            skipped += 1
        detail_text = f" ({detail})" if detail else ""
        print(f"{status} {name}{detail_text}")

    print()
    print(f"Summary: {passed} passed, {failed} failed, {skipped} skipped, {len(selected)} total")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
