# AntCloud Native agent guide

Read [README.md](README.md), [setup.md](setup.md) and `source-provenance.json` before changing the native workflow. This file owns developer instructions, technical background, verification and packaging. Keep user-facing project information in README and installation steps in setup; do not add separate publishing guides, release narratives or duplicate checklists.

## Working rules

- Execute clear, reversible local work through verification without permission handoffs. Ask only for genuinely ambiguous, destructive or unauthorized external actions.
- Use native subagents for independent, bounded tasks when they improve quality or throughput. Give each agent a clear scope; the leader integrates results and owns final verification.
- Keep changes small. Before cleanup, write a plan and lock affected behavior with regression tests when coverage is missing. Prefer deletion and existing patterns; add dependencies only when requested.
- Keep comments focused on constraints, invariants and non-obvious behavior. Avoid release/setup instructions, personal session history, speculative claims and narration of obvious code.
- Preserve GPL-3.0, upstream copyright and submodule license notices. Root attribution lives in README; upstream documentation and legal files in `src/` remain intact.
- Record actual checks and residual limitations in the validation record below. Do not describe an old result as a fresh run.

## Architecture and source map

The normal product is one macOS app: a Qt account/settings interface, a C++ provider worker on its own QThread, and the retained Moonlight SDL streaming engine. Do not reintroduce a Python/Terminal runtime controller or file-based IPC. Python is a build-time SDK fetcher only.

- `src/app/antcloud/controller.*`: GUI-facing state, connection orchestration, host discovery/pairing and teardown gating.
- `src/app/antcloud/providerworker.*`: provider account/session requests, WebSocket events, readiness and heartbeat timers. Network objects and credentials stay on the worker.
- `src/app/antcloud/protocol.h`: bounded frame/account/settings validation and saved-setting migration.
- `src/app/antcloud/tokenstore.*`: remembered token storage, including Mac Keychain.
- `src/app/antcloud/streambridge.h`: process-local stream controls and atomic termination intent across GUI/SDL/worker boundaries.
- `src/app/gui/AntCloudMain.qml`: native account, settings, progress, notices and confirmation UI.
- `src/app/backend/computerseeker.*`, `computermanager.*`, `nvhttp.*`, `nvpairingmanager.*`: host discovery, fresh polling, certificate-pinned host requests and pairing.
- `src/app/streaming/`: retained input, video, audio and local controls. Physical input additions are in `input/antcloudinput.h`; measured video throughput is in `bandwidth.*`.
- `src/tests/antcloud/`: isolated protocol, input/statistics, provider, controls, host-discovery, bandwidth and QML fixtures.
- `Build-Mac.command`, `Open-App.command`, `Test.command`, `tools/fetch_sdk.py`: local build, launch, checks and verified SDK extraction.

Upstream is Moonlight 6.2.0 at the commit recorded in `source-provenance.json`. Public submodule sources are included under `src/`. The Mac SDK is Moonlight's pinned dependency release v19; its SHA-256 is recorded in provenance and checked by the fetcher. Existing SDK files are checked against the verification receipt before reuse.

## Behavior contracts

### Input and local controls

- Preserve the strict physical mapping: Command→Ctrl, Option→Alt, Control→Windows, Shift→Shift. This replaces the v1.0.3 mapping (Command→Alt, Option→Windows, Control→Ctrl); all other keys retain their existing behavior. macOS owns Fn/Globe.
- Local stream shortcuts use Control+Command+Shift. Keep input release, focus recovery, relative capture and local-control routing safe.
- Shutdown requires deliberate local confirmation. Default focus/Return selects Cancel; explicit Shutdown sends once.

### Provider, discovery and teardown

- Native host launch precedes successful provider readiness. HTTP 400 is pending, never readiness success. Host address revalidation precedes video control.
- Preserve generation guards, bounded request/frame sizes, timeouts, HTTPS/WSS redirect restrictions and certificate pinning. Do not invent protocol bypasses or silently treat failures as success.
- A cached online host is insufficient discovery evidence. Notify discovery on a fresh successful poll even when metadata is unchanged. Retry failed read-only adds within the startup deadline, with one in flight. The retained upstream CLI keeps one-attempt adds unless retry is explicitly enabled.
- Do not auto-retry pairing or shutdown, or force-quit another host application. Pairing and provider writes remain one-shot.
- Disconnect sends no explicit provider shutdown. Shutdown sends `closestream` once after confirmation. Report requested; do not claim a confirmed off state without evidence.
- Mark local Stop intent before queuing worker cancellation or SDL exit. Mark expected Shutdown termination only after the provider write is accepted; rejected writes remain errors. Reset intent at each provider start. Expected closure must not become a false connection failure.
- Cancel pending readiness work after shutdown. Generation/lifetime guards must prevent late callbacks from launching video or affecting another session.
- Keep Connect disabled until pairing completion/cancellation, provider stop and Session deletion finish. Destroy a borrowed seeker before its owned manager.
- Start clears prior session errors, notices and power status. Successful video clears errors. Host contact and pairing are separate UI stages.
- Remaining time is optional account data (`totalTimeMonth - timeUsedMonth`, zero total means unavailable) or the stream `control.timeLeft` value in minutes. Accept only bounded finite nonnegative integers, never strings/coercions. Reset stream time at session start/stop and sign-out; do not decrement locally or infer shutdown. Transfer only the validated integer atomically to the stats renderer, without GUI access or new streaming-loop networking. Never retain/copy the private HAR in source.
- Stream advisories have their own property and end with the stream. Clearing them must not clear unrelated account/export/shutdown notices.

