# AntCloud Native development guide

## Project contract

AntCloud Native is an unofficial native macOS client for AntCloud Windows VMs. It uses Qt for account and settings UI, a dedicated provider worker for AntCloud networking, and the Moonlight 6.2.0 SDL engine for streaming. The production version is **2.0.0**.

Read [README.md](README.md), [build.md](build.md) and `source-provenance.json` before changing behavior. Then inspect the relevant source and tests.

- `README.md` owns the public project, usage, settings and controls reference.
- `build.md` owns installation, source build and verification commands.
- `AGENTS.md` owns architecture, invariants, maintenance and packaging.

Keep these guides about the current product. Use Git history for old releases and discarded behavior. Preserve upstream licenses, notices and vendored documentation.

## Working rules

- Complete clear local work through verification. Keep unrelated user changes intact.
- Use native subagents for bounded independent work when useful. Assign file ownership, preserve other agents' edits, and own integration and final verification.
- Write a bounded plan before cleanup or refactoring. Add meaningful regression coverage before changing behavior that lacks it.
- Prefer deletion and existing patterns over new layers or dependencies.
- Keep provider I/O off the streaming and GUI threads. Preserve decoder, audio and platform fallbacks unless evidence justifies a change.
- Never put credentials, pairing PINs, VM addresses, raw provider traffic, microphone samples, local preferences or Keychain data in source, fixtures, diagnostics or packages.
- Make UI and documentation claims from source and fresh test evidence. Do not present a fixture as proof of live provider, hardware or VM behavior.

## Source map

The application has three cooperating parts: Qt owns user-facing state, `ProviderWorker` owns AntCloud network state on its `QThread`, and the existing Moonlight engine owns the native stream.

- `src/app/antcloud/controller.*` — UI-facing state, immediate preference persistence, connection orchestration and teardown.
- `src/app/antcloud/providerworker.*` — AntCloud HTTP/WebSocket requests, authentication, startup, heartbeat, quota refresh and shutdown requests.
- `src/app/antcloud/protocol.h` — validated provider values, resolution tiers, bitrate choices and settings normalization.
- `src/app/antcloud/modifiermapping.h` — modifier defaults, collision handling, saved-data normalization and packed stream snapshots.
- `src/app/antcloud/remainingtime.h` — bounded provider-minute formatting shared by the app and stream overlay.
- `src/app/antcloud/tokenstore.*` — device-local remembered access and refresh tokens in macOS Keychain.
- `src/app/antcloud/streambridge.h` — process-local, generation-bound commands and atomic stream state across Qt and SDL.
- `src/app/antcloud/microphone*` — Mac permission, capture, conversion, queueing and AntCloud microphone delivery.
- `src/app/gui/AntCloudMain.qml` — sign-in, settings, connection progress, notices, confirmations and help.
- `src/app/backend/` — native host discovery, pairing, cached identity and pinned host certificate handling.
- `src/app/streaming/` — Moonlight Session, input, playback, rendering and overlays.
- `src/app/streaming/input/antcloudinput.h` — Mac-to-Windows modifier translation.
- `src/app/streaming/input/antcloudcontrols.h` — the SDL stream controls window and shortcut reference.
- `src/app/streaming/video/antcloudstats.h` — native stream statistics text.
- `src/tests/antcloud/` — isolated protocol, lifecycle, input, statistics, microphone and QML fixtures.
- `Build-Mac.command`, `Open-App.command`, `Test.command` — supported Mac build, launch and test entry points.
- `tools/fetch_sdk.py`, `tools/test_fetch_sdk.py` — pinned SDK preparation and integrity checks.

Keep the supported user flow inside one application process. Do not add a runtime helper or file-based IPC for provider control.

## Provider and stream lifecycle

The normal sequence is:

1. authenticate or restore a remembered login;
2. open the provider session and start or join the VM;
3. discover a fresh native host;
4. pair and launch Desktop;
5. revalidate provider readiness and address;
6. send video control and run the native stream;
7. stop provider and native work before enabling another Connect.

Preserve these invariants:

