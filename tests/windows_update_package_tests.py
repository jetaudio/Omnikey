"""Test the updater script embedded in the release EXE, against isolated installs."""
import base64
import ctypes
from ctypes import wintypes
import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
kernel.LoadLibraryExW.restype = wintypes.HMODULE
kernel.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
kernel.FindResourceW.restype = wintypes.HANDLE
kernel.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
kernel.LoadResource.restype = wintypes.HANDLE
kernel.LockResource.argtypes = [wintypes.HANDLE]
kernel.LockResource.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
kernel.SizeofResource.restype = wintypes.DWORD
kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


def script_from_helper(helper):
    module = kernel.LoadLibraryExW(str(helper.resolve()), None, 2)
    assert module, 'Cannot read updater resources'
    try:
        resource = kernel.FindResourceW(module, 201, 10)
        assert resource, 'Updater has no embedded apply script'
        data = kernel.LoadResource(module, resource)
        return ctypes.string_at(kernel.LockResource(data), kernel.SizeofResource(module, resource)).decode('utf-8')
    finally:
        kernel.FreeLibrary(module)


def apply(script, directory, target):
    command = f"& {{ {script} }} -MainExe '{target}'"
    encoded = base64.b64encode(command.encode('utf-16-le')).decode('ascii')
    return subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded],
                          cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          creationflags=subprocess.CREATE_NO_WINDOW).returncode


def main(package):
    package = package.resolve()
    base = (ROOT / 'dist/update-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    temporary = pathlib.Path(tempfile.mkdtemp(prefix='Omnikey cập nhật ', dir=base)).resolve()
    assert temporary.is_relative_to(base), 'Test cleanup must remain within workspace'
    try:
        with zipfile.ZipFile(package) as archive:
            helper = temporary / 'OmnikeyUpdate.exe'
            helper.write_bytes(archive.read('OmnikeyUpdate.exe'))
            script = script_from_helper(helper)
            expected = (ROOT / 'Sources/Omnikey/win32/Omnikey/OmnikeyUpdate/ApplyUpdate.ps1').read_text(encoding='utf-8')
            assert script.replace('\r\n', '\n') == expected.replace('\r\n', '\n'), 'Embedded script differs from source'
            for target, arch in [('Omnikey64.exe', 'x64'), ('Omnikey32.exe', 'x86')]:
                install = temporary / arch
                install.mkdir()
                for name in ('Omnikey64.exe', 'Omnikey32.exe'):
                    (install / name).write_bytes(b'previous-' + name.encode())
                shutil.copy2(package, install / '_OmnikeyUpdate.zip')
                assert apply(script, install, target) == 0, f'Update failed for {arch}'
                assert (install / target).read_bytes() == archive.read(target), 'Wrong executable installed'
                other = 'Omnikey32.exe' if arch == 'x64' else 'Omnikey64.exe'
                assert (install / other).read_bytes() == b'previous-' + other.encode(), 'Unselected architecture changed'
                for name in [f'Rime/bin/{arch}/rime.dll', f'Rime/bin/{arch}/OmnikeyUpdate.exe', 'Rime/build/pinyin_simp.table.bin']:
                    assert (install / name).read_bytes() == archive.read(name), 'Missing or damaged Rime payload'
                assert apply(script, install, target) == 0, 'Repeated update with existing Rime/staging must work'
            # An install updated from OpenKey keeps its EXE name; it gets the Omnikey binary.
            install = temporary / 'legacy-name'
            install.mkdir()
            (install / 'OpenKey64.exe').write_bytes(b'previous-OpenKey64.exe')
            shutil.copy2(package, install / '_OmnikeyUpdate.zip')
            assert apply(script, install, 'OpenKey64.exe') == 0, 'Update failed for a legacy OpenKey install'
            assert (install / 'OpenKey64.exe').read_bytes() == archive.read('Omnikey64.exe'), 'Legacy EXE not updated'
            assert not (install / 'Omnikey64.exe').exists(), 'Legacy install got a second EXE'
            for label, valid in [('missing-data', True), ('corrupt-zip', False), ('locked-exe', None)]:
                install = temporary / label
                install.mkdir()
                target = install / 'Omnikey64.exe'
                target.write_bytes(b'keep-old-application')
                locked = None
                if valid is True:
                    with zipfile.ZipFile(install / '_OmnikeyUpdate.zip', 'w') as incomplete:
                        incomplete.writestr('Omnikey64.exe', b'incomplete-new-application')
                elif valid is False:
                    (install / '_OmnikeyUpdate.zip').write_bytes(b'not-a-zip')
                else:
                    shutil.copy2(package, install / '_OmnikeyUpdate.zip')
                    locked = kernel.CreateFileW(str(target), 0x80000000, 1, None, 3, 0x80, None)
                    assert locked != ctypes.c_void_p(-1).value, 'Could not lock test executable'
                try:
                    assert apply(script, install, 'Omnikey64.exe') != 0, f'{label} should fail'
                    assert target.read_bytes() == b'keep-old-application', 'Failed update removed the previous application'
                finally:
                    if locked is not None: kernel.CloseHandle(locked)
        print('Windows update package tests passed (x64/x86, repeat, legacy OpenKey name, missing data, corrupt ZIP, locked EXE)')
    finally:
        # The fully resolved cleanup target was checked above, inside the workspace.
        shutil.rmtree(temporary)


if __name__ == '__main__':
    main(pathlib.Path(sys.argv[1]))
