# AntCloud Native development guide

## Purpose and documentation

AntCloud Native is a personal macOS client for AntCloud Windows VMs, built on Moonlight 6.2.0. The main project release is **1.0.5**.

Read [README.md](README.md), [setup.md](setup.md) and `source-provenance.json`, then inspect the relevant source before changing behavior. README owns the project and usage reference; setup owns installation/build steps; this guide owns architecture, maintenance, verification and packaging. Keep these three guides current without release histories, personal session narratives, duplicate documents or archived patches.

## Working approach

- Complete clear, reversible local work through verification. Ask only for material ambiguity, destructive actions outside the requested scope or missing authority.
- Use native subagents for bounded independent work when useful. Assign ownership and preserve other agents' edits; the leader integrates and verifies.
- Keep diffs small. Write a cleanup plan before editing; add meaningful regression coverage when affected behavior lacks it. Prefer existing patterns and deletion over new layers. Add dependencies only when requested.
- Explain non-obvious constraints in comments. Avoid obvious narration, release instructions and speculative performance claims.
- Preserve GPL-3.0, copyright, upstream documentation and dependency notices. Do not remove reachable engine/CLI/platform paths without their callers and evidence.
- Keep account credentials, PINs, host addresses, raw provider/socket traffic and private captures out of source, diagnostics and packages.

## Architecture and source map

The app has three cooperating parts: Qt for account/settings state, a provider worker on a dedicated QThread, and the existing Moonlight SDL streaming engine. Provider HTTP/WebSocket objects and timers belong on the provider worker; keep retained host network tasks off the streaming GUI loop. Reusing the engine preserves its decoder, audio and platform compatibility paths.

- `src/app/antcloud/controller.*`: GUI-facing state, saved settings, host discovery, pairing, launch and teardown orchestration.
- `src/app/antcloud/providerworker.*`: HTTP/WebSocket requests, credentials, readiness, heartbeat and session timers.
- `src/app/antcloud/protocol.h`: account/frame/address validation, stream limits, saved-setting normalization and provider minutes.
- `src/app/antcloud/tokenstore.*`: device-local remembered token storage through Mac Keychain.
- `src/app/antcloud/streambridge.h`: process-local stream commands and atomic termination/time snapshots across GUI/worker/SDL boundaries.
- `src/app/gui/AntCloudMain.qml`: account, settings, connection progress, notices and confirmation UI.
- `src/app/backend/`: manager/seeker/NvHTTP host discovery, cached identity, certificate pinning and pairing.
- `src/app/streaming/`: Session, input, audio, video and overlays. Modifier translation is in `input/antcloudinput.h`; throughput accounting is in `bandwidth.*`.
- `src/tests/antcloud/`: isolated native fixtures; `tools/test_fetch_sdk.py` covers SDK verification.
- `Build-Mac.command`, `Open-App.command`, `Test.command`, `tools/fetch_sdk.py`: build, launch, tests and verified SDK preparation.

The supported user flow is a single native app. Keep provider control in process and off the streaming GUI loop; do not add a runtime helper or file-based IPC. Python is used for build tooling.

## Connection and session contracts

- Provider session → VM startup → fresh native host discovery → pairing/Desktop launch → readiness/address revalidation → video control. Native host launch must precede successful provider readiness. HTTP 400 means pending, never success.
- Preserve request/frame size limits, deadlines, generation guards, HTTPS/WSS redirect restrictions and pinned host certificates.
- A cached online host is not fresh discovery evidence. Successful polls notify discovery even when metadata is unchanged. Native failed read-only adds retry within the startup deadline, one in flight; cancellation stops retries. Retained CLI adds remain one-attempt unless retry is enabled.
- Do not retry pairing/shutdown mutations or force-quit another host application automatically.
- Stop/disconnect sends no explicit provider shutdown. Record Stop intent before queued worker cancellation or SDL exit. Accepted Shutdown writes alone mark expected termination; failed writes remain errors. Reset intent on each provider session.
- Shutdown sends `closestream` once after deliberate confirmation. Report the request, not a confirmed off state. Cancel is the initial/default keyboard action; explicit Tab/Shift+Tab traversal and safe focus on reopen must remain.
- Cancel pending readiness after shutdown and prevent stale callbacks from launching video or affecting another session.
- Keep Connect disabled until provider stop, pairing completion/cancellation and Session deletion finish. Destroy the borrowed seeker before its manager.
- A new connection clears prior session errors/notices/power status; successful video clears errors. Host contact and pairing are distinct stages. Stream advisories end with the stream without clearing unrelated notices.
- Stop host polling before video launch and connection-progress repaint ticks after video connects. Preserve decoder/audio/platform fallbacks unless specific evidence justifies a change.

