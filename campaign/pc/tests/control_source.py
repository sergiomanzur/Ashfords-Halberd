"""Select complete production functions for isolated input regression harnesses."""
from pathlib import Path
import sys
from reload_source import function
source, output, *names = sys.argv[1:]
text = Path(source).read_text()
Path(output).write_text('/* Generated production routines; do not edit. */\n' +
                        '\n\n'.join(function(text, name) for name in names) + '\n')
