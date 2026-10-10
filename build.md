# Install and build AntCloud Native

## Install the packaged app

The AntCloud Native 2.0.1 Mac package targets **Apple Silicon and macOS 26 or newer**.

1. Download `AntCloudNative-2.0.1-macos.zip` from the [releases page](https://github.com/AshrafMd-1/antcloud-native-macos/releases).
2. Extract the ZIP.
3. Quit any running AntCloud Native app.
4. Move `AntCloudNative.app` to Applications, replacing an older copy if needed.
5. Open the app and check **Help → About** for version 2.0.1.

The app bundle includes its runtime libraries. Homebrew, Qt and Python are required only when building from source.

The package is ad-hoc signed and is not notarized. If macOS blocks the first launch, open **System Settings → Privacy & Security** and use the offered **Open Anyway** action for AntCloud Native. Confirm that the prompt names the copy you just installed.

## Build requirements

Build on the Mac that will run the app. `Build-Mac.command` uses the build Mac's architecture and macOS major version as the deployment target; it does not create a universal binary.

Install:

- Apple command-line developer tools.
- [Homebrew](https://brew.sh/).
- Python 3.10 or newer.
- Qt 6.11 or newer modules: `qtbase`, `qtdeclarative`, `qtsvg`, `qttools` and `qtwebsockets`.

Install Apple's tools with:

```bash
xcode-select --install
```

The build script reuses installed compatible Homebrew Qt modules and installs a missing module without starting a general Homebrew upgrade. Keep all five Qt modules on compatible versions.

## Build from source

Extract `AntCloudNative-2.0.1-source.zip` or clone the [repository](https://github.com/AshrafMd-1/antcloud-native-macos). Run this from the repository root:

```bash
bash Build-Mac.command
```

The script:

1. checks the build tools and Qt version;
2. downloads or verifies the pinned Moonlight Mac SDK;
3. runs the core native fixture suite;
4. builds the complete Qt and Moonlight engine;
5. deploys the required frameworks and QML modules;
6. applies the app name, icon, version, minimum macOS version and microphone usage description;
7. ad-hoc signs the staged bundle; and
8. checks the signature and command-line version before replacing the previous generated output.

The result is:

```text
dist/AntCloudNative.app
```

The builder keeps one previous generated app at `dist/AntCloudNative-previous.app`. It does not replace a copy in Applications.

Open the generated app with:

```bash
bash Open-App.command
```

Check **Help → About** before replacing an installed copy. macOS Keychain may ask for access again after a locally signed rebuild.

## Run tests

Prepare the pinned SDK, then run every native and QML fixture:

```bash
python3 tools/fetch_sdk.py
bash Test.command
```

The test suite uses isolated provider endpoints and token storage. It does not contact a real AntCloud account or change the Mac Keychain.

Run the non-QML fixtures only with:

```bash
bash Test.command --core-only
```

For release work, also run the source checks used by the project:

```bash
/opt/homebrew/bin/qmllint src/app/gui/AntCloudMain.qml
bash -n Build-Mac.command Open-App.command Test.command
python3 -m py_compile tools/fetch_sdk.py tools/test_fetch_sdk.py
git diff --check
```

If Homebrew installed `qmllint` elsewhere, use the `qmllint` from the same Qt installation as `qmake`.

## Verify a generated app

The build script already validates the staged signature and embedded version. Before packaging a release, also check the generated app directly:

```bash
codesign --verify --deep --strict --verbose=2 dist/AntCloudNative.app
dist/AntCloudNative.app/Contents/MacOS/Moonlight --version
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' dist/AntCloudNative.app/Contents/Info.plist
/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' dist/AntCloudNative.app/Contents/Info.plist
/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' dist/AntCloudNative.app/Contents/Info.plist
```

Both bundle version fields and the executable must report `2.0.1`. The minimum system version must match the macOS major version used for the build.

Fixtures do not prove live provider login, Keychain behavior, native pairing, VM power control, optional codec support, VideoToolbox performance, microphone permission or a VM's available display modes. Test those behaviors with an authorized account and save Windows work before testing Shutdown VM.

## Build troubleshooting

- **Qt versions do not match:** update `qtbase`, `qtdeclarative`, `qtsvg`, `qttools` and `qtwebsockets` together, then rebuild.
- **Homebrew is locked:** let the other Homebrew process finish, then retry.
- **SDK verification fails:** do not substitute an arbitrary binary. Check the error from `tools/fetch_sdk.py` and verify the release and SHA-256 recorded in `source-provenance.json`.
- **A staged destination is rejected:** rename the unexpected file, symlink or unfinished `dist/AntCloudNative-new.app`, then rerun the builder.
- **A generated app opens with the wrong version:** quit every copy, run `bash Open-App.command`, and confirm the path and About version before changing Applications.

## Clean generated files

After verifying an installed copy, generated build output can be removed from this source tree:

- `build/`
- `dist/`
- `src/libs/mac/`

Future builds recreate them. Removing these folders does not clear preferences, pairing identity or saved Keychain credentials. Use **Sign out** in the app to request deletion of remembered login tokens.
