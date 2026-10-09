# Install and use AntCloud Native

## Choose your installation

Use `AntCloudNative-1.0.5-macos.zip` to install the app, or `AntCloudNative-1.0.5-source.zip` to build it. Local packages are generated in `dist/`; published downloads are listed on the [releases page](https://github.com/AshrafMd-1/antcloud-native-macos/releases).

The supplied **1.0.5 Mac build targets Apple Silicon and macOS 26 or newer**. Build from source on your target Mac for a different supported architecture or build environment. A locally built app targets the macOS major version on which it was built.

You need an AntCloud account with access to a Windows VM and a network connection suitable for streaming.

## Install the Mac app

1. Extract `AntCloudNative-1.0.5-macos.zip`.
2. Quit any running AntCloud Native app.
3. Drag `AntCloudNative.app` into Applications, replacing the existing copy if updating.
4. Open the app and check **Help → About** for version 1.0.5.

The app contains its runtime libraries. Homebrew, Qt and Python are build tools and are not needed to run the installed app. This is a locally signed app, not a notarized distribution.

## Connect and play

1. End any competing stream to the same VM.
2. Sign in inside AntCloud Native. Enable remembered login if you want the token stored in Keychain.
3. Choose **720p or 1080p** within your account limit, then Connect. The app handles startup, host discovery and pairing.
4. Use **Control+Command+Shift+P** to open local controls. Resume returns to the stream; Stop stream returns to the app.
5. To shut down the VM, save your Windows work and confirm **Shutdown VM**. Check the dashboard for the final power state. Reconnect when Connect becomes available.

Windows copy/paste and similar shortcuts use **Command**. Option maps to Alt, and Control maps to the Windows key. See [README.md](README.md) for the full controls, settings and statistics reference.

For connection testing, use **720p / 60 FPS / 6 Mbps / H.264**. Bitrate choices extend to **45 and 50 Mbps**; choose a rate your network and VM can sustain. **H.265 - Experimental** (HEVC), **AV1 - Experimental** and experimental 1440p/4K resolutions depend on VM and Mac support and may fail. Settings changes apply to the next connection.

## Build from source

Requirements: Apple command-line developer tools, Homebrew, Python 3.10 or newer, and compatible Qt 6.11+ modules. The builder installs missing Homebrew Qt modules and reuses those already installed.

If the Apple tools are missing, run `xcode-select --install` and finish that installation first. Install Homebrew from [brew.sh](https://brew.sh/) if needed.

Extract the source ZIP or clone the [repository](https://github.com/AshrafMd-1/antcloud-native-macos). From the folder containing `Build-Mac.command`, run:

```bash
bash Build-Mac.command
```

The builder verifies the streaming SDK, runs core tests, compiles the complete app, bundles libraries and checks the local signature and version. The result is **`dist/AntCloudNative.app`**.

Open the generated app with `bash Open-App.command`. Check it before replacing a working copy in Applications. The builder retains one previous generated app and does not replace your installed copy. Avoid concurrent builds or Homebrew installations.

Updates retain preferences, pairing identity and remembered login. Keychain may request access after a locally signed rebuild. Check **Help → About** to confirm which copy is running.

## Troubleshooting

- **Connection failure:** check the AntCloud dashboard and competing sessions, then try the testing profile above. Export a diagnostic report if the failure repeats.
- **Experimental setting fails:** reconnect using H.264 and 720p or 1080p. The host must support the chosen setting.
- **Time left is unavailable:** the provider has not supplied a valid value. Use the dashboard to check the account balance.
- **Qt mismatch:** upgrade `qtbase`, `qtdeclarative`, `qtsvg`, `qttools` and `qtwebsockets` together before rebuilding.
- **Homebrew lock:** let the other installation/build finish, then retry.
- **SDK or staged-app error:** follow the builder's specific rename/retry message; it will not silently replace an unexpected SDK or staged app.

Use **Help → Export diagnostic report** to save a redacted report. Share that report, not preferences or Keychain contents. Run `bash Test.command` from the source folder for isolated local checks; [AGENTS.md](AGENTS.md#target-mac-verification) describes live checks.

## Clean up build files

After verifying the installed copy in Applications, you may delete this project's `build/`, `dist/` and `src/libs/mac/` folders. Future builds recreate them. Keep the source if you plan to rebuild.

Build-folder cleanup does not remove preferences, pairing identity or Keychain data. Use Sign out to remove remembered login. Homebrew and Apple developer tools are shared installations; remove them only if other projects no longer need them.
