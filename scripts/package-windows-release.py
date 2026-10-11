"""Create complete x64/x86 and combined Windows release ZIPs and checksums."""
import argparse
import ctypes
import hashlib
import pathlib
import shutil
import struct
import zipfile


def verify_pe(path, machine):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    if data[offset:offset + 4] != b'PE\0\0' or struct.unpack_from('<H', data, offset + 4)[0] != machine:
        raise RuntimeError('Wrong PE architecture: ' + str(path))


def version(path):
    api = ctypes.windll.version
    size = api.GetFileVersionInfoSizeW(str(path.resolve()), None)
    data = ctypes.create_string_buffer(size)
    if not size or not api.GetFileVersionInfoW(str(path.resolve()), 0, size, data):
        raise RuntimeError('Missing version resource: ' + str(path))
    pointer = ctypes.c_void_p()
    length = ctypes.c_uint()
    if not api.VerQueryValueW(data, '\\', ctypes.byref(pointer), ctypes.byref(length)):
        raise RuntimeError('Invalid version resource: ' + str(path))
    fields = struct.unpack_from('<13I', ctypes.string_at(pointer, length.value))
    return f'{fields[2] >> 16}.{fields[2] & 65535}.{fields[3] >> 16}'


def add_rime(source, target, arch, common=True):
    if common:
        for name in ('shared', 'build', 'licenses'):
            shutil.copytree(source / 'Rime' / name, target / 'Rime' / name, dirs_exist_ok=True)
    architecture = target / 'Rime' / 'bin' / arch
    architecture.mkdir(parents=True, exist_ok=True)
    for name in ('rime.dll', 'OmnikeyUpdate.exe'):
        shutil.copy2(source / 'Rime' / 'bin' / arch / name, architecture / name)


def archive(directory, output):
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                package.write(path, path.relative_to(directory).as_posix())
    with zipfile.ZipFile(output) as package:
        bad = package.testzip()
        if bad:
            raise RuntimeError('Corrupt ZIP member: ' + bad)


def package(build, output, release):
    output.mkdir(parents=True, exist_ok=True)
    stages = {}
    for arch, main, machine in [('x64', 'Omnikey64.exe', 0x8664), ('x86', 'Omnikey32.exe', 0x14c)]:
        source = build / arch
        for filename in (main, 'OmnikeyUpdate.exe'):
            verify_pe(source / filename, machine)
        verify_pe(source / 'Rime' / 'bin' / arch / 'rime.dll', machine)
        if version(source / main) != release:
            raise RuntimeError('Application version does not match release: ' + str(source / main))
        target = output / 'staging' / release / arch
        target.mkdir(parents=True, exist_ok=True)
        for name in (main, 'OmnikeyUpdate.exe'):
            shutil.copy2(source / name, target / name)
        add_rime(source, target, arch)
        stages[arch] = target
    combined = output / 'staging' / release / 'combined'
    combined.mkdir(parents=True, exist_ok=True)
    for arch, main in [('x64', 'Omnikey64.exe'), ('x86', 'Omnikey32.exe')]:
        shutil.copy2(stages[arch] / main, combined / main)
        add_rime(build / arch, combined, arch, common=arch == 'x64')
    # The x86 helper runs on either Windows architecture and detects its target.
    shutil.copy2(stages['x86'] / 'OmnikeyUpdate.exe', combined / 'OmnikeyUpdate.exe')
    instructions = (
        f'Omnikey {release} for Windows\n\n'
        'Extract the entire ZIP and keep the Rime folder beside the EXE.\n'
        'Run Omnikey64.exe on 64-bit Windows, Omnikey32.exe on 32-bit Windows.\n'
        'Alt+Z cycles Vietnamese -> English -> Chinese -> Vietnamese.\n'
        'Chinese Pinyin: nihao + Space = 你好. Numbers select candidates;\n'
        'PgUp/PgDn change pages; Esc cancels.\n'
        'https://github.com/jetaudio/Omnikey\n'
    )
    assets = []
    for suffix, directory in [('x64', stages['x64']), ('x86', stages['x86']), ('', combined)]:
        (directory / 'README-Windows.txt').write_text(instructions, encoding='utf-8')
        filename = f'Omnikey-{release}-Windows' + ('-' + suffix if suffix else '') + '.zip'
        path = output / filename
        archive(directory, path)
        assets.append(path)
    # OpenKey 2.1.1 and earlier download OpenKey-<version>-Windows.zip and look
    # for OpenKey64.exe/OpenKey32.exe in it. This package lets them update to
    # Omnikey; the new updater keeps those EXE names afterwards.
    legacy = output / 'staging' / release / 'legacy'
    shutil.copytree(combined, legacy, dirs_exist_ok=True)
    for main in ('Omnikey64.exe', 'Omnikey32.exe'):
        (legacy / main).rename(legacy / main.replace('Omnikey', 'OpenKey'))
    path = output / f'OpenKey-{release}-Windows.zip'
    archive(legacy, path)
    assets.append(path)
    checksums = '\n'.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name for path in assets) + '\n'
    (output / 'SHA256SUMS.txt').write_text(checksums, encoding='ascii')
    print(checksums, end='')
    print('Combined package staging:', combined)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('build', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    package(args.build, args.output, args.version)