## Input, settings and statistics

### Input

Map physical Command→Windows Ctrl, Option→Alt and Control→Windows key. Recognize local Control+Command+Shift shortcuts before remote translation. Keep other key behavior and modifier flags intact. Maintain release-all/focus cleanup so capture changes, local controls and teardown cannot leave remote keys pressed.

### Settings

Standard presets are 1280×720 and 1920×1080; experimental presets are 2560×1440 and 3840×2160. Display experimental dimensions explicitly and enforce account limits. Changes apply to the next connection.

Preserve resolution schema 2 migration: for stored schema below 2, index 2 maps to 1080p, and indices 3/4 shift down one; current indices remain stable. Changing dimensions/indices requires migration regressions.

Frame rates are 30/60/120. `BitrateChoices` in protocol.h supplies 2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45 and 50 Mbps; client validation accepts 2–50 Mbps. Keep saved values normalized to visible choices and pass selected values to both provider requests and the native stream. Provider delivery at every requested rate is not established by loopback tests.

Codec indices 0/1/2 mean H.264/H.265 (HEVC)/AV1, default 0. Label H.265 and AV1 explicitly as Experimental. Persist codec independently of resolution schema. Use existing StreamingPreferences negotiation/fallbacks; selected higher codecs and experimental resolutions are not guaranteed by provider or decoder support. Native HDR remains disabled.

### Statistics and account time

RTT is control round trip; loss is missing video frames; FPS is incoming video; Mbps is compressed video payload excluding audio/headers/recovery. Do not relabel these as input latency, raw packet loss, game FPS or total traffic.

Bandwidth averages use completed 250 ms buckets covering up to 2.5 seconds. Byte totals and duration must share completed boundaries and exclude unfinished samples. Preserve deterministic phase, expiry and quiet-interval tests. The selected rate reaches provider and StreamConfig; the common engine's compatibility adjustment remains intact.

Account minutes use validated `totalTimeMonth - timeUsedMonth`; zero total is unavailable. Stream updates use `control.timeLeft`. Accept only finite, nonnegative bounded integers; unknown is −1. Reset at stream start/end and sign-out, transfer only the validated integer atomically, and never add decoder-thread networking, a local billable countdown or a power-state inference.

## Privacy and storage

Credentials stay on the provider worker. Save only the remembered token in device-local Keychain; passwords are transient. Ordinary validation network failures retain the saved token. Explicit sign-out requests its deletion and reports storage failures.

QSettings retains native preferences and host/pairing identity. Sign-out does not erase these or diagnostic history. Never share settings files as diagnostic evidence.

Diagnostic history is in memory, at most 64 fixed safe messages and 512 characters per complete prefixed entry. Export is explicit, owner-readable/writable and atomic through QSaveFile. Normal app use creates no routine diagnostic files. Do not enable developer console/file logging in normal packaged builds.

## Build and local verification

Mac source builds require Apple command-line tools, Homebrew, Python 3.10+ and compatible Qt 6.11+ modules: qtbase, qtdeclarative, qtsvg, qttools and qtwebsockets. The builder reuses installed modules, verifies pinned Moonlight SDK v19, runs core fixtures, refreshes makefiles, builds the full engine, deploys frameworks/QML, signs locally and smoke-tests CLI help/version.

Build only the requested architecture/OS target. The script uses the build Mac's architecture and OS major; do not promise universal binaries or older macOS support. Generated app staging must pass before replacing current output. Do not touch the installed Applications copy. Keep SDK inventory/hash/symlink checks and extraction path/size limits.

Native version lives in `src/app/antcloud/version.txt` and is embedded as a resource. CLI/About and both bundle version fields derive from it. Upstream `src/app/version.txt` remains 6.2.0. Build output stays in `build/`/`dist/`, SDK in `src/libs/mac/`.

