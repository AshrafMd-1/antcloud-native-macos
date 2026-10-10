# AntCloud Native development guide

## Purpose and documentation

AntCloud Native is a personal macOS client for AntCloud Windows VMs, built on Moonlight 6.2.0. The main project release is **1.0.8**.

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
- `src/app/antcloud/modifiermapping.h`: shared modifier defaults, unique-target assignment, saved-data normalization and stream snapshot encoding.
- `src/app/antcloud/tokenstore.*`: device-local remembered token storage through Mac Keychain.
- `src/app/antcloud/streambridge.h`: process-local stream commands and atomic termination/time snapshots across GUI/worker/SDL boundaries.
- `src/app/antcloud/microphone*`: explicit Mac permission, bounded capture/conversion and the separate AntCloud UDP microphone sender.
- `src/app/gui/AntCloudMain.qml`: account, settings, connection progress, notices and confirmation UI.
- `src/app/backend/`: manager/seeker/NvHTTP host discovery, cached identity, certificate pinning and pairing.
- `src/app/streaming/`: Session, input, audio, video and overlays. Modifier translation is in `input/antcloudinput.h`; throughput accounting is in `bandwidth.*`.
- `src/tests/antcloud/`: isolated native fixtures; `tools/test_fetch_sdk.py` covers SDK verification.
- `Build-Mac.command`, `Open-App.command`, `Test.command`, `tools/fetch_sdk.py`: build, launch, tests and verified SDK preparation.

The supported user flow is a single native app. Keep provider control in process and off the streaming GUI loop; do not add a runtime helper or file-based IPC. Python is used for build tooling.

## Connection and session contracts

- Provider session → VM startup → fresh native host discovery → pairing/Desktop launch → readiness/address revalidation → video control. Native host launch must precede successful provider readiness. HTTP 400 means pending, never success.
- Preserve request/frame size limits, deadlines, generation guards, HTTPS/WSS redirect restrictions and pinned host certificates.
- Retiring host monitors must not overwrite a new connection’s host/app state or deliver queued discovery notifications. Serialize monitor identity checks and state writes against retirement with the manager lock. Suppress old queued callbacks, and release the manager lock before joining monitors in destruction.
- A cached online host is not fresh discovery evidence. Successful polls notify discovery even when metadata is unchanged. Native failed read-only adds retry within the startup deadline, one in flight; cancellation stops retries. Retained CLI adds remain one-attempt unless retry is enabled.
- Pairing dispatch is reply-bound, one-shot and stamped with the provider generation. After the first getservercert requestSent, wait one second for host challenge setup; cancel if the reply finishes or the session changes. This is a transport-ordering mitigation, not a readiness acknowledgment. Do not retry pairing/shutdown mutations or force-quit another host application automatically.
- Stop/disconnect sends no explicit provider shutdown. Record Stop intent before queued worker cancellation or SDL exit. Accepted Shutdown writes alone mark expected termination; failed writes remain errors. Reset intent on each provider session.
- Shutdown sends `closestream` once after deliberate confirmation. Report the request, not a confirmed off state. Cancel is the initial/default keyboard action; explicit Tab/Shift+Tab traversal and safe focus on reopen must remain.
- Cancel pending readiness after shutdown and prevent stale callbacks from launching video or affecting another session.
- Keep Connect disabled until provider stop, pairing completion/cancellation and Session deletion finish. Destroy the borrowed seeker before its manager.
- Controller callbacks capture a non-null Session identity; retiring Session signals cannot change a newer attempt. Native Desktop launch/resume failures expose fixed numeric host/network status while raw host messages remain out of provider diagnostics.
- A new connection clears prior session errors/notices/power status; successful video clears errors. Host contact and pairing are distinct stages. Stream advisories end with the stream without clearing unrelated notices.
- Stop host polling before video launch and connection-progress repaint ticks after video connects. Preserve decoder/audio/platform fallbacks unless specific evidence justifies a change.

## Input, settings and statistics

### Input

Settings have two tabs: Video is initially selected; Keyboard contains the modifier editor. Switching tabs changes visibility without recreating controls or saving a different value. Keep the tabs inspectable while busy and disable stream/mapping edits; the microphone checkbox remains editable in an actively connected stream. Use scoped Basic controls only where custom styling is required; the remainder of the app uses its native style.

The native settings UI has only three rows: Command, Option and Control. Targets are None/Ctrl/Alt/Windows key (indices 0/1/2/3). Defaults are Command→Ctrl, Option→Windows and Control→Alt. Assigning an occupied non-None target clears the other row to None; duplicate None choices are allowed. None sends no remote modifier event or flag.

Persist the whole mapping immediately in `native/modifierMapping`; absent data uses the defaults. Invalid targets become None and malformed duplicate saved targets retain the earlier row. Disable changes while busy and snapshot the mapping once per SDL input handler so key-down/up pairs use the same mapping. The process bridge transfers all three targets in one atomic value; retained CLI sessions keep their existing mapping.