- HTTP 400 during startup means pending, never ready.
- Provider HTTP/WebSocket objects and timers stay on the provider worker thread.
- Request and frame size limits, timeouts, generation guards, redirect restrictions and pinned host certificates remain enforced.
- A cached online host is not fresh discovery evidence. Successful native polls notify discovery even when metadata is unchanged.
- Retired host monitors cannot overwrite a new attempt or deliver stale discovery notifications. Release manager locks before joining monitor threads.
- Pairing dispatch is reply-bound, one-shot and stamped with the provider generation. Do not retry pairing or shutdown mutations.
- The host challenge delay is transport ordering, not provider readiness.
- Session callbacks carry the active `Session` identity. A retiring session cannot change a newer attempt.
- Cancel startup work after Stop or Shutdown so stale callbacks cannot launch video.
- Destroy the borrowed seeker before its manager. Do not enable Connect until worker stop, pairing cleanup and Session deletion finish.
- Native host launch/resume failures expose fixed safe numeric status. Raw host messages do not enter provider diagnostics.

**Stop stream** records stop intent before queued cancellation or SDL exit and sends no explicit VM shutdown. **Shutdown VM** sends `closestream` once after deliberate confirmation. Report the request without claiming the VM is off. Cancel is the default confirmation action and keyboard traversal must remain explicit and safe.

Clear stale errors and notices when a new attempt begins. Successful video clears connection errors. Stream-only advisories end with that stream.

## Authentication, storage and diagnostics

Passwords are transient. Remember access and refresh tokens together in a device-local Keychain item. Keep backward compatibility with a legacy raw access-token value.

Renew through `GET /api/users/refresh` with `refresh: JWT <refreshToken>`. Validate both rotated tokens before adopting them. Coalesce concurrent renewal and retry only authenticated reads once; never replay pairing, shutdown or another mutation. Authentication and session generations must reject stale replies.

An expired JWT claim can trigger authenticated preflight. It never establishes login. Temporary validation or renewal failures keep saved credentials. Explicit sign-out requests Keychain deletion and reports a storage failure.

QSettings stores stream preferences and native pairing identity. Signing out does not clear those values.

Diagnostics remain in memory until an explicit export. Keep at most 64 complete, fixed safe messages, each no longer than 512 characters including its prefix. Export atomically with owner read/write permissions. Never log or export secrets, VM addresses, raw packets or microphone audio.

## Settings and resolution contracts

The settings panel opens on **Video**. **Keyboard** contains the modifier editor. Tabs remain inspectable while the app is busy, but settings edits are disabled. The microphone remains editable during a connected stream.

Every accepted setting is written immediately. Saved values override fresh defaults on the next app launch. In-stream shortcuts persist the resulting fullscreen, relative-mouse, statistics, controls-hint and microphone state for later sessions. Generation guards must reject queued updates from an old stream.

Fresh settings are:

- 1920 × 1080, 16:9
- 60 FPS
- 10 Mbps
- H.264
- fullscreen on
- relative mouse on
- stream statistics on
- stream controls hint on
- microphone off
- Command→Ctrl, Option→Windows key, Control→Alt

The resolution indices are stable:

| Index | UI label | Provider tier |
| ---: | --- | ---: |
| 0 | 1280 × 720, 16:9 | 720 |
| 1 | 1920 × 1080, 16:9 | 1080 |
| 2 | 2560 × 1440, 16:9 | 1440 |
| 3 | 3840 × 2160, 16:9 | 2160 |
| 4 | 2560 × 1600, 16:10 | 2160 |
| 5 | 2048 × 1280, 16:10 | 1440 |
| 6 | 1920 × 1200, 16:10 | 1440 |
| 7 | 1680 × 1050, 16:10 | 1080 |
| 8 | 1440 × 900, 16:10 | 1080 |
| 9 | 1280 × 800, 16:10 | 1080 |
| 10 | 1024 × 640, 16:10 | 720 |
| 11 | 960 × 600, 16:10 | 720 |

The provider tier bounds both dimensions; the native host still receives the exact width and height. Enforce the account limit before provider startup. Account allowance does not establish VM display or encoder support.

The current resolution schema is 3. For a stored schema below 2, old index 2 maps to current index 1 and old indices 3/4 shift down one. Schema 2 defined only indices 0–3, so a stored value above 3 from any schema below 3 normalizes to the 1080p default instead of acquiring a newly added meaning. Schema 3 retains indices 0–3 and appends the new modes at 4–11. Any index change requires migration regressions.

Frame-rate indices 0/1/2 mean 30/60/120 FPS. `BitrateChoices` is 2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45 and 50 Mbps. Normalize saved values to a visible choice and pass the value to both provider and native stream configuration.

