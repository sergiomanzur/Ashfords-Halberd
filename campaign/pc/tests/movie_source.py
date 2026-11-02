"""Extract the real movie policy tables and lifecycle for the regression fixture."""
import re
import sys
from pathlib import Path
from reload_source import function

source, output = map(Path, sys.argv[1:])
text = source.read_text()
tables = []
for name in ('MovieTypeDef', 'MovieVolDef', 'MovieDef'):
    match = re.search(r'^.*\b' + name + r'\[\d+\].*?=\s*\{[^;]*;', text, re.M)
    if not match:
        raise ValueError(f'Missing native movie table {name}')
    tables.append(match.group())
names = ('PlayStartMovieEx', 'PlayStopMovieEx', 'PlayStopMovie', 'PlayMovieMain')
Path(output).write_text('/* Complete production tables and functions. */\n' +
                        '\n\n'.join(tables + [function(text, name) for name in names]) + '\n')
