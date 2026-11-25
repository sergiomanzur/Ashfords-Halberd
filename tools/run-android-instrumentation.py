"""On-device instrumentation runner for the Android Java-lifecycle-dependent
SDL campaign tests.

Installs the debug APK plus its androidTest APK on a connected adb device and
runs the built-in android.test.InstrumentationTestRunner harness over
com.ashfordshalberd.app.NativeSDLTests, which executes the seven
libjni_test_*.so test bodies (pad_weapon_switch, pad_quicksave, rumble,
test_audio_snapshot, test_audio_snapshot_decoder, test_audio_snapshot_synth,
and the bindings virtual-gamepad block) inside a real Android Java process.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

# Test method names reported by the runner, in execution order.
TEST_METHODS = [
    "testBindings",
    "testPadWeaponSwitch",
    "testPadQuicksave",
    "testRumble",
    "testAudioSnapshot",
    "testAudioSnapshotDecoder",
    "testAudioSnapshotSynth",
]

INSTRUMENTATION = "com.ashfordshalberd.app.test/android.test.InstrumentationTestRunner"
DEFAULT_TIMEOUT = 300


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
    prefix = [str(adb)]
    if serial:
        prefix += ["-s", serial]
    if kwargs.get("text"):
        kwargs.setdefault("encoding", "utf-8")
        kwargs.setdefault("errors", "replace")
    return subprocess.run(prefix + cmd, **kwargs)


def ensure_device_ready(adb, serial):
    result = run_adb(adb, serial, ["shell", "echo", "ok"], capture_output=True, text=True)
    if result.returncode != 0 or "ok" not in (result.stdout or ""):
        raise SystemExit(f"adb device not ready (serial={serial}): {result.stderr or result.stdout}")


def install_apk(adb, serial, path):
    result = run_adb(adb, serial, ["install", "-r", "-t", str(path)], capture_output=True, text=True)
    if result.returncode != 0 or "Success" not in (result.stdout or ""):
        raise SystemExit(f"adb install failed for {path}: {result.stderr or result.stdout}")


def parse_results(stdout):
    """Map the InstrumentationTestRunner output to per-test PASS/FAIL.

    The runner's one-character-per-test stream does not identify method
    order, so names come from the "Failure/Error in <method>:" blocks and
    the final verdict from the OK/FAILURES summary line.

    Returns (outcomes, ok): outcomes is a list of (method, status, detail);
    ok mirrors the runner's final verdict.
    """
    stdout = stdout or ""
    failure_blocks = {}
    current = None
    for line in stdout.splitlines():
        m = re.match(r"(?:Failure|Error) in (\w+):", line)
        if m:
            current = m.group(1)
            failure_blocks.setdefault(current, [])
            continue
        if current:
            failure_blocks[current].append(line)
            if not line.strip():
                current = None

    ok = bool(re.search(r"^OK \(\d+ tests\)", stdout, re.MULTILINE))
    crashed = "Process crashed" in stdout

    outcomes = []
    for method in TEST_METHODS:
        if method in failure_blocks:
            detail = "\n".join(failure_blocks[method][:12]).strip()
            outcomes.append((method, "FAIL", detail or "instrumentation failure"))
        elif ok:
            outcomes.append((method, "PASS", ""))
        else:
            note = "process crashed before a verdict" if crashed else "no result reported by runner"
            outcomes.append((method, "FAIL", note))
    return outcomes, ok


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run the Android instrumentation SDL tests on device.")
    parser.add_argument("--repo-root", default=None, help="Repository root (default: parent of this script)")
    parser.add_argument("--serial", default=None, help="adb serial (default: first device)")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT, help="Overall instrumentation timeout in seconds")
    parser.add_argument("--skip-install", action="store_true", help="Do not (re)install the APKs")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parent.parent
    app_apk = repo_root / "campaign" / "android" / "app" / "build" / "outputs" / "apk" / "debug" / "app-debug.apk"
    test_apk = repo_root / "campaign" / "android" / "app" / "build" / "outputs" / "apk" / "androidTest" / "debug" / "app-debug-androidTest.apk"

    adb = find_adb()
    if not adb.exists():
        raise SystemExit(f"adb not found: {adb}")

    ensure_device_ready(adb, args.serial)

    if not args.skip_install:
        for apk in (app_apk, test_apk):
            if not apk.exists():
                raise SystemExit(f"APK not built: {apk}")
        print(f"Installing {app_apk.name} ...")
        install_apk(adb, args.serial, app_apk)
        print(f"Installing {test_apk.name} ...")
        install_apk(adb, args.serial, test_apk)

    print(f"Running instrumentation {INSTRUMENTATION} ...")
    start = time.monotonic()
    try:
        result = run_adb(
            adb,
            args.serial,
            ["shell", "am", "instrument", "-w", INSTRUMENTATION],
            capture_output=True,
            text=True,
            timeout=args.timeout,
        )
    except subprocess.TimeoutExpired:
        print("FAIL instrumentation (adb timeout)")
        return 1
    elapsed = time.monotonic() - start

    stdout = result.stdout or ""
    outcomes, ok = parse_results(stdout)

    passed = failed = 0
    for method, status, detail in outcomes:
        line = f"{status} {method} ({detail})" if detail else f"{status} {method}"
        print(line)
        if status == "PASS":
            passed += 1
        else:
            failed += 1

    # Surface unexpected native crashes reported by the runner verbatim.
    crash = re.search(r"^Process crashed.*$", stdout, re.MULTILINE)
    print()
    print(f"Summary: {passed} passed, {failed} failed, {len(outcomes)} total ({elapsed:.1f}s)")
    if crash:
        print(crash.group(0))
    if not ok:
        tail = "\n".join(stdout.strip().splitlines()[-20:])
        if tail:
            print(f"Runner output tail:\n{tail}")
    return 0 if (ok and failed == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