Codec indices 0/1/2 mean H.264/H.265/AV1. UI labels are **H.264**, **H.265 - if supported** and **AV1 - if supported**. Use existing `StreamingPreferences` negotiation and fallback behavior. Native HDR remains disabled.

The engine's 4K-to-1080 fallback applies only to legacy NVIDIA hosts. Preserve exact non-NVIDIA requests and the fixed safe advisory for an actual fallback.

## Keyboard and local controls

Modifier rows are Command, Option and Control. Targets are None, Ctrl, Alt and Windows key at indices 0/1/2/3. Assigning an occupied non-None target clears the other row; duplicate None values are allowed. Invalid stored targets become None and malformed duplicates keep the earlier row.

Snapshot all three targets once per SDL input handler so down/up pairs use the same mapping. Translate left and right scancodes and modifier flags from the original event. None sends no remote modifier event or flag. Preserve release-all and focus cleanup so changing capture or opening controls cannot leave remote keys held.

Recognize physical Control+Command+Shift shortcuts before remote modifier translation, even when all mappings are None. The shortcuts are P/Q/Z/X/S/H/M/B/V/C/D/L/K/E. X, S, H, M and B persist their resulting preference. Retained non-native CLI sessions keep the upstream mapping behavior.

The in-stream controls window shows Resume, Stop stream and Shutdown VM above a divider and a compact shortcut reference. The top-right controls hint uses the statistics font size and remains independent of the statistics toggle.

The regular window's Controls button has a full-height dark fill, white text and visible keyboard focus. Its help uses the running version and live mapping. Bound the viewport to the window, keep Close outside the scroll area and reset scroll position when reopened.

## Audio and microphone

GameStream audio carries Windows playback to the Mac. Microphone input uses a separate AntCloud channel: raw 44,100 Hz mono signed PCM16 little-endian UDP to the validated VM address on port 9002, with no application header, acknowledgment or encryption.

The saved microphone preference defaults off. Enabling it before a connection must not create a capture worker, timer or socket. Start permission and capture only in a connected stream. Teardown always closes the send gate, stops capture, joins the worker, clears queued audio and invalidates late callbacks while preserving the user's preference.

Permission denial, capture loss or conversion failure disables the current capture attempt without terminating video. Preserve the user's saved opt-in and the visible failure status; a later connected session may try again unless the user switches the preference off. Keep preference, permission, capture, send and failure state distinct from host delivery.

Use AVAudioEngine independently of Moonlight playback. The audio callback must not allocate, block, send packets or touch Qt GUI state. Use the bounded preallocated queue and off-thread conversion/network work. Preserve converter state, downmix and resample actual input, send 10 ms packets no larger than 882 bytes, and discard stale queued audio.

Show the short trusted-network notice only while microphone sending is enabled. Do not claim that local UDP sends prove VM delivery. Hardened-runtime or sandbox signing requires the corresponding microphone and network entitlements.

## Statistics and provider time

The native overlay has one line with RTT, loss, FPS, Mbps and provider time. It does not show stream dimensions. Time uses bounded hours/minutes formatting such as `120h 10m`; unknown is `unavailable`.

- RTT is control round-trip time.
- Loss is missing video frames.
- FPS is incoming video frames.
- Mbps is compressed video payload excluding audio, headers and recovery.

Do not relabel these as input latency, raw packet loss, game render rate or total traffic.

Bandwidth averages use completed 250 ms buckets covering up to 2.5 seconds. Byte totals and elapsed time must use the same completed boundaries and exclude unfinished samples.

Account minutes are `totalTimeMonth - timeUsedMonth`; a zero total is unavailable. Active stream updates come from `control.timeLeft`. Accept only finite, nonnegative integers within `int`; unknown is -1. The provider worker refreshes account time after authentication, at stream start/end and every 60 seconds. Allow one read in flight and coalesce another request into one deferred refresh.

Keep the last validated account balance through Stop and temporary refresh failure. Clear account data on a new authentication boundary or sign-out. Clear the stream's atomic value at stream boundaries so an old session cannot leak quota into a new one. Never synthesize a countdown or infer VM power from time data.

## Build and tests

The supported Mac build path is documented in [build.md](build.md). Native version is stored in `src/app/antcloud/version.txt` and embedded as a resource. CLI/About and both bundle version fields derive from it. Keep upstream `src/app/version.txt` at Moonlight 6.2.0.

Run the smallest fixture that proves a change, followed by the integrated suite for native lifecycle, input, UI or packaging work:

