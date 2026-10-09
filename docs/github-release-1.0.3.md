# AntCloud Native 1.0.3

An unofficial native macOS client for starting and streaming an AntCloud Windows VM, using the Moonlight streaming engine.

## Changes

- Experimental resolutions show exact dimensions: 2560 × 1440 and 3840 × 2160. 720p and 1080p remain the standard choices.
- Short account/session forms have balanced vertical padding; taller connection views scroll. Session and VM power sections use consistent spacing.
- Disconnect queues cancellation once, keeping deliberate Stop/Shutdown cleanup distinct from genuine connection failures.
- Native app and Mac bundle version fields derive from one release version file.
- The complete source repo includes Git ignore rules, build/setup documentation, license/provenance notices and a manual GitHub publishing guide.

Includes the previous reconnect, stream-advisory cleanup and statistics averaging fixes. The unsupported 1440×900 preset remains removed.

## Build and use

Extract or clone the source. From the project root on your Mac:

```bash
bash Build-Mac.command
bash Open-App.command
```

Follow `setup.md` for installation and updates. After building, drag `dist/AntCloudNative.app` into Applications. Normal use is inside the app; Terminal is only needed for building.

Start with 720p / 60 FPS / 6 Mbps. Use Control+Command+Shift+P for local controls. Windows shortcuts use Control; Command maps to Alt and Option maps to Windows.

## Verification and distribution

This is a source release with a Mac build script. Automated fixtures and the full Linux build are checked locally; Mac native styling, Keychain, decoding and live VM power transitions require the checklist in `docs/mac-test.md`. Public Mac binary distribution additionally requires Apple signing/notarization and target-device verification. The source includes GPL and dependency notices.
