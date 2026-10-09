"""Isolated SDK receipt regressions; no network or real SDK writes."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import fetch_sdk


class ReceiptTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.target = Path(self.directory.name) / 'sdk'
        (self.target / 'lib').mkdir(parents=True)
        library = self.target / 'lib/example.dylib'
        library.write_bytes(b'fixture-library')
        self.receipt = {
            'archive_sha256': fetch_sdk.DIGEST,
            'files': {'lib/example.dylib': hashlib.sha256(library.read_bytes()).hexdigest()},
        }
        self.write_receipt()
        self.override = patch.object(fetch_sdk, 'TARGET', self.target)
        self.override.start()
        self.addCleanup(self.override.stop)

    def write_receipt(self):
        (self.target / fetch_sdk.RECEIPT).write_text(json.dumps(self.receipt))

    def test_exact_inventory_is_reused(self):
        self.assertTrue(fetch_sdk.existing_sdk())

    def test_unlisted_library_is_rejected(self):
        (self.target / 'lib/extra.dylib').write_bytes(b'extra')
        with self.assertRaisesRegex(SystemExit, 'inventory'):
            fetch_sdk.existing_sdk()

    def test_changed_and_missing_files_are_rejected(self):
        library = self.target / 'lib/example.dylib'
        library.write_bytes(b'changed')
        with self.assertRaisesRegex(SystemExit, 'files have changed'):
            fetch_sdk.existing_sdk()
        library.unlink()
        with self.assertRaisesRegex(SystemExit, 'inventory'):
            fetch_sdk.existing_sdk()

    def test_symlinked_directory_is_rejected(self):
        real = self.target / 'lib'
        renamed = self.target / 'real-lib'
        real.rename(renamed)
        real.symlink_to(renamed, target_is_directory=True)
        with self.assertRaisesRegex(SystemExit, 'symbolic link'):
            fetch_sdk.existing_sdk()

    def test_symlinked_target_is_rejected(self):
        real = self.target.with_name('real-sdk')
        self.target.rename(real)
        self.target.symlink_to(real, target_is_directory=True)
        with self.assertRaisesRegex(SystemExit, 'symbolic link'):
            fetch_sdk.existing_sdk()

    def test_receipt_cannot_escape_target(self):
        self.receipt['files'] = {'../escape': 'not-a-hash'}
        self.write_receipt()
        with self.assertRaises(SystemExit):
            fetch_sdk.existing_sdk()


if __name__ == '__main__':
    unittest.main()
