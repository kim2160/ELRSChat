"""Fetch the pinned upstream hardware definitions for local builds."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HARDWARE = ROOT / 'src/hardware'
URL = 'https://github.com/ExpressLRS/targets.git'
REVISION = '2b90c511e965304e333d99ca5f724dbc8b7c154f'


def git(*args, capture=False):
    return subprocess.run(
        ['git', '-C', str(HARDWARE), *args], check=True, text=True,
        input='', stdout=subprocess.PIPE if capture else None,
        env=dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never')
    ).stdout


def main():
    if HARDWARE.exists() and not (HARDWARE / '.git').is_dir():
        raise SystemExit(f'Refusing to replace an existing directory: {HARDWARE}')
    if not HARDWARE.exists():
        HARDWARE.mkdir()
        git('init')
    if git('status', '--porcelain', capture=True).strip():
        raise SystemExit('Hardware definitions have local changes; preserve them before updating.')
    current = subprocess.run(
        ['git', '-C', str(HARDWARE), 'rev-parse', '--verify', 'HEAD'],
        capture_output=True, text=True
    )
    if current.returncode or current.stdout.strip() != REVISION:
        git('fetch', '--depth=1', '--no-tags', URL, REVISION)
        git('checkout', '--detach', REVISION)
    print(f'Hardware definitions ready: {REVISION}')


if __name__ == '__main__':
    main()
