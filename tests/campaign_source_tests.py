"""Source/data boundary checks for the imported campaign backend."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
ENGINE = ROOT / "campaign"


class CampaignSourceTests(unittest.TestCase):
    def test_decomp_includes_use_configured_header_tree(self):
        for folder in (ENGINE / "pc/glue", ENGINE / "pc/tests", ENGINE / "pc/include"):
            for source in folder.rglob("*"):
                if source.suffix not in {".c", ".h"}:
                    continue
                text = source.read_text(encoding="utf-8")
                self.assertFalse(re.search(r'#include\s+"\.\./\.\./decomp/', text), str(source))
                for include in re.findall(r'#include\s+<((?:ps2|cri)/[^>]+)>', text):
                    self.assertTrue((ROOT / "upstream/recvx-campaign-decomp/include" / include).is_file(), include)

    def test_campaign_sources_present(self):
        for name in ("pc/src/host/host.c", "pc/src/host/overlay.cpp",
                     "pc/glue/tps_glue.c", "pc/glue/save_glue.c"):
            self.assertTrue((ENGINE / name).is_file(), name)

    def test_no_copied_art_or_game_data(self):
        self.assertTrue(ENGINE.is_dir())
        forbidden = {".ico", ".png", ".jpg", ".jpeg", ".dds", ".afs", ".rdx",
                     ".iso", ".pss", ".bin", ".bmp", ".tga"}
        found = [str(p.relative_to(ENGINE)) for p in ENGINE.rglob("*")
                 if p.is_file() and p.suffix.lower() in forbidden]
        self.assertEqual(found, [])

    def test_attribution_and_pins(self):
        provenance = (ENGINE / "PROVENANCE.md").read_text(encoding="utf-8")
        for value in ("38d457e3ec11a3120447e3544031af9a40eccbdb",
                      "4aac5133901da3821fe443b79d6cdd1e3b29e6da"):
            self.assertIn(value, provenance)
        self.assertIn("Copyright (c) 2026 aikatore", (ENGINE / "LICENSE").read_text())


if __name__ == "__main__":
    unittest.main()
