"""Bundle pinned Rime DLLs and a precompiled Pinyin dictionary for Windows.

Usage: python scripts/fetch-rime-windows.py x64 dist/windows/x64
Requires Python 3 and Windows tar (or 7z for the .7z archive).
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
LIBRARIES = {
    'x64': '7478c7caa4ff6b37de86daba1f7ce4a994a4f5ba24872a820fb2b3a9b01fed15',
    'x86': 'af235c26c06152ce09ceb8fe9d9ab9fba7ab43ce30aa952b40174d806f5cc3d9',
}
DATA = [
    ('rime-prelude', '082425ea0684bca36474415d4a0e8db9b016487e', '66239ba4745d54471e5ce5540b45e86ec663ff5dba004cb23c95531a3bdec00f'),
    ('rime-essay', '054920de4f54c9e5994276a96a4fc2a35cb51aa3', '3339b40aae91b0216d393aeccc6a8f7daad56a1f3a5e1ffcf464c80690d1b6ce'),
    ('rime-pinyin-simp', '0c6861ef7420ee780270ca6d993d18d4101049d0', '46f37114a7929ecc01003a236803c8b1e5198382e6a21f83fae036604a6b08bf'),
]


def fetch(url, path, digest):
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        print('Downloading', path.name, flush=True)
        urllib.request.urlretrieve(url, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise RuntimeError('SHA-256 mismatch: ' + str(path))


def bundle(arch, output):
    cache = ROOT / 'dist' / 'rime-cache'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / ('rime-' + arch + '.7z')
    fetch(f'https://github.com/rime/librime/releases/download/1.17.0/rime-33e7814-Windows-msvc-{arch}.7z', archive, LIBRARIES[arch])
    unpack = cache / arch
    unpack.mkdir(exist_ok=True)
    if shutil.which('7z'):
        subprocess.run(['7z', 'x', '-y', str(archive), '-o' + str(unpack)], check=True, stdout=subprocess.DEVNULL)
    else:
        subprocess.run(['tar', '-xf', str(archive), '-C', str(unpack)], check=True)
    rime = output.resolve() / 'Rime'
    for directory in ('bin', 'shared', 'build', 'licenses'):
        (rime / directory).mkdir(parents=True, exist_ok=True)
    for dll in (unpack / 'dist' / 'lib').glob('*.dll'):
        shutil.copy2(dll, rime / 'bin' / dll.name)
        architecture_dir = rime / 'bin' / arch
        architecture_dir.mkdir(exist_ok=True)
        shutil.copy2(dll, architecture_dir / dll.name)
    # The official MSVC DLL statically links its third-party dependencies.
    for name, commit, digest in DATA:
        package = cache / (name + '.tar.gz')
        fetch(f'https://codeload.github.com/rime/{name}/tar.gz/{commit}', package, digest)
        with tarfile.open(package) as tar:
            for member in tar.getmembers():
                relative = pathlib.PurePosixPath(member.name).parts
                if member.isfile() and len(relative) == 2:
                    filename = relative[-1]
                    if filename.endswith(('.yaml', '.txt')) or filename == 'LICENSE':
                        target = rime / ('licenses' if filename == 'LICENSE' else 'shared')
                        target /= name + '-LICENSE.txt' if filename == 'LICENSE' else filename
                        target.write_bytes(tar.extractfile(member).read())
    shutil.copy2(ROOT / 'Sources/Omnikey/macOS/Rime/default.custom.yaml', rime / 'shared/default.custom.yaml')
    shutil.copy2(ROOT / 'Sources/Omnikey/macOS/Rime/LICENSE-librime.txt', rime / 'licenses/librime-LICENSE.txt')
    # Keep tools next to the DLL for Windows' DLL search path.
    deployer = rime / 'bin/rime_deployer.exe'
    shutil.copy2(unpack / 'dist/bin/rime_deployer.exe', deployer)
    user = cache / ('deploy-' + arch)
    user.mkdir(exist_ok=True)
    subprocess.run([str(deployer), '--build', str(user), str(rime / 'shared'), str(rime / 'build')], check=True)
    for required in ('pinyin_simp.table.bin', 'pinyin_simp.prism.bin', 'pinyin_simp.schema.yaml', 'default.yaml'):
        if not (rime / 'build' / required).is_file():
            raise RuntimeError('Missing prebuilt Rime data: ' + required)
    print('Rime 1.17.0 ready:', rime, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('arch', choices=LIBRARIES)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    bundle(args.arch, args.output)