Run the smallest checks proving the change. Native lifecycle/input/UI changes require the fixture suite:

```bash
bash Test.command
```

`--core-only` skips QML. On Linux use `QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software bash Test.command`, with Qt/qmake6, SDL2/SDL2_ttf and OpenSSL development packages; engine builds also need their platform/media dependencies.

Fixtures isolate real accounts, Keychain and provider access. Preserve coverage for protocol/settings/migration, physical input and bounded formatting, one-shot mutations, heartbeats, Stop/accepted-versus-rejected Shutdown, reconnect/cancellation, real loopback manager/seeker/NvHTTP, provider minutes, bandwidth windows, safe confirmation and actual QML layout/state.

On Mac, SDK library lookup is scoped to test processes and fixtures target the Mac's OS major. Standalone tests have no app Frameworks directory; do not alter SDK install names or shell profiles to accommodate them. Run UI executables from their build folder.

Run `qmllint src/app/gui/AntCloudMain.qml`, `bash -n` on command scripts, Python syntax checks and `git diff --check`. Documentation changes need source-backed fact/link/anchor checks. Build/version changes need a full Mac build and CLI/plist/signature checks. Review correctness/security and architecture independently for substantive changes.

Inspect narrow/wide light/dark states after layout edits; compact forms stay centered and tall content scrolls to the footer, with consistent separators. Create a screenshot directory and set `ANTCLOUD_SCREENSHOT_DIR` explicitly for fixture exports.

## Target-Mac verification

Fixtures do not establish live Keychain, provider pairing/quota, VideoToolbox performance, experimental codec/resolution support or VM power. Live provider actions need authorization for that account/session.

1. Check the generated app's About version and target compatibility. Test sign-in, remembered login, reopen and sign-out.
2. Connect using a standard resolution and H.264. Check honest stage progression, video/audio, relative mouse, edge clicks, focus recovery and remote modifier shortcuts.
3. Test local controls, Resume, Stop and same-process reconnect. Check advisory cleanup and fresh discovery.
4. Save Windows work before deliberate Shutdown. Confirm default Cancel, explicit one-shot request, separate VM power information and dashboard power state; reconnect after cleanup.
5. Verify a genuine failed attempt reports its own issue and the next attempt clears stale state. Compare remaining minutes with provider data and exercise experimental settings only where supported.
6. Inspect narrow/wide light/dark UI and explicit diagnostic export without sharing preferences or Keychain contents.

## Packaging

Keep the complete modified engine, pinned public dependency source, GPL/copyright/SDK notices and all three guides. `source-provenance.json` records exact upstream/submodule revisions, SDK digest, native version and repository. Change history belongs in Git; do not add patch archives or extra documentation folders.

Regenerate sorted `source-manifest.json` as relative path→SHA-256, excluding itself. Check all intended source files are present and match. Exclude SDK/build/binaries, Git/runtime/editor state, captures, credentials, logs and diagnostic exports.

Create source and Mac app ZIPs in `dist/` with versioned names. Verify entry uniqueness, CRC and hashes; extract the source and stage in a fresh temporary Git repository to verify manifest membership and exclusions. Check archived app plist/executable versions against the generated bundle and provide SHA-256 checksums.

Local builds are ad-hoc signed. Developer ID signing, notarization and publication are separate actions; do not claim production certification or live performance without evidence.

## Current verification

The main 1.0.5 Apple Silicon build passed `bash Test.command`: six SDK-verifier tests, protocol/input assertions, provider 17, controls 3, host discovery 5, bandwidth 5 and UI 24, with zero failures. New-rate regressions cover saved settings, 2–50 Mbps boundaries, actual 25/45/50 Mbps provider queries and UI selection. Codec label/fit tests passed; narrow light and wide dark renders were inspected. Independent review found no actionable defects; qmllint, shell/Python syntax, documentation links/anchors and diff whitespace checks passed.

The complete Mac release build passed deployment and signature checks. CLI and both bundle version fields report 1.0.5; the app is arm64 with macOS minimum 26.0. Qt 6.11.2 warns that the installed SDK 27 is newer than its tested SDK 26. Live account/Keychain, pairing, delivered bitrate, video performance, experimental settings and VM power checks remain target-Mac verification tasks; the local app is not notarized.
