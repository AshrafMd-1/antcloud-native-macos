# Build and install AntCloud Native

## Requirements

- A Mac with Apple command-line developer tools.
- Homebrew and Python 3.10 or newer, used only for building.
- Qt 6.11 or newer. The builder installs missing Homebrew modules and reuses installed ones; existing modules must be compatible with each other.

The app targets the macOS major version of the build machine. The bundled Homebrew libraries do not guarantee compatibility with older macOS versions.

## Build

If Apple developer tools are missing, run `xcode-select --install` and finish their installation first.

Get the source from the [project repository](https://github.com/AshrafMd-1/antcloud-native-macos) or [releases page](https://github.com/AshrafMd-1/antcloud-native-macos/releases). Version 1.0.4 is this source update; the previous published release is [v1.0.3](https://github.com/AshrafMd-1/antcloud-native-macos/releases/tag/v1.0.3).

Open Terminal in the source folder containing `Build-Mac.command`, then run:

```bash
bash Build-Mac.command
bash Open-App.command
```

The builder verifies the downloaded streaming SDK, runs core tests, compiles the app, bundles runtime libraries, and checks its version and local signature. It writes **`dist/AntCloudNative.app`**, retaining one previous generated app. Avoid concurrent builds or Homebrew installations.

## Install or update

1. Quit any running AntCloud Native app and end any active AntCloud browser stream.
2. Open the generated app and check Help → About for the expected version. Test a connection, audio/video, Stop stream and reconnect before replacing a working installation.
3. Quit the tested app and drag `dist/AntCloudNative.app` into Applications. For an update, replace the existing copy.

The installed app contains its runtime libraries; Terminal can be closed. Preferences, pairing identity and remembered login remain in their existing per-user locations. Keychain may request access after a locally signed rebuild. No manual token copying is needed.

## First connection

Open AntCloud Native from Applications and sign in. Enable remembered login if desired. Choose **720p or 1080p**, within your account limit, then Connect; startup and pairing are handled inside the app.

Press **Control+Command+Shift+P** to open local controls. Resume returns to the stream; Stop stream disconnects. Shutdown VM sends a request after confirmation—save Windows work first, then check the dashboard for the final power state. Reconnect when Connect becomes available.

Use **720p / 60 FPS / 6 Mbps** when testing a connection problem. 1440p and 4K are experimental and may work inconsistently. H.264 is the default codec; H.265/HEVC and AV1 need compatible VM/Mac support. You can adjust FPS and choose a bitrate from 2–40 Mbps. Changes take effect on the next connection.

Windows shortcuts now use **Command** for copy/paste and similar actions. Command maps to Windows Ctrl, Option to Alt, and Control to the Windows key. This replaces the modifier mapping in v1.0.3; other keys and local stream shortcuts stay the same. See [README.md](README.md) for all controls, settings and statistics.

## Troubleshooting

- **Qt version mismatch:** upgrade `qtbase`, `qtdeclarative`, `qtsvg`, `qttools` and `qtwebsockets` together before rebuilding.
- **Homebrew lock:** let the other install/build finish, then retry.
- **Unfinished staged app:** follow the builder's message to rename `dist/AntCloudNative-new.app`, then retry.
- **Connection issue:** check the AntCloud dashboard and end other active streams. Retry at 720p/60 FPS/6 Mbps. A new attempt clears the previous issue; a genuine new failure appears under Connection issue.
- **Repeated failure:** use Help → Export diagnostic report. Share the redacted report, not preferences or Keychain contents.

Run `bash Test.command` from the source folder for isolated local tests. The target-Mac checklist in [AGENTS.md](AGENTS.md#live-mac-checklist) covers native input, Keychain, pairing and shutdown/reconnect checks that fixtures cannot prove.

## Reclaim build space

After checking the copy in Applications, you may delete this project's `build/`, `dist/` and `src/libs/mac/` folders. This does not affect the installed app; a future build recreates them. Keep the source if you plan to rebuild.

Use Sign out to remove the remembered token. Build-folder cleanup does not remove preferences, pairing identity or Keychain data. Homebrew and Apple developer tools are shared installations; remove them only if you no longer need them for other projects.
