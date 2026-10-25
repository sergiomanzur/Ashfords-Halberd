import importlib.util
import pathlib
import unittest
from unittest import mock
from types import SimpleNamespace

path=pathlib.Path(__file__).resolve().parents[1]/"campaign/pc/tools/decomp_warnings.py"
spec=importlib.util.spec_from_file_location("decomp_warnings",path)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class WarningToolTests(unittest.TestCase):
    def test_compiler_errors_cannot_be_hidden_by_other_warnings(self):
        module.BUILD="."
        result=SimpleNamespace(returncode=1,stderr="unit.c: In function 'f':\nunit.c:1:1: warning: overflow [-Woverflow]\nunit.c:2:1: error: syntax error\n")
        with mock.patch.object(module.subprocess,"run",return_value=result):
            with self.assertRaises(RuntimeError):
                module.warnings(("unit.c",["gcc","-c","unit.c"]))

    def test_native_windows_command_quotes(self):
        line='"C:\\Program Files\\gcc.exe" -I"C:/A B/include" -c "C:/A\'s Folder/game.c"'
        self.assertEqual(module.split_command(line),
                         [r"C:\Program Files\gcc.exe", "-IC:/A B/include", "-c", "C:/A's Folder/game.c"])


if __name__=="__main__":
    unittest.main()
