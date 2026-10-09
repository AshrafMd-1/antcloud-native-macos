# AntCloud Native 1.0.3 setup

## Updating your working installation

1. Quit AntCloud Native. Keep the installed working app until the new build passes.
2. In Finder, rename the old Downloads/AntCloudNative folder to AntCloudNative-previous. Unzip the new source package so the new folder is Downloads/AntCloudNative.
3. Build the new folder with the commands below. Already installed Qt modules are reused. The fresh folder needs the verified streaming SDK download again (about 28 MB).
4. Open the new `dist/AntCloudNative.app`. Check that its resolution menu shows `2560 × 1440 · Experimental` and `3840 × 2160 · Experimental`; About reports 1.0.3. After checking it, quit the app, drag it into Applications and replace the old app. Preferences, pairing identity and remembered login use the same locations; no manual token copying is needed. Keychain may request access because this is a locally signed rebuild.
5. Test **Connect → local controls → Shutdown VM → wait until Connect is enabled → Connect**, without quitting the app. Check the dashboard to distinguish a powered-off VM booting again from a host discovery failure.

## First build

Run the commands from the project root containing `Build-Mac.command`. The supplied ZIP extracts as `AntCloudNative`; a GitHub clone or automatic source ZIP may use a different folder name.

Requirements: your Mac, Apple command-line developer tools, Homebrew and Python 3.10+. Use the current Qt 6.11+ modules together. This source builder targets the macOS major version it runs on because Homebrew Qt bottles can require that OS; it does not promise an older-Mac compatible bundle.

If Apple developer tools are missing:

```bash
xcode-select --install
```

Finish that installation, then:

```bash
cd ~/Downloads/AntCloudNative
bash Build-Mac.command
bash Open-App.command
```

Build-Mac installs only missing Qt modules, checks the streaming SDK checksum, runs the core fixtures, compiles the app and bundles libraries. It refreshes existing build files and checks the built app's version and local signature before replacing the generated app. It keeps one previous generated app. Build output stays in this folder. Do not run two builds/Homebrew installations at the same time; if Homebrew reports a lock, let the other process finish before retrying.

If an earlier 1.0.2 build stopped with `Library not loaded: @loader_path/../Frameworks/libssl.3.dylib` in test-seeker, use this 1.0.3 source package. Its Test.command selects the existing SDK libraries only for test processes. It does not require another OpenSSL install or change your shell profile. Rebuild normally; no cache deletion is needed.

The app is `dist/AntCloudNative.app`. Put it in Applications. Terminal can now be closed. The local signature is suitable for your own build; public distribution needs Apple Developer ID signing and notarization.

## Daily use

1. End any old Terminal controller and active AntCloud browser stream.
2. Open AntCloudNative from Applications. Sign in; enable remembered login if wanted.
3. Start with 1280×720, 60 FPS and 6 Mbps. Connect. The displayed stages reflect connection events; elapsed time is not a guessed countdown.
4. In games, keep Relative mouse enabled. Hold Control+Command+Shift and press **P** to open local controls. Resume returns to the game, Stop stream disconnects, Shutdown VM sends a confirmed shutdown request.
5. Windows shortcuts use **Control**, not Command: Control+A/C/V/X/Z. Command maps to Alt; Option maps to Windows. The full local shortcut list is in README.md.

Stop stream sends no shutdown request, but AntCloud may stop an idle VM. Chrome handoff depends on the provider. After shutdown, check the dashboard for the final power state. A new connection clears the previous connection issue and shutdown status. No PIN prompt or external Moonlight app is needed.

720p and 1080p are the standard choices. 1440p (2560×1440) and 4K (3840×2160) are experimental. A stream advisory applies only to the active stream and disappears after stopping; it is separate from a real connection error. Expected closure after your Shutdown request should show Disconnected and VM power information, without a red connection error.

## Basic commands

| Command from the source folder | Purpose |
| --- | --- |
| `bash Build-Mac.command` | Build/update the self-contained app |
| `bash Open-App.command` | Open the generated app |
| `bash Test.command` | Run core and interface fixtures |
| `bash Test.command --core-only` | Run core fixtures without UI checks |
| `du -sh build dist src/libs/mac` | Check the project's build/app/SDK space |

Help → Export diagnostic report writes a small, redacted report only to your chosen location. Help → Clear diagnostic history clears the bounded in-memory history. No routine logs are created. A report excludes passwords, tokens, PINs and VM addresses.

## Reclaim disk space

After the new app works from Applications, use Finder to delete the old AntCloudMac helper folder (including its virtual environment and old token cache) and any unused stock Moonlight app. Retain a source ZIP for future development.

You may delete this project's `build/`, `dist/` and `src/libs/mac/` folders after installing the app. The installed app contains its runtime libraries; deleting build output does not break that copy. Future builds will recreate/download these folders. Preferences and pairing identity remain in normal macOS per-user settings; remembered login is a separate Keychain item. Use Sign out to remove the remembered token. These small settings are not growing diagnostic logs.

If Qt build tools were installed solely for this project and you will not compile again, remove those specific formulae with Homebrew:

```bash
brew uninstall --formula qttools qtwebsockets qtsvg qtdeclarative qtbase
brew autoremove --dry-run
```

Review the dry-run before removing dependencies. Do not force-remove libraries used by other apps. The custom app does not need the stock Moonlight cask; if `brew list --cask` lists it and you want to remove it, use `brew uninstall --cask moonlight`.

The clang/make commands compile inside this project's build folder. They do not install extra apps in your home folder. Homebrew and Apple tools have their own normal installations and caches.

## If connection fails

A genuine failure appears in red under Connection issue. The app returns to Disconnected and a fresh Connect clears the prior message. Shutdown information appears under VM power, separately. Confirm the dashboard state and that there is no other active stream. Try 720p/60 FPS/6 Mbps. If it fails again, export the bounded diagnostic report; do not share preferences or Keychain contents.

The 1.0.3 source is verified by the Linux build and automated fixtures. Build on the target Mac and follow docs/mac-test.md to verify native style, Keychain, video/input and live shutdown/reconnect before discarding the working install or sharing a binary.

## Publish the source

The full repo includes `.gitignore`, README/setup, GPL license/provenance notices, pinned public dependency source and review records. Follow [docs/github-publishing.md](docs/github-publishing.md) for creating `antcloud-native-macos` and a single `v1.0.3` release. Copy-ready release notes are in [docs/github-release-1.0.3.md](docs/github-release-1.0.3.md).
