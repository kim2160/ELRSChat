"""Check source-only publication, attribution, history, and the review patch."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6'
PATCH = 'patches/elrs-4.1.0-chat.patch'
FORBIDDEN = ('.bin', '.elf', '.hex', '.uf2', '.zip', '.exe', '.dll', '.luac', '.7z', '.tar.gz')
GENERATED = ('dist/', 'build/', '.tools/', 'src/.pio/', 'src/hardware/')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def main():
    tracked = git('ls-files', '-z').decode().split('\0')
    unexpected = [p for p in tracked if p.lower().endswith(FORBIDDEN) or p.startswith(GENERATED)]
    if unexpected:
        raise SystemExit('Generated artifacts must not be tracked:\n' + '\n'.join(unexpected))
    required = (
        'LICENSE', 'LICENSES/GPL-2.0.txt', 'LICENSES/BSD-3-Clause.txt',
        'NOTICE.md', 'README.upstream.md', PATCH,
        'src/lib/ElrsChat/ChatEngine.cpp', 'src/test/test_elrs_chat/test_chat.cpp',
        'src/lua/ELRSChat.lua', 'src/lua/ELRSChat/r16/transport.lua',
    )
    for name in required:
        if name not in tracked or not (ROOT / name).is_file():
            raise SystemExit(f'Required source or notice is missing from Git: {name}')
    if 'ExpressLRS 기반의 독립적인 실험용 채팅 포크' not in (ROOT / 'README.md').read_text(encoding='utf-8'):
        raise SystemExit('README must identify this as an independent experimental fork.')
    upstream_license = git('show', f'{BASE}:LICENSE').replace(b'\r\n', b'\n')
    if (ROOT / 'LICENSE').read_bytes().replace(b'\r\n', b'\n') != upstream_license:
        raise SystemExit('The upstream root license was changed.')
    git('merge-base', '--is-ancestor', BASE, 'HEAD')
    git('diff', '--check')
    git('diff', '--cached', '--check')
    git('apply', '--reverse', '--check', PATCH)
    actual = git('diff', '--no-ext-diff', '--no-color', BASE, '--', 'src')
    if (ROOT / PATCH).read_bytes().replace(b'\r\n', b'\n') != actual.replace(b'\r\n', b'\n'):
        raise SystemExit('Review patch is stale; run python tools/export_chat_patch.py.')
    print('Source-only tree, upstream license/history, and applied patch: OK')


if __name__ == '__main__':
    main()
