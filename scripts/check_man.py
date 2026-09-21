#!/usr/bin/env python3
"""Render source and installed manuals, including direct-file aliases."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAN = ROOT / 'man'
OUTPUT = ROOT / 'build' / 'man-review'
WIDTHS = (60, 80, 100)


def run(args, *, env=None, cwd=None, input=None):
    result = subprocess.run(args, text=True, capture_output=True,
                            env=env, cwd=cwd, input=input, check=True)
    # man can report failed .so requests but still exit successfully.
    if result.stderr.strip():
        raise RuntimeError(f'{args}:\n{result.stderr}')
    return result.stdout


def plain(text):
    text = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', text)
    return run(['col', '-bx'], input=text)


def validate(text, page, width):
    for heading in ('NAME', 'DESCRIPTION', 'SEE ALSO'):
        assert heading in [line.strip() for line in text.splitlines()], (page, heading)
    for line in text.splitlines():
        assert len(line) <= width, (page, width, line)
    if page.suffix == '.3':
        assert 'SYNOPSIS' in [line.strip() for line in text.splitlines()], page
        assert '#include "' in text, page
    assert not re.search(r'^\.(?:TH|SH|so|BI|EX|EE)\b', text, re.M), page


def main():
    for tool in ('man', 'groff', 'col', 'make'):
        if not shutil.which(tool):
            raise SystemExit(f'Required manual-check tool not found: {tool}')
    pages = sorted(MAN.glob('man*/*.[37]'))
    assert pages, 'No manual sources found'
    prototypes = {}
    for header in ('bignums.h', 'complexFFT.h'):
        for line in (ROOT / 'bigNums' / header).read_text().splitlines():
            match = re.match(r'(?:void|int|uint32_t) (\w+)\(.*\);$', line)
            if match:
                prototypes[match[1]] = line
    for name in prototypes:
        assert (MAN / 'man3' / f'{name}.3').is_file(), name
    with tempfile.TemporaryDirectory(prefix='bignums-man-check-') as temp:
        staged = Path(temp) / 'usr' / 'share' / 'man'
        run(['make', '-C', str(ROOT / 'bigNums'), 'install-man',
             'PREFIX=/usr', f'DESTDIR={temp}', 'MANDIR=/usr/share/man'])
        for page in pages:
            # Absolute paths and an unrelated cwd exercise direct-file lookup.
            groff = run(['groff', '-ww', '-Tutf8', '-man', str(page)], cwd=temp)
            validate(plain(groff), page, 80)
            installed = staged / page.parent.name / page.name
            assert not installed.is_symlink(), installed
            assert installed.read_text() == page.read_text(), installed
            for width in WIDTHS:
                env = {**os.environ, 'MANWIDTH': str(width), 'MANPAGER': 'cat',
                       'PAGER': 'cat', 'MANOPT': '', 'MAN_KEEP_FORMATTING': '1',
                       'LC_ALL': 'C.UTF-8'}
                commands = (
                    ['man', '-l', str(page)],
                    ['man', '-M', str(MAN), page.suffix[1:], page.stem],
                    ['man', '-M', str(staged), page.suffix[1:], page.stem],
                )
                texts = [plain(run(cmd, cwd=temp, env=env)) for cmd in commands]
                for text in texts:
                    validate(text, page, width)
                assert texts[0] == texts[1] == texts[2], (page, width)
                if page.stem in prototypes:
                    synopsis = texts[0].split('\nSYNOPSIS\n')[1].split('\nDESCRIPTION\n')[0]
                    assert prototypes[page.stem] in ' '.join(synopsis.split()), page
                out = OUTPUT / str(width) / page.parent.name
                out.mkdir(parents=True, exist_ok=True)
                (out / f'{page.name}.txt').write_text(texts[0])
    print(f'Checked {len(pages)} pages at {WIDTHS} columns: direct-file, source-tree,')
    print(f'and installed lookup; {len(prototypes)} public prototypes; no roff diagnostics.')
    print(f'Read rendered previews in {OUTPUT}')


if __name__ == '__main__':
    main()
