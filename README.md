# AntCloud Native

An unofficial macOS client for streaming an AntCloud Windows VM, built on Moonlight 6.2.0. Sign in, connect, play, and disconnect from one native app.

The account and settings interface uses Qt, provider networking runs on a C++ worker thread, and Moonlight's SDL engine handles video, audio and input. Terminal is needed only to build the app. Normal use needs no Python controller, browser stream or separate Moonlight installation.

Current source version: **1.0.4**. This is a personal, experimental client and is not affiliated with or endorsed by AntCloud or Moonlight. AntCloud's provider integration is based on observed behavior and may change.

Project: [antcloud-native-macos](https://github.com/AshrafMd-1/antcloud-native-macos) · [Published releases](https://github.com/AshrafMd-1/antcloud-native-macos/releases). The previous published release is [v1.0.3](https://github.com/AshrafMd-1/antcloud-native-macos/releases/tag/v1.0.3).

## Changes in 1.0.4

The Mac modifier mapping now follows familiar copy/paste shortcuts: Command→Windows Ctrl, Option→Alt, Control→Windows key. Version 1.0.3 used Command→Alt, Option→Windows and Control→Ctrl. Other keys, including Backspace and Delete, retain their existing behavior. Local Control+Command+Shift shortcuts stay the same.

This update adds provider-reported remaining minutes, codec selection and more bitrate choices, fixes keyboard navigation in the shutdown confirmation, and tightens SDK file verification. Project documentation is consolidated into README, setup and AGENTS; the complete source and Git history replace archived patch documents.

## Getting started

Follow [setup.md](setup.md) to build and install the app on your Mac. The source includes pinned public dependencies; the build downloads and verifies the Mac streaming SDK separately. No prebuilt Mac app is included.

Choose **720p or 1080p** for normal use, within your account limit. **720p / 60 FPS / 6 Mbps** is a conservative testing profile. Sign in inside the app and select Connect. The app handles VM startup, host discovery and pairing before opening the stream. There is no Terminal PIN prompt.

## Features

- Native video and audio, fullscreen, and relative mouse capture for continuous camera movement.
- Windows keyboard shortcuts and local stream controls.
- Resolution, frame rate, bitrate and codec settings, with compact stream statistics and remaining account time.
- Connection progress through provider session, VM startup, host contact, pairing and streaming.
- Resume, Stop stream and confirmed Shutdown VM controls.
- Optional remembered login through macOS Keychain.
- Bounded diagnostic history in memory, exported only when requested.

## Stream controls

**Disconnect / Stop stream** ends this Mac's connection and returns to the app. **Quit** closes the app. Neither sends an explicit VM shutdown request.

**Shutdown VM** sends one shutdown request after local confirmation. Save your Windows work first. “Shutdown requested” describes the request, not a verified power state; check the AntCloud dashboard for the final state.

Reconnect when Connect becomes available. A powered-off VM may need to boot again. AntCloud may stop an idle VM after disconnection, and switching between native and browser streams depends on the provider's session policy.

## Keyboard and mouse

While streaming, the physical modifier mapping is:

- **Command → Windows Ctrl**
- **Option → Alt**
- **Control → Windows key**
- **Shift → Shift**

Use **Command+A/C/V/X/Z** for Windows select-all, copy, paste, cut and undo. **Option+Tab** becomes Windows Alt+Tab when system-key capture is enabled. macOS handles Fn/Globe.

Hold **Control + Command + Shift**, then press a key for local controls:

| Key | Action |
| --- | --- |
| P | Release input capture and open local controls; Resume returns to the game |
| Z | Toggle mouse capture; releasing opens local controls |
| Q | Stop stream and return to the app |
| X | Toggle fullscreen |
| S | Show or hide statistics |
| M | Switch relative game mouse / absolute desktop mouse |
| V | Type Mac clipboard text into the remote app, if supported by the host |
| C | Show or hide the cursor in absolute mouse mode |
| D | Minimize the stream window |
| L | Toggle pointer confinement in desktop mouse mode |
| K | Toggle capture of system keyboard shortcuts |
| E | Stop stream and exit the app |

Relative mouse mode suits games; absolute mode suits the Windows desktop. Clipboard typing does not synchronize clipboards or transfer files. Command+Q quits when the regular Mac app window has focus; use the local controls while streaming.

## Stream settings

Available resolutions, subject to your account limit:

- **1280 × 720 (720p)** — standard.
- **1920 × 1080 (1080p)** — standard.
- **2560 × 1440 (1440p)** — experimental.
- **3840 × 2160 (4K)** — experimental.

Choose **30, 60 or 120 FPS** and **2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35 or 40 Mbps**. Changes apply to the next connection. Higher settings need enough host and network capacity. Experimental 1440p/4K resolutions may work inconsistently or fail on a given VM; selecting a stream resolution does not install Windows display modes. Fullscreen preserves aspect ratio, which can leave bars.

**Video codec** selects how video is compressed. **H.264** is the default and recommended starting point. **H.265/HEVC** and **AV1** are experimental choices that need compatible host encoding and Mac decoding. The existing engine negotiates supported formats and can fall back; selecting a codec does not add support to the provider. These choices are ready to use when the VM supports them, without changing this settings UI.

## Reading the statistics

- **RTT:** estimated round-trip time to the streaming host, not total input-to-picture latency or the game's multiplayer ping.
- **FPS:** incoming video frame rate averaged over roughly two seconds, not the game's rendering rate. Static desktop content may produce fewer frames.
- **Loss:** missing video frames inferred from frame-number gaps, not raw packet loss. A displayed 0.0% is rounded.
- **Mbps:** compressed video payload received over completed 250 ms samples covering up to 2.5 seconds. Audio, headers and recovery traffic are excluded.
- **Time left:** latest remaining minutes reported by AntCloud. Account sign-in can supply a balance; the stream uses provider updates. Unavailable data shows N/A. This is not a locally estimated countdown or a guarantee of billing/power behavior.
- **Dimensions:** decoder-reported dimensions when available, otherwise the requested dimensions; these differ from the Mac screen size.

The bitrate setting is a request/bandwidth budget. Recovery traffic, host limits and scene complexity can make the measured video rate lower. A static desktop using little bandwidth does not establish a fault. To compare settings, keep resolution, FPS and network unchanged, reconnect after each bitrate change, and watch the same moving gameplay for 20–30 seconds. Increased RTT, loss or stutter at higher bitrates can indicate congestion. No measured native/browser performance comparison is claimed.

## Privacy and diagnostics

Passwords are never saved. Remembered login stores only the account token in Keychain; Sign out removes it. Preferences and the pairing identity use normal macOS per-user settings.

Diagnostic history holds at most 64 safe messages in memory. Each entry is limited to 512 characters, including its elapsed-time prefix. **Help → Export diagnostic report** saves a redacted report to a location you choose; **Help → Clear diagnostic history** clears the history. There are no automatic diagnostic log files. Reports exclude passwords, tokens, PINs, VM addresses and raw provider payloads. Do not share preferences or Keychain contents.

## Source and development

[AGENTS.md](AGENTS.md) contains the architecture, development contracts, test commands, live Mac checklist, validation record and source packaging rules.

The complete modified engine and pinned public dependency sources are in `src/`. Native account/session code is in `src/app/antcloud/`, and the interface is `src/app/gui/AntCloudMain.qml`. `source-provenance.json` records upstream revisions and the SDK digest; `source-manifest.json` records source hashes. The complete modified source is included directly; change history is maintained in the project repository. No `docs/` folder or patch archive is required to build.

Automated fixtures isolate real accounts, Keychain and provider access. Previous Linux builds and fixtures passed; they cannot establish live Mac decoding, Keychain access, pairing or VM power behavior. Local builds use an ad-hoc signature and are not notarized distributions.

## License and attribution

AntCloud Native is based on Moonlight PC 6.2.0. Moonlight's GPL-3.0 license, copyright notices and dependency licenses are retained in `src/`; see [LICENSE](LICENSE) and [src/LICENSE](src/LICENSE). New AntCloud account, session, UI and input modifications are provided under GPL-3.0-or-later. The combined distribution must satisfy upstream GPL-3.0 requirements.

The Mac SDK comes from the pinned official Moonlight dependency release and is verified against its recorded SHA-256. Preserve SDK and dependency license notices in binary distributions. Build output, SDK binaries, credentials and private captures are excluded from the source package.
