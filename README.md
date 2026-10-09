# AntCloud Native 1.0.3

One Mac app to sign in, start your Windows VM, stream games and disconnect or request shutdown. Terminal is only needed to build it. This is a personal client using Moonlight 6.2.0, not an official AntCloud app.

## Install or update

See [setup.md](setup.md) for the complete install, update and cleanup steps. Build on your Mac:

```bash
cd ~/Downloads/AntCloudNative
bash Build-Mac.command
bash Open-App.command
```

Drag `dist/AntCloudNative.app` into Applications for daily use. Close the old app, old Terminal session helper and any active AntCloud Chrome stream before connecting. Start with **720p / 60 FPS / 6 Mbps**. Sign in inside the app; remembered login uses Keychain. Your password is never saved. Pairing happens automatically, without another app or a Terminal PIN prompt.

The download is full source with a Mac build script. It contains no prebuilt Mac app; the build bundles its runtime libraries so normal use needs no Homebrew, Python or Terminal.

## What it does

- Native video, audio, fullscreen and relative mouse capture for continuous game camera movement.
- Windows keyboard shortcuts with the strict modifier mapping below.
- Resolution, frame rate, bitrate/quality and compact stream statistics.
- Connection stages: provider session → VM boot → native host → Mac pairing → video/audio.
- Local controls: Resume, Stop stream and confirmed Shutdown VM.
- Optional saved login and sign-out; bounded diagnostic history in memory, exported only on request.

**1.0.3:** the experimental resolution choices now show their exact pixel dimensions. Short forms are centered vertically; connection views scroll when needed. Section spacing is consistent, Disconnect queues cancellation once, and the app/bundle versions come from one version file. The complete source includes Git ignore rules, build/setup documentation and a manual GitHub release guide.

The previous reconnect, Stop/Shutdown message and statistics fixes are included. The unsupported 1440×900 preset remains removed; its saved selection becomes 1080p, while saved 1440p/4K retain their dimensions.

## Disconnect or shutdown?

| Action | What happens |
| --- | --- |
| Disconnect / Stop stream | Ends this Mac's connection and returns to the app. Sends no explicit VM shutdown command. |
| Shutdown VM | Asks AntCloud to shut down the VM, once, after your confirmation. Save your Windows work first. |
| Sign out | Removes the remembered account token; available after disconnecting. |
| Quit app | Closes the Mac app. Does not send an explicit shutdown request. |

AntCloud may stop an idle VM after disconnection. Switching to Chrome may work after the native session ends, but this app cannot guarantee the provider's handoff or idle policy. “Shutdown requested” means the request was sent; check the dashboard for the final power state. You can reconnect from the same app when Connect becomes available; a powered-off VM may need to boot again.

## Keyboard cheat sheet

| Mac key while streaming | Windows key |
| --- | --- |
| Command | Alt |
| Option | Windows |
| Control | Ctrl |
| Shift | Shift |

Use **Control+A/C/V/X/Z** for Windows select-all/copy/paste/cut/undo. **Command+Tab** becomes Windows Alt+Tab while system-key capture is active. macOS handles Fn/Globe; a separate Windows Fn modifier is not guaranteed.

For local controls, hold **Control + Command + Shift**, then press:

| Key | Action |
| --- | --- |
| P | Release capture and open local controls; Resume returns to the game |
| Z | Toggle mouse capture; releasing shows local controls |
| Q | Stop stream and return to the app |
| X | Toggle fullscreen |
| S | Show/hide stream statistics |
| M | Switch relative game mouse / absolute desktop mouse |
| V | Type Mac clipboard text into the remote app, if the host supports text input |
| C | Show/hide cursor in absolute mouse mode |
| D | Minimize the stream window |
| L | Toggle pointer confinement in desktop mouse mode |
| K | Toggle capture of system keyboard shortcuts |
| E | Stop stream and exit the app |

The clipboard shortcut types text; it does not synchronize both clipboards or transfer files. **Command+Q** quits the local app when its regular app window has focus. During streaming, use the local shortcuts above.

## Settings and statistics

Resolution choices remain subject to your account limit:

| Resolution | Status |
| --- | --- |
| 1280×720 (720p) | Standard |
| 1920×1080 (1080p) | Standard |
| 2560×1440 (1440p) | Experimental |
| 3840×2160 (4K) | Experimental |

Choose 30/60/120 FPS and 3/6/12/20/40 Mbps. Higher settings require enough host/network capacity; experimental resolutions may not match the VM display as expected. Fullscreen preserves aspect ratio, so differing Mac/VM ratios can leave bars. A stream setting does not install new Windows display-driver modes.

Statistics show round-trip time (RTT), network-dropped **video frames**, FPS, received video bitrate and requested dimensions. RTT is not total input latency; frame loss is not a packet-loss count.

The selected bitrate is a request/bandwidth budget; the overlay measures received compressed video only. Moonlight reserves bandwidth for recovery and other traffic, and the encoder need not use the whole budget for a static/simple scene. A lower measured value alone does not establish a stream fault. Keep resolution/FPS constant, reconnect after changing bitrate, and compare moving gameplay. See [docs/stream-statistics.md](docs/stream-statistics.md) for details. The included statistics fix corrects a small measurement-window undercount. The Mac fixture runner uses the existing SDK libraries; these changes do not increase the provider encoder budget.

## Verification and source

```bash
bash Test.command
```

Local tests do not contact your paid VM or change your real Keychain. Reconnect and shutdown regressions, input/stats, protocol, controls and interface checks passed on Linux; the full Linux release build passed. **Build and check 1.0.3 on the target Mac before publishing a binary release**: [docs/mac-test.md](docs/mac-test.md). Detailed review and limits: [docs/validation.md](docs/validation.md).

The streaming engine keeps its hardware decoding and platform fallbacks. This is a tested personal source release, not a certified/notarized public distribution. Build tools and SDK stay in the source folder; ordinary use creates no automatic diagnostic files or background service. See setup.md to reclaim build space.

Full modified source and pinned public dependency source are in `src/`. Native lifecycle code is `src/app/antcloud/`; UI is `src/app/gui/AntCloudMain.qml`. [AGENTS.md](AGENTS.md) describes development contracts. `source-provenance.json`, `source-manifest.json` and the three ordered patches in `docs/` record provenance and changes. Preserve GPL-3.0 and upstream notices when publishing on GitHub. Build output, SDK binaries, credentials and private captures are excluded.

## GitHub

Suggested repository name: **`antcloud-native-macos`**. About: “Unofficial native macOS client for AntCloud Windows VMs, built on Moonlight.” The manual repository setup and single v1.0.3 release steps are in [docs/github-publishing.md](docs/github-publishing.md), with copy-ready [release notes](docs/github-release-1.0.3.md). Verify the live Mac Stop/reconnect and Shutdown/reconnect flow before publishing the release.
