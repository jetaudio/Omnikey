"""Exercise the real replacement helper with signed disposable bundles.
Never touches an installed app or launches a GUI. Failure injection replaces
only the open/alert tools in a temporary copy of the production helper.
"""
import pathlib
import plistlib
import shutil
import subprocess
import tempfile
import unittest

HELPER = pathlib.Path(__file__).resolve().parents[1] / 'Sources/Omnikey/macOS/ModernKey/update-install.sh'

class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='OmnikeyUpdate-test-')
        self.root = pathlib.Path(self.temp.name) / 'folder with spaces'
        self.root.mkdir()
        self.target = self.root / "Omnikey's.app"
        self.stage = self.root / '.Omnikey-update-test.app'
        self.work = pathlib.Path(self.temp.name) / 'OmnikeyUpdate-work'
        self.work.mkdir()
        self.bundle(self.target, '50')
        self.bundle(self.stage, '51')

    def tearDown(self):
        self.temp.cleanup()

    def bundle(self, path, version):
        (path / 'Contents/MacOS').mkdir(parents=True)
        with (path / 'Contents/Info.plist').open('wb') as f:
            plistlib.dump({'CFBundleIdentifier': 'io.github.jetaudio.omnikey',
                          'CFBundleExecutable': 'Omnikey', 'CFBundleVersion': version,
                          'CFBundlePackageType': 'APPL'}, f)
        shutil.copy('/bin/echo', path / 'Contents/MacOS/Omnikey')
        subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(path)], check=True, capture_output=True)

    def run_helper(self, helper=HELPER, mode='no-relaunch'):
        # This PID cannot exist on macOS; no active application is involved.
        return subprocess.run(['/bin/bash', str(helper), str(self.stage), str(self.target),
                               '2147483647', str(self.work), mode], capture_output=True, text=True)

    def version(self, path):
        with (path / 'Contents/Info.plist').open('rb') as f:
            return plistlib.load(f)['CFBundleVersion']

    def test_install_keeps_previous_and_cleans_temporary_files(self):
        result = self.run_helper()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.version(self.target), '51')
        backups = list(self.root.glob('*.previous-*.app'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(self.version(backups[0]), '50')
        self.assertFalse(self.stage.exists())
        self.assertFalse(self.work.exists())

    def test_tampered_app_cannot_replace_current_app(self):
        (self.stage / 'Contents/MacOS/Omnikey').write_bytes(b'tampered')
        self.assertNotEqual(self.run_helper().returncode, 0)
        self.assertEqual(self.version(self.target), '50')
        self.assertFalse(list(self.root.glob('*.previous-*.app')))

    def test_failed_relaunch_restores_previous_bundle(self):
        helper = pathlib.Path(self.temp.name) / 'injected.sh'
        helper.write_text(HELPER.read_text().replace('/usr/bin/open', '/usr/bin/false')
                          .replace('/usr/bin/osascript', '/usr/bin/true'))
        result = self.run_helper(helper, 'relaunch')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.version(self.target), '50')
        self.assertFalse(list(self.root.glob('*.previous-*.app')))

    def test_symlink_target_is_rejected_without_changes(self):
        actual = self.root / 'actual.app'
        self.target.rename(actual)
        self.target.symlink_to(actual)
        self.assertNotEqual(self.run_helper().returncode, 0)
        self.assertEqual(self.version(actual), '50')
        self.assertTrue(self.stage.exists())

if __name__ == '__main__':
    unittest.main()
