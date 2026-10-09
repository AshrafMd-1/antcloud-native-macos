#!/usr/bin/env python3
"""Fetch the pinned Mac SDK once, verify it, and keep only the extracted SDK."""
import hashlib
import json
from pathlib import Path
import shutil
import stat
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'src/libs/mac'
DIGEST = '39fab7c95f5d513a6eb3bb19c66f1ae66bbf41dfe4118f82fabe3b46cc502d17'
URL = 'https://github.com/moonlight-stream/moonlight-qt-deps/releases/download/v19/macOS-universal.zip'
RECEIPT = '.antcloud-sdk-receipt.json'


def digest_file(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def existing_sdk():
    receipt = TARGET / RECEIPT
    if not TARGET.exists():
        return False
    if not receipt.is_file():
        raise SystemExit('The SDK directory has no verification receipt. Rename src/libs/mac and retry.')
    data = json.loads(receipt.read_text())
    if data.get('archive_sha256') != DIGEST:
        raise SystemExit('SDK verification receipt does not match this release.')
    for name, value in data['files'].items():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise SystemExit('SDK receipt contains an unsupported path.')
        path = TARGET / relative
        if path.is_symlink() or not path.is_file() or digest_file(path) != value:
            raise SystemExit('SDK files have changed. Rename src/libs/mac and retry.')
    print('Verified existing v19 SDK; no download needed.')
    return True


def main():
    if existing_sdk():
        return
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sdk-download-', dir=build) as temporary:
        directory = Path(temporary)
        archive = directory / 'sdk.zip'
        print('Downloading the pinned v19 Mac SDK (about 28 MB)…')
        with urllib.request.urlopen(URL, timeout=30) as response, archive.open('wb') as output:
            total = 0
            for chunk in iter(lambda: response.read(1024 * 1024), b''):
                total += len(chunk)
                if total > 64 * 1024 * 1024:
                    raise SystemExit('SDK download exceeded the expected size limit.')
                output.write(chunk)
        if digest_file(archive) != DIGEST:
            raise SystemExit('SDK checksum did not match. Nothing was installed.')
        staged = directory / 'extracted'
        staged.mkdir()
        hashes = {}
        with zipfile.ZipFile(archive) as source:
            entries = source.infolist()
            if sum(item.file_size for item in entries) > 1024 * 1024 * 1024:
                raise SystemExit('SDK extraction exceeded its size limit.')
            for item in entries:
                relative = Path(item.filename)
                if relative.is_absolute() or '..' in relative.parts or not relative.parts:
                    raise SystemExit('SDK contains an unsupported path.')
                path = staged / relative
                if not path.resolve().is_relative_to(staged.resolve()):
                    raise SystemExit('SDK path leaves the extraction directory.')
                mode = item.external_attr >> 16
                if stat.S_ISLNK(mode):
                    raise SystemExit('This pinned SDK should contain regular files, not links.')
                if item.is_dir():
                    path.mkdir(parents=True, exist_ok=True)
                    continue
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.open(item) as original, path.open('wb') as output:
                    shutil.copyfileobj(original, output)
                path.chmod(mode & 0o777 or 0o644)
                hashes[item.filename] = digest_file(path)
        (staged / RECEIPT).write_text(json.dumps({'archive_sha256': DIGEST, 'files': hashes}, sort_keys=True))
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        if TARGET.exists():
            raise SystemExit('SDK directory appeared during download; it was not replaced.')
        staged.rename(TARGET)
    print('SDK verified and extracted. Temporary download removed.')


if __name__ == '__main__':
    main()
