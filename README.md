# AntCloud Native

AntCloud Native is an unofficial macOS app for connecting to an AntCloud Windows VM. It brings account sign-in, VM connection, streaming settings, video, audio and local controls into one desktop application.

Main release **1.0.5** · [Project repository](https://github.com/AshrafMd-1/antcloud-native-macos) · [Releases](https://github.com/AshrafMd-1/antcloud-native-macos/releases)

## What it does

Use the app to access your Windows desktop, play games with relative mouse capture, and manage your connection from macOS. It provides:

- Account sign-in and optional remembered login through macOS Keychain.
- VM startup, native host discovery and pairing, with visible connection progress.
- Fullscreen or windowed video, audio and keyboard/mouse input.
- Resolution, frame rate, bitrate and video codec settings.
- Stream statistics and provider-reported remaining account time.
- Local Resume, Stop stream and confirmed Shutdown VM controls.

AntCloud Native is a personal project based on Moonlight 6.2.0. It is not affiliated with or endorsed by AntCloud or Moonlight. Provider availability and account limits still apply.

## How it works

The Qt interface handles sign-in and settings. A C++ worker manages AntCloud account and session requests separately from the interface. Once the VM is available, the app discovers its native streaming host, pairs this Mac and launches the Windows desktop through Moonlight's SDL video, audio and input engine.

Connection progress follows provider session → VM startup → host contact → pairing → video and audio. The regular app window returns when streaming ends. Connect becomes available after the current session finishes cleaning up.

## Getting started

Install the packaged Mac app or build it from source using [setup.md](setup.md). Open AntCloud Native, sign in, choose settings within your account limit and select **Connect**.

**720p and 1080p** are the standard resolution choices. The default settings are **720p / 60 FPS / 6 Mbps / H.264**, with fullscreen, relative mouse and statistics enabled. That profile is also useful for testing a connection problem; choose settings suited to your VM and network for normal use.

## Stream settings

Settings take effect on the next connection.

- **Resolution:** 1280 × 720 and 1920 × 1080 are standard. 2560 × 1440 and 3840 × 2160 are experimental and may work inconsistently on a given VM. Account resolution limits are enforced.
- **Frame rate:** 30, 60 or 120 FPS.
- **Bitrate:** 2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45 or 50 Mbps. Higher values need more network and host capacity; the provider may limit the delivered rate.
- **Video codec:** H.264 is the default. **H.265 - Experimental** (HEVC) and **AV1 - Experimental** require compatible host encoding and Mac decoding. The engine can fall back to a supported codec.
- **Fullscreen:** preserves aspect ratio; different Mac and VM display proportions can leave bars.
- **Relative mouse:** suits continuous camera movement in games. Absolute mouse mode suits the Windows desktop.

A codec is a way to compress video for transmission. Selecting a codec or resolution does not add that capability to the VM. Try H.264 and a standard resolution if an experimental choice fails.

## Keyboard and local controls

While streaming, the Mac modifier mapping is:

- **Command → Windows Ctrl**
- **Option → Alt**
- **Control → Windows key**

Use **Command+A/C/V/X/Z** for Windows select-all, copy, paste, cut and undo. **Option+Tab** sends Windows Alt+Tab when system-key capture is enabled. macOS handles Fn/Globe.

Hold **Control + Command + Shift**, then press:

- **P:** release capture and open local controls; Resume returns to the stream.
- **Q:** stop streaming and return to the app.
- **Z:** toggle mouse capture; releasing capture opens local controls.
- **X:** toggle fullscreen.
- **S:** show or hide statistics.
- **M:** switch relative game mouse / absolute desktop mouse.
- **V:** type Mac clipboard text into the remote application, if the host supports it.
- **C:** show or hide the cursor in absolute mouse mode.
- **D:** minimize the stream window.
- **L:** toggle pointer confinement in desktop mouse mode.
- **K:** toggle capture of system keyboard shortcuts.
- **E:** stop streaming and exit the app.

Clipboard typing transfers text; it does not synchronize clipboards or transfer files. Command+Q quits when the regular Mac app window has focus. Use the local stream controls while playing.

## Disconnect and VM shutdown

**Disconnect / Stop stream** ends this Mac's connection and returns to the app. Quitting also ends the connection. These actions send no explicit VM shutdown command.

**Shutdown VM** sends one provider shutdown request after confirmation. Save your Windows work first. “Shutdown requested” describes the request; check the AntCloud dashboard for the final power state.

You can reconnect when Connect becomes available. A stopped VM may need to boot again. AntCloud's idle and competing-session policies determine whether the VM remains running and whether another client can connect.

## Understanding the statistics

- **RTT:** round-trip network time to the streaming host, not total input latency or the game's multiplayer ping.
- **FPS:** incoming video frame rate, not the game's rendering rate. Static desktop content may produce fewer frames.
- **Loss:** missing video frames, not raw packet loss. A displayed 0.0% is rounded.
- **Mbps:** recently received compressed video payload; audio, headers and recovery traffic are excluded.
- **Dimensions:** decoded video dimensions when available, otherwise requested dimensions.
- **Time left:** latest remaining minutes reported by AntCloud. The account view shows “unavailable” and the overlay shows N/A when there is no valid value. The app does not estimate a local countdown.

The bitrate setting is a requested bandwidth budget. Scene complexity, recovery traffic and provider limits can make the measured video rate lower. Compare moving gameplay at the same resolution and frame rate when adjusting bitrate; reconnect between changes.

## Privacy and diagnostics

Passwords are never saved. Remembered login stores the access token in macOS Keychain; Sign out requests its removal. Stream preferences and pairing identity use macOS per-user settings.

The app keeps up to 64 safe diagnostic messages in memory, each limited to 512 characters. **Help → Export diagnostic report** saves a redacted report only to a location you choose. **Help → Clear diagnostic history** clears the history. Reports exclude credentials, PINs, VM addresses and raw provider traffic. Do not share preferences or Keychain contents.

## Development and license

[AGENTS.md](AGENTS.md) explains the architecture, source map, development contracts, tests and packaging. The complete modified engine and pinned public dependency source are in `src/`; `source-provenance.json` records revisions and the SDK digest, and `source-manifest.json` records source hashes.

Moonlight's GPL-3.0 license, copyright notices and dependency licenses are retained; see [LICENSE](LICENSE) and [src/LICENSE](src/LICENSE). AntCloud account, session, UI and input additions are GPL-3.0-or-later. Combined distributions must satisfy upstream GPL-3.0 requirements and preserve dependency/SDK notices.

The local Mac app is ad-hoc signed, not notarized. Automated tests isolate real accounts and provider access; live Keychain, pairing, codec support and VM power behavior require testing on the target Mac and account.
