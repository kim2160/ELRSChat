"""Fingerprint the chat firmware and its upstream integration points."""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = (
    'src/include/deferred.h',
    'src/lib/DEVICE/DevicePause.h',
    'src/lib/DEVICE/device.cpp',
    'src/lib/DEVICE/device.h',
    'src/lib/LR1121Driver/LR1121.cpp',
    'src/lib/LR1121Driver/LR1121.h',
    'src/lib/SX127xDriver/SX127x.h',
    'src/lib/SX1280Driver/SX1280.h',
    'src/lib/SX12xxDriverCommon/SX12xxDriverCommon.h',
    'src/lib/tx-crsf/TXModuleEndpoint.cpp',
    'src/src/rxtx_common.cpp',
    'src/src/tx_main.cpp',
    'tools/build_chat.py',
    'tools/source_digest.py',
)


def firmware_digest():
    paths = {ROOT / name for name in INTEGRATION}
    paths.update(p for p in (ROOT / 'src/lib/ElrsChat').rglob('*') if p.is_file())
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda p: p.relative_to(ROOT).as_posix()):
        digest.update(path.relative_to(ROOT).as_posix().encode() + b'\0')
        digest.update(path.read_bytes().replace(b'\r\n', b'\n') + b'\0')
    return digest.hexdigest()


if __name__ == '__main__':
    print(firmware_digest())
