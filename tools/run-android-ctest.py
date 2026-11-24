"""On-device ctest runner for Android arm64 campaign tests.

Runs the curated portable test roster on a connected adb device, parses the
echoed device-side exit code, and reports PASS/FAIL/SKIP honestly.
"""
import argparse
import os
import re
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


def find_adb():
    env = os.environ.get("ADB")
    if env:
        return Path(env)
    return Path(r"C:\Users\sergi\AppData\Local\Android\Sdk\platform-tools\adb.exe")


def run_adb(adb, serial, cmd, **kwargs):
    """Run an adb command and return a CompletedProcess."""
    prefix = [str(adb)]
    if serial:
        prefix += ["-s", serial]
    full = prefix + cmd
    return subprocess.run(full, **kwargs)


def ensure_device_ready(adb, serial):
    result = run_adb(adb, serial, ["shell", "echo", "ok"], capture_output=True, text=True)
    if result.returncode != 0 or "ok" not in (result.stdout or ""):
        raise SystemExit(f"adb device not ready (serial={serial}): {result.stderr or result.stdout}")


def shell_command(test_dir, binary_name, timeout):
    # Use the device-side timeout utility and echo the real exit code.
    inner = f"cd {test_dir} && timeout {timeout} ./{binary_name}; echo EXIT_CODE:$?"
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

    start = time.monotonic()
    result = run_adb(
        adb,
        serial,
        shell_command(remote_dir, binary_name, timeout),
        capture_output=True,
        text=True,
        timeout=timeout + 30,  # extra slack for adb itself
    )
    elapsed = time.monotonic() - start

    stdout = result.stdout or ""
    exit_code = parse_exit_code(stdout)

    # Drop the pushed binary so the pull captures only files the test wrote.
    run_adb(adb, serial, ["shell", f"rm -f {remote_path}"], capture_output=True)

    # Pull whatever the test wrote before interpreting results.
    results_dir = build_dir / "device-results" / name
    pull_results(adb, serial, remote_dir, results_dir)

    if exit_code is None:
        return "FAIL", "could not parse device exit code"
    if exit_code == 124:
        return "FAIL", f"timeout ({timeout}s)"
    if exit_code == SKIP_RETURN_CODE:
        reason = "ISO-gated (RECVX_ISO not set)" if name == "movies" else "skip return code"
        return "SKIP", reason
    if exit_code != 0:
        return "FAIL", f"exit code {exit_code}"
    return "PASS", f"{elapsed:.1f}s"


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run Android arm64 campaign tests on device.")
    parser.add_argument("--build-dir", default="build/campaign-android", help="Campaign build directory")
    parser.add_argument("--serial", default=None, help="adb serial (default: first device)")
    parser.add_argument("--test", action="append", dest="tests", help="Specific test name(s) to run")
    parser.add_argument("--list", action="store_true", help="List selected tests and exit")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT, help="Per-test timeout in seconds")
    args = parser.parse_args(argv)

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = repo_root / build_dir

    selected = args.tests if args.tests else list(PORTABLE_TESTS)

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