Recognize local physical Control+Command+Shift shortcuts before remote translation, including when every target is None. Translate both left/right scancodes and flags from the original event, never the partially translated result. Keep other key behavior intact. Maintain release-all/focus cleanup so capture changes, local controls and teardown cannot leave remote keys pressed.

The stream panel shows Resume, Stop stream and Shutdown VM above a divider and fourteen compact physical-key shortcuts. B toggles microphone; M toggles mouse mode; H toggles the current stream’s hint. A separate top-right overlay uses the statistics font size and remains independent of the stats toggle. native/showControlsHint defaults true and is saved on Connect; the bridge initializes each Session from that setting. H is per-stream. Preserve all renderer placements/fallbacks and avoid unchanged/off hint work in the frame loop.

The footer Controls button has a full-height dark fill, white text and visible keyboard focus. Its help uses the running application version and live mapping, lists actual engine shortcuts, and explains clipboard typing and Stop versus Shutdown. Bound the help viewport to the window, keep Close outside the scrollable content, and reset the scroll position on reopen.

### Audio and microphone

The pinned GameStream audio path receives host audio for Mac playback. AntCloud's Android client uses a separate microphone channel: raw 44,100 Hz mono signed PCM16 little-endian UDP to the validated provider VM address on port 9002, without an application header, acknowledgment or encryption. This contract comes from the supplied Android transport analysis; the user has confirmed live VM microphone input. UI/status/help must not retain obsolete Experimental/unverified wording. Local send status still describes local activity rather than an acknowledgment from the VM.

The Video checkbox is off on app launch and after every session; do not persist opt-in. Request Mac permission and start capture only when enabled in an actually connected stream. Physical Control+Command+Shift+B toggles sending with every modifier mapping. Local failures/denial disable the feature without terminating healthy video. The local shortcut carries its session generation so an old queued toggle cannot affect a reconnect.

Use AVAudioEngine capture independently of the Moonlight SDL playback subsystem, a bounded preallocated audio callback queue, and off-thread conversion/network work. The callback must not allocate, block, send packets or access the GUI. Preserve converter state, downmix/resample actual input to the wire format, cap raw datagrams at 882 bytes and discard stale queued audio. The sender uses 10 ms packets. Do not add headers/encryption incompatible with the recovered receiver contract.

Mute/Stop/accepted Shutdown/provider failure/app closing immediately close the atomic send gate before queued teardown. Stop capture, join the worker, clear audio and destination, and invalidate late permission/capture callbacks on every teardown. Never retain audio or log raw samples/addresses/errors. Keep permission/capture/send state distinct from host delivery. While disabled, show no unencrypted-channel notice; show the short trusted-network notice only when enabled. Disabled input constructs no worker thread and runs no capture/socket/timer. Active pumping is 10 ms; health checks are bounded to 250 ms. Copy only populated callback samples and reuse consumer buffers. The app's usage description must survive deployment; future hardened-runtime or sandbox signing needs the corresponding audio/network entitlements. No host installation or firewall changes are part of this client feature.

Selecting microphone before a connection shows no selection-status row. Keep an empty pending status hidden without reserving layout space; retain permission, capture, send and failure feedback and the enabled-only network notice.

### Settings

Standard presets are 1280×720 and 1920×1080; experimental presets are 2560×1440 and 3840×2160. Display experimental dimensions explicitly and enforce account limits. Fresh/invalid settings select 1080p, without a default label; retain explicit saved 720p. Changes apply to the next connection. The engine’s 4K→1080 fallback applies only to legacy NVIDIA hosts; preserve non-NVIDIA requests. Actual fallback sets Session legacy4KFallback and yields a fixed safe GUI advisory. Account allowance does not prove VM encoder/display support; keep SOPS/game optimization behavior unchanged.

Preserve resolution schema 2 migration: for stored schema below 2, index 2 maps to 1080p, and indices 3/4 shift down one; current indices remain stable. Changing dimensions/indices requires migration regressions.

Frame rates are 30/60/120. `BitrateChoices` in protocol.h supplies 2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45 and 50 Mbps; client validation accepts 2–50 Mbps. Keep saved values normalized to visible choices and pass selected values to both provider requests and the native stream. Provider delivery at every requested rate is not established by loopback tests.

Codec indices 0/1/2 mean H.264/H.265 (HEVC)/AV1, default 0. Label H.265 and AV1 explicitly as Experimental. Persist codec independently of resolution schema. Use existing StreamingPreferences negotiation/fallbacks; selected higher codecs and experimental resolutions are not guaranteed by provider or decoder support. Native HDR remains disabled.

### Statistics and account time

