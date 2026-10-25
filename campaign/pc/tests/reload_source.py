"""Select real game routines for the isolated reload regression executable.

MinGW's linker keeps unresolved references in otherwise unused sections of the
whole menu translation unit. Copy complete functions verbatim instead; there is
no alternate search/combine implementation in the test. Fail if the source
shape changes rather than silently testing stale copied code.
"""
import re
import sys
from pathlib import Path


def function(source, name):
    match = re.search(r"^(?:static )?(?:void|int|float|short|unsigned char|unsigned int|unsigned) " + name + r"\([^\n]*?\)[ \t]*(?://[^\n]*)?\s*\{", source, re.M)
    if not match:
        raise ValueError(f"cannot locate production function {name}")
    start = match.start()
    opening = source.index("{", match.start())
    depth = 1
    for end in range(opening + 1, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if not depth:
            return source[start:end + 1]
    raise ValueError(f"unterminated production function {name}")


def main():
    source, output = map(Path, sys.argv[1:])
    text = source.read_text()
    body = '\n\n'.join(function(text, name) for name in ('ItemSort', 'Combi_99', 'bhSearchBullet'))
    body += '\n\n' + function((source.parent / 'weapon.c').read_text(), 'bhCheckBullet')
    player = (source.parent / 'player.c').read_text()
    table_start = player.index('const WPN_TAB WpnTab[23] =')
    table_end = player.index('\n};', table_start) + 3
    body += '\n\n' + player[table_start:table_end]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('/* Generated from production sub1.c; do not edit. */\n'
                      '#include <ps2/veronica/prog/types.h>\n'
                      '#include <ps2/veronica/prog/main.h>\n'
                      '#include <ps2/veronica/prog/sub1.h>\n'
                      '#include <ps2/veronica/prog/item.h>\n'
                      '#include <ps2/veronica/prog/player.h>\n'
                      'static unsigned short sakai;\nS_WORK swork;\n' + body + '\n')


if __name__ == '__main__':
    main()