### Settings and interface

- 720p and 1080p are standard. 1440p and 4K are experimental; show `2560 × 1440` and `3840 × 2160` explicitly. No 1440×900 preset.
- Saved resolution schema 2 migrates old index 2 to 1080p and retains higher dimensions. Changes require migration regressions. Saved bitrates normalize to the visible choices.
- Preserve account resolution limits and provider setting validation. Settings apply on the next connection.
- The native bitrate picker uses `BitrateChoices` from protocol.h: 2, 3, 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40 Mbps. Keep the existing 2–40 Mbps provider bound; do not infer a higher provider budget from the upstream settings screen.
- Codec index 0/1/2 means H.264/H.265 (HEVC)/AV1, default 0. Persist the index separately from resolution schema. Use existing StreamingPreferences codec settings and retain decoder/host negotiation and fallback paths. Experimental codec availability is not guaranteed.
- Shutdown buttons have explicit Tab/Shift+Tab traversal on macOS. Keep default focus/Return on Cancel, reset it on reopen, and require explicit confirmation for the one-shot request.
- Center compact forms vertically, keep tall content scrollable to the footer/local-controls hint, and keep section separator gaps consistent. Inspect narrow/wide light/dark states after layout changes.

## Privacy, performance and compatibility

- Tokens/passwords stay on the provider worker. Remember only the token in Keychain. Ordinary validation network failures must not erase a remembered token.
- Never put credentials, PINs, host addresses or raw API/socket payloads in source, logs or diagnostic history.
- Keep diagnostic history in memory, with at most 64 fixed safe messages. Each entry is bounded to 512 characters including its elapsed prefix. Preserve the whole-entry bound and privacy when modifying diagnostics. Export is explicit and uses QSaveFile. No automatic routine log files.
- Keep networking off the streaming GUI loop. Stop host polling when launching video and connection-progress repaint ticks when connected.
- Preserve hardware decoder, audio and platform fallbacks. Remove a workaround only with specific evidence. Preserve reachable upstream CLI/settings paths unless deliberately removed with their callers.
- Do not claim performance gains without measurements. The provider API is an observed workflow, not a supported public integration contract.

### Statistics implementation

The selected Kbps value reaches both the provider request and Moonlight StreamConfig. The common engine's compatibility video budget is 80% of the configured bitrate, minus 500 Kbps on remote streams; Sunshine-compatible hosts also receive the original configured bitrate separately. This does not prove which budget AntCloud's encoder honors. See `src/moonlight-common-c/moonlight-common-c/src/SdpGenerator.c`.

The overlay measures compressed video payload, excluding audio, headers and recovery traffic. Bandwidth averages use completed 250 ms buckets covering up to 2.5 seconds; numerator and elapsed duration must cover the same completed buckets, excluding unfinished samples. Keep deterministic tests for constant rate independent of partial-bucket phase, quiet intervals, unfinished samples and expiry. The meter correction does not change stream budgets or host limits.

RTT is control-connection round-trip time; FPS is incoming video rate; Loss comes from missing video frames. None establishes total input latency, raw packet loss or the game's rendering rate. Do not infer a native/browser performance comparison from these different measurements.

## Build and verification

Build output belongs in `build/` and `dist/`; SDK binaries in `src/libs/mac/`. These and private captures are ignored/excluded. Native release version is `src/app/antcloud/version.txt` (currently 1.0.4); CLI/About and both Mac bundle version fields derive from it. Upstream engine version remains `src/app/version.txt` (6.2.0). Keep the native version embedded as a resource so version-only incremental builds update correctly.

The Mac builder refreshes subproject makefiles, verifies SDK digest, runs core fixtures, bundles libraries, checks the local signature and smoke-tests CLI help/version before replacing generated output. Preserve the previous generated app on failure. Its deployment target is the build Mac's OS major; do not promise older-OS compatibility than bundled Qt permits.