RTT is control round trip; loss is missing video frames; FPS is incoming video; Mbps is compressed video payload excluding audio/headers/recovery. Do not relabel these as input latency, raw packet loss, game FPS or total traffic.

Bandwidth averages use completed 250 ms buckets covering up to 2.5 seconds. Byte totals and duration must share completed boundaries and exclude unfinished samples. Preserve deterministic phase, expiry and quiet-interval tests. The selected rate reaches provider and StreamConfig; the common engine's compatibility adjustment remains intact.

Account minutes use validated `totalTimeMonth - timeUsedMonth`; zero total is unavailable. Stream updates use `control.timeLeft`. Accept only finite, nonnegative bounded integers; unknown is −1. Keep the last validated balance visible across Connect/Stop and failed refreshes. Clear account data on new authentication/sign-out; clear only the stream atomic snapshot at stream boundaries so an old session's quota cannot leak into a new one.

The provider worker refreshes `/api/users/me` on stream start/end, every 60 seconds after authentication, and when login omits quota. Allow one refresh in flight. Authentication/session generations and the time revision guard prevent stale responses overwriting a newer stream report or another account. Quota failures stay nonfatal and never remove credentials or terminate streaming. Transfer only validated minutes atomically; keep networking off decoder/GUI threads and never add a guessed billable countdown or power-state inference.

Coalesce refresh requests made during a pending read into one deferred read. A boundary refresh must still run after an intervening stream report changes the time revision. Invalid stream reports must preserve both the GUI balance and the current stream snapshot; only explicit authentication/session boundaries clear their respective data.

## Privacy and storage

Credentials stay on the provider worker. Remember access and refresh tokens together in device-local Keychain; passwords are transient. Keep the existing Keychain item backward compatible with legacy raw access tokens. Renew through GET /api/users/refresh with refresh: JWT <refreshToken>; validate accessToken/refreshToken before adopting rotation. Coalesce renewal; guard auth/session generations and retry only authenticated reads once. Never replay pairing/shutdown or other mutations. Use strict whole-string token/PIN validation, including terminal newline and length boundaries. An expired JWT claim selects an authenticated preflight before opening the provider socket; it never establishes login. Cancel stale preflight callbacks and detach old sockets before reconnect. Ordinary validation or renewal network failures retain saved credentials. Explicit sign-out requests its deletion and reports storage failures.

QSettings retains native preferences and host/pairing identity. Sign-out does not erase these or diagnostic history. Never share settings files as diagnostic evidence.

Diagnostic history is in memory, at most 64 fixed safe messages and 512 characters per complete prefixed entry. Export is explicit, owner-readable/writable and atomic through QSaveFile. Normal app use creates no routine diagnostic files. Do not enable developer console/file logging in normal packaged builds.

## Build and local verification

Mac source builds require Apple command-line tools, Homebrew, Python 3.10+ and compatible Qt 6.11+ modules: qtbase, qtdeclarative, qtsvg, qttools and qtwebsockets. The builder reuses installed modules, verifies pinned Moonlight SDK v19, runs core fixtures, refreshes makefiles, builds the full engine, deploys frameworks/QML, signs locally and smoke-tests CLI help/version. Regenerate the generated app's plist on incremental builds: qmake's bundle rule has no template prerequisite. Compare the staged microphone usage description with the source template before signing or replacing output.

Build only the requested architecture/OS target. The script uses the build Mac's architecture and OS major; do not promise universal binaries or older macOS support. Generated app staging must pass before replacing current output. Do not touch the installed Applications copy. Keep SDK inventory/hash/symlink checks and extraction path/size limits.

Native version lives in `src/app/antcloud/version.txt` and is embedded as a resource. CLI/About and both bundle version fields derive from it. Upstream `src/app/version.txt` remains 6.2.0. Build output stays in `build/`/`dist/`, SDK in `src/libs/mac/`.

Run the smallest checks proving the change. Native lifecycle/input/UI changes require the fixture suite:

```bash
bash Test.command
```

`--core-only` skips QML. On Linux use `QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software bash Test.command`, with Qt/qmake6, SDL2/SDL2_ttf and OpenSSL development packages; engine builds also need their platform/media dependencies.

Fixtures isolate real accounts, Keychain and provider access. Preserve coverage for protocol/settings/migration, modifier defaults/collisions/None/persistence/both sides/release pairs, physical local controls, bounded formatting, one-shot mutations, heartbeats, Stop/accepted-versus-rejected Shutdown, reconnect/cancellation, real loopback manager/seeker/NvHTTP, account/stream quota boundaries and stale/failed background refreshes, bandwidth windows, safe confirmation and actual QML layout/state.

On Mac, SDK library lookup is scoped to test processes and fixtures target the Mac's OS major. Standalone tests have no app Frameworks directory; do not alter SDK install names or shell profiles to accommodate them. Run UI executables from their build folder.