```bash
bash Test.command
```

`bash Test.command --core-only` skips QML. On Linux, use offscreen Qt and software RHI with Qt/qmake6, SDL2/SDL2_ttf and OpenSSL development packages; a full engine build also needs its platform and media dependencies.

For UI fixture screenshots, create an explicit output directory and set `ANTCLOUD_SCREENSHOT_DIR`. Inspect narrow and wide layouts in light and dark themes. Forms remain centered, tall content scrolls to the footer, tabs remain inspectable while busy, and dialogs keep their actions reachable.

Also run:

```bash
/opt/homebrew/bin/qmllint src/app/gui/AntCloudMain.qml
bash -n Build-Mac.command Open-App.command Test.command
python3 -m py_compile tools/fetch_sdk.py tools/test_fetch_sdk.py
git diff --check
```

Documentation changes require source-backed fact, link and anchor checks. Version changes require a full Mac build plus executable, plist and signature checks.

## Target-Mac checks

Fixtures isolate real accounts, Keychain and microphone devices. Before publishing, check the generated app on the target Mac and an authorized account:

1. Confirm About, CLI and bundle versions and the minimum macOS target.
2. Sign in, restore remembered login, reopen and sign out.
3. Connect with 1920 × 1080, 60 FPS, 10 Mbps and H.264. Check video, playback audio, input, focus recovery and Stop/reconnect.
4. Change every modifier target, including None and target stealing, then verify physical local shortcuts still work.
5. Toggle X/S/H/M/B during a stream, reconnect and reopen to confirm the resulting states persist.
6. Save Windows work, confirm Shutdown defaults to Cancel, and verify a single request plus dashboard state.
7. Compare app and overlay time with provider data after login, during streaming and after Stop.
8. Exercise each desired resolution and optional codec only on a VM that exposes compatible display and encoder support.
9. Test microphone permission allowed/denied/revoked, quiet input and speech, B mute/re-enable, reconnect and teardown. Confirm input through the VM's recording endpoint; local sending is insufficient evidence.
10. Inspect the regular window, controls help, in-stream controls and hint at narrow/wide sizes and in light/dark appearance.

## Packaging

Keep the complete modified Moonlight engine, pinned public dependency source, GPL/copyright/SDK notices and the three project guides. `source-provenance.json` records upstream revisions, dependency digest, native version and repository.

Regenerate `source-manifest.json` as a sorted relative-path-to-SHA-256 map, excluding itself. Exclude build output, SDK binaries, Git/editor/runtime state, captures, credentials, logs, preferences and diagnostic exports.

Create versioned source and Mac app ZIPs in `dist/`. Verify:

- archive entry uniqueness and CRC;
- SHA-256 checksums;
- source manifest membership and hashes;
- fresh Git staging after source extraction;
- executable and both plist versions;
- minimum macOS target;
- QML modules and microphone usage description;
- deep strict signature verification on the extracted app; and
- only the 2.0.0 app and release artifacts remain in `dist/`.

Local packages are ad-hoc signed. Developer ID signing, notarization and publication are separate operations and require separate evidence.

## Verified release behavior

The 2.0.0 Mac fixture suite passed six SDK checks, standalone protocol/input/statistics assertions, and 137 Qt test rows: provider 47, controls 7, seeker 18, bandwidth 5, microphone 14, production Controller settings 5 and UI 41. The real Controller fixture verifies immediate saves, reloads, shortcut delivery after Stop, stale generations and microphone preference notification after a local failure. The queued host-monitor regression also passed 50 isolated repetitions after deterministic fixture setup.

QML lint, shell/Python syntax, documentation targets and whitespace checks passed. Actual fixture screenshots cover Video/Keyboard defaults, supported-codec labels, compact/wide light/dark states, enabled-only microphone notices, scrollable help and the SDL controls panel/hint. Independent correctness/security and architecture reviews found no unresolved blockers.

The full Apple Silicon build passed deployment, CLI help/version, both bundle version fields, the microphone usage description and strict deep ad-hoc signature verification. It targets macOS 26.0 with Qt 6.11.2. The installed macOS 27 SDK produces Qt's compatibility warning; these build checks do not prove performance or compatibility on other hardware or targets.

These fixtures isolate accounts, Keychain and microphone devices. Live provider renewal, VM power, display/codec availability, actual Mac permission/devices and hardware performance require target-Mac verification; a passing fixture does not establish them.