Run checks appropriate to changed behavior. Documentation-only edits require local link/anchor checks, stale-reference search, manifest verification and `git diff --check`; no live provider action is needed. Native lifecycle/input/UI changes require:

```bash
bash Test.command
```

On Linux:

```bash
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software bash Test.command
```

`--core-only` skips the QML fixture. On Mac the runner passes the SDK library path only to each test process and targets the build Mac's OS major. Standalone fixtures have no app Frameworks directory; do not change SDK install names or the user's shell profile to accommodate them.

Tests isolate real accounts, Keychain and provider access. Host tests exercise the real manager/seeker/NvHTTP against loopback. Preserve regressions for cached-host reconnect, cold-host retry/cancellation, shutdown then reconnect, accepted versus rejected shutdown, one-shot mutations, independent heartbeats, safe confirmation focus, resolution migration, advisory teardown and bandwidth windows.

For layout snapshots, create an output directory and set `ANTCLOUD_SCREENSHOT_DIR` explicitly; fixtures do not export screenshots by default. Run Qt `qmllint` on `src/app/gui/AntCloudMain.qml`, `bash -n` on root command scripts, Python syntax checks on build tools, and `git diff --check`. For engine/build changes, build the complete engine with platform dependencies; optional Linux libplacebo/Wayland may be disabled when unavailable. A CLI smoke check must report the native version and accept `--help`.

Review code/spec/security and architecture independently for substantive changes. Record evidence and gaps below. Fixtures and Linux builds cannot prove live Mac Keychain, VideoToolbox, native styling, pairing or VM power state. Do not label a local ad-hoc signed build as notarized or production-certified.

## Live Mac checklist

Use the target Mac; provider checks act on a real VM and require authorization for that session. Local documentation/test work is not permission to shut down a paid VM.

1. Build with `bash Build-Mac.command`; check signature verification and open the generated app. Close Terminal and confirm Help → About matches the native version file. Test remembered login and any Keychain access prompt.
2. Check all four resolutions at narrow/wide widths. Only 2560 × 1440 and 3840 × 2160 are Experimental; the retired custom selection migrates to 1080p and saved higher dimensions remain intact.
3. End competing browser streams. Connect at 720p/60 FPS/6 Mbps. Provider/boot/host/pairing/video stages must reflect actual events; host contact is not pairing.
4. Check audio/video, continuous relative camera movement, top-edge clicks and focus recovery. Test Command+A/C/V, Command→Ctrl, Option→Alt and Control→Windows. Backspace/Delete behavior is unchanged. Clipboard typing depends on host support.
5. Open local controls with Control+Command+Shift+P. Resume recaptures input; Stop returns to the app without exiting or a false red error. Repeat using Control+Command+Shift+Q. Advisories disappear. Reconnect in the same process.
6. Save Windows work before testing Shutdown. Return on default Cancel must not send a request; deliberate Shutdown sends once. Accepted closure shows Disconnected and separate VM power information. Check the dashboard for actual power state.
7. When Connect enables, reconnect without quitting. Repeat shutdown/reconnect twice. A fresh successful poll must advance discovery even when cached metadata is unchanged; a stopped VM may need to boot again.
8. Check a genuine failed attempt shows a red issue. A new attempt clears old issue/power status; successful video clears errors, while a new failure remains visible.
9. Inspect narrow/wide light/dark layouts, balanced compact centering, tall-content scrolling, footer reachability and consistent separator gaps.
10. Quit/reopen and validate remembered login. Sign out, quit/reopen and confirm sign-in is required. Run `bash Test.command` with the Mac control style.

Use only explicit redacted diagnostic exports when sharing results. Preferences contain the pairing identity and must not be shared.

## Source provenance and packaging

Preserve `source-provenance.json`, upstream notices and all pinned public dependency source. It records the exact Moonlight/submodule revisions, SDK digest, native version and project repository. The package contains the full modified engine; applying patches is not a build prerequisite. The user chose to remove the root `docs/` directory and archived patches in v1.0.4. Maintain subsequent change history in Git; do not recreate patch archives or extra Markdown guides.

Before packaging, regenerate `source-manifest.json` as a sorted mapping of relative source paths to SHA-256 hashes, excluding the manifest itself. Check every listed file exists and matches, and that no intended source file is missing. Include README, setup and this guide. Preserve pinned upstream READMEs and legal files rather than counting them as new first-party guides.