Run `qmllint src/app/gui/AntCloudMain.qml`, `bash -n` on command scripts, Python syntax checks and `git diff --check`. Documentation changes need source-backed fact/link/anchor checks. Build/version changes need a full Mac build and CLI/plist/signature checks. Review correctness/security and architecture independently for substantive changes.

Inspect narrow/wide light/dark states after layout edits; compact forms stay centered and tall content scrolls to the footer, with consistent separators. Create a screenshot directory and set `ANTCLOUD_SCREENSHOT_DIR` explicitly for fixture exports.

## Target-Mac verification

Fixtures do not establish live Keychain, provider pairing/quota, VideoToolbox performance, experimental codec/resolution support, real microphone permission/device behavior or VM power. Live provider actions need authorization for that account/session.

1. Check the generated app's About version and target compatibility. Test sign-in, remembered login, reopen and sign-out.
2. Connect using a standard resolution and H.264. Check honest stage progression, video/audio, relative mouse, edge clicks, focus recovery, saved modifier choices and remote shortcuts. Test target stealing, None and physical local controls after changing all mappings.
3. Test local controls, Resume, Stop and same-process reconnect. Check advisory cleanup and fresh discovery.
4. Save Windows work before deliberate Shutdown. Confirm default Cancel, explicit one-shot request, separate VM power information and dashboard power state; reconnect after cleanup.
5. Verify a genuine failed attempt reports its own issue and the next attempt clears stale state. Compare remaining minutes after login, during streaming and after Stop with provider data; allow the refresh interval. Exercise experimental settings only where supported.
6. Inspect narrow/wide light/dark UI and explicit diagnostic export without sharing preferences or Keychain contents.
7. For microphone validation, opt in after checking the unencrypted-channel notice. Test Mac permission allowed/denied/revoked, absent/changed input devices, B mute/re-enable, quiet input/speech and repeated reconnects. Verify the provider recording endpoint, its Windows input meter and an intelligible recording; local UDP sends alone are insufficient. Check that Stop, native failure, Shutdown and quit leave no capture/sends or old audio. Compare packet timing/size with a working Android reference before claiming compatibility.

## Packaging

Keep the complete modified engine, pinned public dependency source, GPL/copyright/SDK notices and all three guides. `source-provenance.json` records exact upstream/submodule revisions, SDK digest, native version and repository. Change history belongs in Git; do not add patch archives or extra documentation folders.

Regenerate sorted `source-manifest.json` as relative path→SHA-256, excluding itself. Check all intended source files are present and match. Exclude SDK/build/binaries, Git/runtime/editor state, captures, credentials, logs and diagnostic exports.

Create source and Mac app ZIPs in `dist/` with versioned names. Verify entry uniqueness, CRC and hashes; extract the source and stage in a fresh temporary Git repository to verify manifest membership and exclusions. Check archived app plist/executable versions against the generated bundle and provide SHA-256 checksums.

Local builds are ad-hoc signed. Developer ID signing, notarization and publication are separate actions; do not claim production certification or live performance without evidence.

## Current verification

Version 1.0.8 passed the full Mac fixture suite: six SDK checks, standalone protocol/input assertions, and 127 Qt test rows (provider 47, controls 6, seeker 18, bandwidth 5, microphone 13 and UI 38), with no failures. Coverage includes retired host polls during immediate reconnect, stale Session callbacks, pairing dispatch, renewal/rotation and strict token/PIN boundaries. QML lint, command-script syntax, Python syntax, guide links/anchors and whitespace checks passed.

Actual fixture screenshots were inspected for compact/wide light/dark Video and Keyboard tabs, microphone notices off/on, scrollable help with its reachable Close button, and the SDL action panel and controls hint. The pending microphone selection row is absent, including when selected; permission/capture/send/error feedback remains covered. Independent correctness/security and architecture reviews found no unresolved blockers, including a fresh review of the microphone status change.

The full Apple Silicon build passed deployment, CLI help/version, both bundle version fields, microphone usage-description and strict deep ad-hoc signature checks. It targets macOS 26.0 and uses Qt 6.11.2. The installed macOS 27 SDK produces Qt's SDK compatibility warning; build success does not establish hardware performance or compatibility beyond this target.

Release verification checks the 571-file source manifest, the 572-entry source ZIP including that manifest, fresh Git staging after extraction, archive uniqueness/CRC/hashes, the extracted app's signature/version/QML modules and SHA-256 checksums. Only the app and versioned 1.0.8 packages remain in `dist/`.

These checks isolate real accounts, Keychain and microphone capture. Live provider pairing, immediate reconnect, renewal, quota, VM power, real microphone permission/devices and VideoToolbox performance were not exercised for this build. User-reported live microphone success is separate evidence. The one-second PIN delay mitigates ordering without proving host readiness; experimental codec/resolution support still depends on the VM. This build is not notarized.
