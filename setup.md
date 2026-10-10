# Install and use AntCloud Native

## Choose your installation

Use `AntCloudNative-1.0.8-macos.zip` to install the app, or `AntCloudNative-1.0.8-source.zip` to build it. Local packages are generated in `dist/`; published downloads are listed on the [releases page](https://github.com/AshrafMd-1/antcloud-native-macos/releases).

The supplied **1.0.8 Mac build targets Apple Silicon and macOS 26 or newer**. Build from source on your target Mac for a different supported architecture or build environment. A locally built app targets the macOS major version on which it was built.

You need an AntCloud account with access to a Windows VM and a network connection suitable for streaming.

## Install the Mac app

1. Extract `AntCloudNative-1.0.8-macos.zip`.
2. Quit any running AntCloud Native app.
3. Drag `AntCloudNative.app` into Applications, replacing the existing copy if updating.
4. Open the app and check **Help → About** for version 1.0.8.

The app contains its runtime libraries. Homebrew, Qt and Python are build tools and are not needed to run the installed app. This is a locally signed app, not a notarized distribution.

## Connect and play

1. End any competing stream to the same VM.
2. Sign in inside AntCloud Native. Enable remembered login if you want access and refresh tokens stored in Keychain. Access renewal is automatic while the provider permits it.
3. In **Video**, choose **720p or 1080p** within your account limit. Fresh settings select 1080p; saved choices are retained. Open **Keyboard** to adjust the three modifier mappings if needed, then Connect. The app handles startup, host discovery and pairing.
4. Use **Control+Command+Shift+P** to open local controls. Resume returns to the stream; Stop stream returns to the app.
5. To shut down the VM, save your Windows work and confirm **Shutdown VM**. Check the dashboard for the final power state. Reconnect when Connect becomes available.

The default mapping is **Command → Windows Ctrl, Option → Windows key, Control → Alt**. Each row offers None, Ctrl, Alt or Windows key. Assigning a target already used elsewhere changes its previous row to None. Choices are saved immediately and apply to the next stream; changes are disabled while a connection is in progress. With the defaults, Windows copy/paste uses Command and Windows Alt+Tab uses Control+Tab when system-key capture is enabled.

The physical **Control+Command+Shift** local controls work with every mapping. Click **Controls** for scrollable help with your current mapping, shortcuts, clipboard, Stop/Shutdown and microphone information. The stream panel also lists shortcuts below its three actions. **Stream controls hint** is enabled initially and shows the controls shortcut at the top right; **Control+Command+Shift+H** toggles it for the current stream. Tabs can be inspected while the app is busy; settings edits are locked except the microphone toggle once connected. See [README.md](README.md) for the full controls, settings and statistics reference.

For connection testing, use **720p / 60 FPS / 6 Mbps / H.264**. Bitrate choices extend to **45 and 50 Mbps**; choose a rate your network and VM can sustain. **H.265 - Experimental** (HEVC), **AV1 - Experimental** and experimental 1440p/4K resolutions depend on VM and Mac support and may fail. Settings changes apply to the next connection.

## Microphone

Windows audio plays on your Mac automatically. To send your Mac microphone, enable **Send microphone** in **Video** before Connect or once connected. **Control+Command+Shift+B** also toggles it during a connected stream. Allow macOS microphone access when prompted. Capture starts only in a connected session; ending the session stops capture and resets the option to off.

Microphone audio is **unencrypted**; enable it only on a trusted network. The notice appears while enabled. Selecting the checkbox before Connect shows no extra status line; permission, capture, sending and error feedback appears when needed. In the VM, select the provider’s recording endpoint in Windows Sound/input settings. Disabled microphone input runs no capture worker or send loop; see [README.md](README.md#audio-and-microphone) for details.

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

- **Remembered login expires:** the app renews access using the provider refresh token. An expired/revoked refresh token needs sign-in again; older access-only saved logins need one new sign-in to obtain renewal credentials. Temporary network failures retain remembered credentials.
- **Pairing failed:** wait for cleanup, then reconnect in the same app. PIN submission waits briefly for the host challenge and belongs to that connection only. It is sent once, without an automatic mutation retry.
- **Immediate reconnect fails:** wait until Connect is available after Stop. If the native host rejects Desktop launch/resume, export diagnostics; the report includes a safe host or network error code. Provider idle and competing-session policies still apply.
- **Connection failure:** check the AntCloud dashboard and competing sessions, then try the testing profile above. Export a diagnostic report if the failure repeats.
- **Experimental setting fails:** reconnect using H.264 and 720p or 1080p. Both the VM encoder and its Windows desktop/display mode must support the chosen setting. Account resolution allowance alone does not establish that support. Statistics show effective video dimensions.
- **Time left is unavailable:** no valid provider balance has arrived yet. The app fetches account time at stream start/end and every 60 seconds, and keeps the last valid account balance through temporary refresh failures. The stream overlay waits for fresh data after each Connect. The display follows provider reports, not a local countdown; use the dashboard to check the account balance.
- **Microphone permission denied:** allow AntCloud Native in **System Settings → Privacy & Security → Microphone**, then toggle microphone sending off/on in a connected stream. If Windows receives no input despite local sending, check the VM's recording endpoint; the provider receiver must support this channel.
- **Qt mismatch:** upgrade `qtbase`, `qtdeclarative`, `qtsvg`, `qttools` and `qtwebsockets` together before rebuilding.
- **Homebrew lock:** let the other installation/build finish, then retry.
- **SDK or staged-app error:** follow the builder's specific rename/retry message; it will not silently replace an unexpected SDK or staged app.

Use **Help → Export diagnostic report** to save a redacted report. Share that report, not preferences or Keychain contents. Run `bash Test.command` from the source folder for isolated local checks; [AGENTS.md](AGENTS.md#target-mac-verification) describes live checks.

## Clean up build files

After verifying the installed copy in Applications, you may delete this project's `build/`, `dist/` and `src/libs/mac/` folders. Future builds recreate them. Keep the source if you plan to rebuild.

Build-folder cleanup does not remove preferences, pairing identity or Keychain data. Use Sign out to remove remembered login. Homebrew and Apple developer tools are shared installations; remove them only if other projects no longer need them.
