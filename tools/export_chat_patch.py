"""Export the applied source patch; stage newly added source files first."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6'


def main():
    patch = subprocess.check_output(
        ['git', 'diff', '--no-ext-diff', '--no-color', BASE, '--', 'src'], cwd=ROOT
    )
    if not patch:
        raise SystemExit('No source changes found relative to ExpressLRS 4.1.0.')
    destination = ROOT / 'patches/elrs-4.1.0-chat.patch'
    destination.write_bytes(patch)
    print(f'Updated {destination.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
