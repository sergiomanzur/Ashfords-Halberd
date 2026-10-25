"""Contract checks for the single-player campaign presentation and package."""
import pathlib
import re
import struct
import unittest
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]


class OfflineIdentityTests(unittest.TestCase):
    def test_transport_and_build_dependencies_removed(self):
        pc = ROOT / "campaign/pc"
        for name in ("src/net", "server", "src/host/coop.h", "src/host/lobby.h",
                     "glue/coop_glue.c", "glue/lobby_glue.c", "glue/trade_glue.c"):
            path = pc / name
            self.assertFalse(any(p.is_file() for p in path.rglob('*')) if path.is_dir() else path.exists(), name)
        cmake = (pc / "CMakeLists.txt").read_text()
        self.assertFalse(re.search(r"miniupnpc|natpmp|monocypher|ws2_32|add_subdirectory\(server", cmake, re.I))

    def test_generated_assets_and_music_are_packaged(self):
        art = ROOT / "assets/branding"
        self.assertEqual((art / "ashford.ico").read_bytes()[:4], b"\x00\x00\x01\x00")
        for name in ("launcher.bmp", "ashford-icon.bmp"):
            self.assertEqual((art / name).read_bytes()[:2], b"BM")
        with wave.open(str(art / "launcher.wav")) as music:
            self.assertEqual(music.getnchannels(), 2)
            self.assertEqual(music.getsampwidth(), 2)
            self.assertEqual(music.getnframes() / music.getframerate(), 32)
        self.assertIn("'assets'", (ROOT / "tools/build-campaign.ps1").read_text())

    def test_launcher_has_explicit_automation_bypass(self):
        host = (ROOT / "campaign/pc/src/host/host.c").read_text()
        self.assertIn('"--skip-launcher"', host)
        self.assertIn("ashford_launcher(g_config.iso_path", host)
        self.assertNotIn('"--coop-', host)


if __name__ == "__main__":
    unittest.main()