Exclude SDK/build/binaries, local Git history, private captures, credentials, diagnostic exports and editor/runtime state. Review source paths before staging. Verify ZIP entry uniqueness, CRC and hashes against the manifest, then extract and stage in a fresh temporary Git repository to check manifest completeness and exclusions. Public binary distribution additionally requires target-Mac testing and appropriate Apple Developer ID signing/notarization. Publishing is a separate user-requested action, not part of routine setup.

## Validation record

### Recorded release baseline — 9 October 2026

The former release records reported a fresh extracted-source Linux build using Qt 6.4.2, SDL2 and FFmpeg, with optional libplacebo/Wayland disabled. CLI version/help passed. Protocol and input assertions passed; QtTest reported provider 14, controls 3, seeker 5, bandwidth 5 and UI 20 results, with zero failures/skips. UI covered 11 light/dark snapshots and no binding loops. QML lint, shell/Python syntax and whitespace checks passed.

Recorded regressions reproduced cached-online discovery stalls, expected-shutdown errors, local Stop closure races, partial-bucket bandwidth undercount, compact centering failure and stale version-only incremental builds before their fixes. Mocked Mac script checks covered whitespace paths, makefile refresh, scoped SDK lookup, plist versions and stale-version rejection preserving the previous generated app. They did not execute Apple deployment, signing or dyld.

The prior package record reported 571 manifest-listed source files, matching ZIP hashes/CRC, fresh staging exclusions and successful ordered patch reconstruction. Those counts/hashes describe that earlier archive, not a guarantee for a modified tree or new package.

Code/spec/security review found no release-blocking issue in that reviewed scope. Architecture remained limited by the observed provider contract and unverified target-Mac behavior. A supplied older Mac log failed at standalone seeker OpenSSL loading; the scoped test-library lookup was subsequently corrected, but that log does not prove a completed current Mac build. Live Keychain, VideoToolbox, pairing, shutdown/reconnect and VM power transitions remain unproven by the recorded Linux/mock checks. No benchmark gain or production certification is claimed.

### v1.0.4 scope and verification — 9 October 2026

Plan: clarify normal resolution choices versus the conservative testing profile, add actual repository/release links, consolidate the three guides, replace archived patches with full-source/Git provenance, and bump the native version. Fix native confirmation traversal while keeping safe cancellation. Add the requested modifier mapping, validated provider minutes, existing codec choices and finer bitrate presets. Preserve the provider lifecycle, input-release safety, decoder fallbacks and 2–40 Mbps envelope. Verify changed behavior with fixtures, syntax/lint, screenshots, a complete target-Mac build, CLI/bundle versions, source hashes and archive checks.

Baseline evidence: the prior Mac fixture run passed core cases but failed `deliberateShutdownRequiresSelection` because Tab stayed on Cancel under the native style. The new modifier regression failed against the old Command→Alt mapping before implementation. SDK review found unlisted files could escape receipt verification; inventory/symlink checks and isolated regressions address it. The diagnostic cap now applies to the complete prefixed entry.

Fresh target-Mac checks (macOS 26.7.1, Qt 6.11.2): six isolated SDK receipt tests and protocol/input assertions pass. Provider 15, controls 3, seeker 5 and bandwidth 5 QtTest results pass. The final native-style QML run passes 24 results, including 14 snapshots, new codec/remaining-time states, safe default Cancel, Tab/Shift+Tab traversal, explicit one-shot confirmation and safe default after reopen. No failures, skips or binding-loop warnings remain. Narrow/wide light/dark views were inspected for label fit, centering, separator gaps and footer reachability.

`bash Build-Mac.command` completes the full engine build, SDK verification, core fixtures, framework deployment and ad-hoc signing verification. CLI/help smoke checks pass and native CLI plus both bundle version fields report 1.0.4. QML lint, shell/Python syntax, local documentation links/anchors and working-diff whitespace pass. The source manifest contains 561 files. The source ZIP passes hash/CRC/unique-path checks and fresh Git staging matches the full manifest plus the manifest file, with generated/private paths excluded. The Mac app ZIP passes CRC/unique-path, bundle-version and executable-hash checks.

Independent correctness/privacy and architecture review found no remaining blocker in this scope. The requested modifiers are translated only after physical local-shortcut recognition; decoded account time crosses threads as one validated atomic integer. No decoder-loop networking or provider mutation was introduced. Existing codec negotiation and platform fallbacks remain intact.

Residual limits: Qt warns that SDK 27 is newer than its tested SDK 26; retained upstream platform code emits deprecation/duplicate-rpath warnings. The build completes despite these warnings. Fixtures do not establish live Keychain, pairing, decoder performance, provider quota updates on this account, higher-codec host support or actual VM power state. No live provider action was performed. The generated app is locally ad-hoc signed, not notarized; v1.0.4 is prepared locally, not published to GitHub.
