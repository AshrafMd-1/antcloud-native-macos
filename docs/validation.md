# Release validation and review

## 1.0.3 current release

Scope: exact experimental resolution labels, compact vertical centering/scrolling, consistent section spacing, redundant Disconnect cancellation cleanup, reliable release version propagation and public repository documentation. The detailed cleanup plan, fallback inventory and separate review lanes are in [release-1.0.3.md](release-1.0.3.md).

| Check actually run | Result |
| --- | --- |
| Protocol/settings/address/frame and physical input/statistics assertions | PASS |
| Provider fixture, 12 behavior cases including deliberate Stop and Shutdown/reconnect (14 results) | PASS |
| Local controls and deliberate confirmation (3 results) | PASS |
| Real manager/seeker/NvHTTP with loopback fake host (5 results) | PASS |
| Completed-window bandwidth regressions (5 results) | PASS |
| Actual QML with isolated controller, centering/scrolling/text fit and 11 snapshots (20 results) | PASS, no binding-loop warnings |
| Full Linux release engine build, Qt 6.4.2, SDL2/FFmpeg; optional libplacebo/Wayland disabled | PASS |
| Native CLI version and resource-only incremental update/restore | PASS, restored to 1.0.3 |
| Mocked Mac build script: path spaces, makefile refresh, plist versions, stale-version rejection preserving previous app | PASS; no Apple deployment/signing executed |
| QML lint, shell/Python syntax, git diff whitespace | PASS |
| Source ZIP CRC/hashes, ordered patch reconstruction and fresh Git staging | Verified during final packaging |

Before the fixes, the centering test failed and a reused release build could retain its old CLI version. Both regressions pass with the current source. Narrow/wide light/dark screenshots were inspected for exact labels, selector clipping and Session/VM power spacing. Qt's Mac native style is not rendered by these Linux checks.

Tests isolate real account/Keychain/provider access. Expected fixture warnings for rejected fake requests and deliberately closed sockets do not establish a production error. No live VM action was performed. Source packaging excludes SDK/build/apps, local Git history, credentials and private captures. Apply the three source patches in their documented order; root scripts/docs are included directly.

Review synthesis: code/spec/security CLEAR within this change; architecture WATCH for the recovered provider contract and public distribution. Target Mac build, native input/video, Keychain and live Stop/Shutdown→Connect must pass [mac-test.md](mac-test.md). A public Mac binary additionally needs Apple signing/notarization. These checks support a personal source release, not production certification or performance claims.

## Final whole-release check — 9 October 2026

This check reviewed the complete 1.0.3 release together after feature work ended. No runtime code or behavior was changed, and the release version remains 1.0.3.

- Verified that all 571 manifest-listed files in the delivered ZIP matched the working source. ZIP CRC, unique entry paths and manifest contents passed.
- Extracted that ZIP into a fresh temporary folder and built the complete Linux release from scratch. Build passed with no compiler warnings; `--version` reports `AntCloudNative 1.0.3` and `--help` succeeds. Qt 6.4.2/SDL2/FFmpeg were used, with optional libplacebo/Wayland disabled.
- Built and ran all seven test executables from the fresh extraction. Protocol/input assertions pass; QtTest reports provider 14, controls 3, seeker 5, bandwidth 5 and UI 20 results, all passed with zero failures/skips. UI covers 11 light/dark snapshots and has no binding-loop warnings. The first snapshot-export attempt lacked its temporary output folder; after creating that folder the complete runner passed. No product fix was required.
- Reviewed account validation and Keychain paths, provider/thread separation, host discovery and pairing, cancellation/generation guards, Stop versus accepted Shutdown, same-process reconnect gating, error/advisory lifetime, physical modifier mapping, local shortcut routing, relative capture, statistics and bounded explicit diagnostics. No new release-blocking defect was identified in this review.
- Reviewed build/test/open scripts and SDK verification. Native CLI/About and both bundle version fields derive from the native version file; the upstream engine version intentionally remains 6.2.0. Re-ran the mocked Mac builder check: path spaces, makefile refresh order, both plist versions and stale-version rejection preserving the previous generated app pass. It does not execute Apple deployment/signing/dyld.
- Reversed all three engine patches, then reapplied them in the documented order. Every reconstructed source hash matches the release. A fresh Git staging contains exactly the expected release files; build/SDK/private test probes remain excluded.
- Current README/setup/AGENTS/release-guide links resolve. QML lint, shell/Python syntax and the working-change whitespace check pass. Archived unified patches keep required context whitespace, and pinned upstream/legal files retain their original line endings; they were not rewritten for an initial-commit whitespace warning. License, notices, pinned dependency source and provenance are present; private captures, real credentials and generated binaries are absent from the source package.

Code/spec/security review: no new blocking findings in the reviewed release scope. Architecture review: WATCH remains for the recovered provider contract and target-Mac/public-binary verification. Existing network/decoder/audio fallbacks remain grounded compatibility behavior; no further pruning or speculative optimization was justified. No performance gain is claimed.

The source release is ready for the user's Mac build and live checklist. Linux fixtures cannot certify Mac Keychain, VideoToolbox, native styling, real pairing or VM power transitions; use docs/mac-test.md. Public binary distribution additionally needs Apple signing/notarization. Only this review record and package hashes were updated after the final checks.

## 1.0.2 historical validation and cleanup review

## Scope and baseline evidence

Cleanup covers the native controller/provider lifecycle, discovery, resolution choices, interface, build metadata and documentation. The proven streaming/input engine and hardware/platform fallbacks remain intact. The supplied Mac build log for the prior 0.2.0 source passed protocol/input/control/provider checks, compiled the ARM64 app and verified its local signature; the user reported working native streaming. It also showed duplicate plist rules and Qt frameworks requiring macOS 26 despite a lower deployment label.

Two regressions were reproduced against the old source before validating the fixes:

- A second discovery of the same cached-online host timed out despite fresh serverinfo responses. PcMonitorThread emitted only metadata changes, which could leave ComputerSeeker waiting forever within its timeout.
- An accepted shutdown followed by provider inactivity emitted a false video-start failure. Expected closure and failure shared the same finish path.

## Changes and cleanup passes

1. Discovery now emits fresh successful poll notification independently of metadata changes. The seeker balances its polling reference and retries failed read-only first adds one at a time, within the remaining five-minute startup budget. CLI behavior retains its default one-attempt add unless retries are explicitly enabled.
2. Shutdown cancels pending readiness replies/polling and treats subsequent expected closure as normal. Pairing/shutdown are one-shot, reset for a new session; the shutdown timer is generation-guarded.
3. New connection clears old error/notice/power status; successful video clears errors. The controller destroys its borrowed seeker before its owned manager during teardown. Busy remains true until provider/pairing/Session cleanup finishes.
4. Removed the custom resolution from native settings, validation and the retained CLI settings preset. Saved-selection migration preserves higher dimensions. Saved bitrates now match the visible choices.
5. Removed redundant current-attempt clears, duplicated Mac plist packaging and a repeated whole-UI elapsed notification. Progress ticks notify only elapsed and stop during connected video. No per-frame path was changed, and no performance benchmark is claimed.
6. UI wording separates contacting a host from pairing, shows Disconnected, uses red for errors and a separate VM power section for shutdown status. Native version is 1.0.2; upstream engine version stays 6.2.0. The personal Mac builder labels deployment for its current macOS major instead of claiming compatibility with older OS versions than bundled Homebrew Qt.
7. README, setup.md and AGENTS.md now cover user commands/shortcuts, cleanup and development contracts. Retained upstream CLI/decoder/platform paths are reachable compatibility code, not dead code.

Fallback inventory: readiness HTTP 400 stays pending (never success); account parsing remains strict; host certificate pinning remains enforced; hardware decoder/audio/platform fallbacks remain. No cached-online substitute, speculative protocol bypass, retry of destructive mutations or automatic force-quit was added.

## Checks actually run for this release

| Check | Result |
| --- | --- |
| Protocol/address/frame/settings assertions, including resolution migration and retired-preset rejection | PASS |
| Native physical modifiers/input/statistics assertions | PASS |
| Provider integration: 12 behavior cases (14 QtTest results with setup/cleanup) | PASS |
| Local controls: input isolation, deliberate confirmation, Resume/Stop/Shutdown routing | PASS |
| Real manager/seeker/NvHTTP: cached-online reconnect, cold first-lookup retry, cancellation/reconnect (5 results with setup/cleanup) | PASS |
| Interface: keyboard sign-in, busy connect blocking, safe shutdown focus, retired preset/error/power state stream advisory lifetime, experimental labels and 10 light/dark width/state snapshots (18 results) | PASS |
| Full Linux release engine build, Qt 6.4.2, SDL2/FFmpeg, default hardware backends; optional libplacebo/Wayland disabled | PASS |
| Built CLI `--version` reports AntCloudNative 1.0.2 | PASS |
| AntCloudMain.qml qmllint | PASS, no warnings |
| Shell syntax, Python build helper syntax, git diff whitespace check | PASS |

The provider fixture includes independent heartbeats during blocked GUI work, native launch before readiness, one-shot pairing/shutdown, disconnect without shutdown, malformed readiness/account limits, inactivity, shutdown then reconnect in the same worker and no late video control after shutdown. Host fixtures use loopback and temporary settings, with unrelated compatibility download/STUN services isolated; they do not touch a live host or the real Keychain. Interface snapshots were visually inspected in Linux; Mac native style is not rendered here.

The source ZIP includes pinned public submodules and excludes SDK/build/binary artifacts, account data and private captures. Its manifest and ZIP CRC/hash checks are verified during packaging. Engine changes are recorded by applying docs/native-changes.patch then docs/1.0.2-changes.patch from a pinned engine checkout's root (equivalent to this package's src/). Root build scripts/docs are included directly in the package.

## Code/spec/security review

No unresolved critical/high finding within this source scope after fixes. Confirmed defects fixed: unchanged cached-host polling failed discovery, expected shutdown reported an error, stale current-attempt messages, misleading host/pairing stage, short cold-host cutoff, late readiness work after shutdown, seeker/manager destruction order and duplicated plist packaging.

Credentials remain on the provider worker, account API requests use HTTPS/WSS with redirects disabled, raw responses/addresses are excluded from diagnostics, tokens are not erased on an ordinary validation network failure, and mutations are sent once. Timers, request/frame sizes and diagnostics remain bounded. No automatic Mac diagnostic files are introduced.

Residual limits: the controller's actual SDL/Mac loop, Keychain, VideoToolbox, live host certificate/pairing and power transitions need the Mac checklist. Fixture UI state tests exercise the actual QML with an isolated model; they do not certify every live Controller transition. Recommendation: COMMENT for a personal source release pending live Mac verification.

## Architecture review

Status: WATCH. AntCloud's recovered mobile/API workflow is not a supported public integration contract. Keeping the existing native decoder/input engine with a dedicated provider thread is justified by the user's working Mac stream. Replacing it with a new video engine would introduce unmeasured risk. The polling change is an event at connection setup, not additional streaming-loop work; cancellation and generation guards contain lifecycle callbacks.

Strongest counterargument: local regressions demonstrate the discovered source bugs are fixed, but cannot guarantee a new VM boot/pairing sequence or actual shutdown acknowledgment from the provider. Repeat Shutdown→Connect on the Mac without restarting the app; public distribution additionally needs stable Apple signing, notarization and a release test matrix.

Synthesis: code/spec/security COMMENT + architecture WATCH. Deliver the complete tested source and personal Mac build path; do not claim production certification or a notarized binary.


## Follow-up Mac log and bitrate audit

The supplied 20261009-035041 Mac log passed all 11 provider and 3 controls results, then stopped at the standalone seeker fixture: dyld could not resolve @loader_path/../Frameworks/libssl.3.dylib. This is a real test-launcher packaging defect; this log does not demonstrate a completed 1.0.2 Mac app build. The verified SDK contains libssl.3 and libcrypto.3 under lib/, both with app Frameworks-relative install names. The revised runner uses a process-local DYLD_LIBRARY_PATH only when executing tests, and applies the current Mac OS major to test qmake targets. A mocked Mac shell check covers all seven fixture invocations, whitespace paths, deployment arguments and scoped environment lookup; it does not execute Mac dyld. Actual Mac verification remains required.

The bitrate audit traced the native picker through preferences, StreamConfig, providerSpec and SDP. The selected value is passed through; the common engine computes an encoding compatibility budget of 80% of the configured bitrate, less another 500 Kbps for remote streams, while Sunshine also receives the original configured budget. Actual host behavior is not proven by the local trace. The overlay counts complete compressed video payload bytes and excludes audio, packet headers and FEC.

An upstream average calculation included the in-progress bucket's elapsed time without its bytes, and visited nine rather than ten completed default buckets. A deterministic constant-rate regression reproduced phase-dependent underreporting approaching 10%. The revised calculation counts ten completed samples with duration ending at their common closed boundary. It keeps packet budgets, protocol and encoder behavior intact. Three bitrate-window cases pass (5 QtTest results): constant known rate independent of partial-bucket phase, unfinished/expired samples excluded, quiet intervals counted. Full fixture suite and full Linux release rebuild pass; shell syntax and whitespace checks pass. This is a meter accuracy correction, not a proven increase in stream quality or lower network ping.

A provider integration fixture also sends a non-default 20,000 Kbps request and inspects the received WebSocket query; it confirms the selected rate is forwarded rather than hardcoded to 6,000.


## Stop/shutdown message follow-up

The supplied screenshots contain older UI wording (black connection text and the previous disconnected description); the previous failed Mac build may have left the installed app unchanged. They do not prove which binary is running. Open the freshly built app and check both About and the Experimental resolution labels.

A new regression reproduced a provider `inactive` frame arriving after local Stop intent but before the queued worker stop executed: the old path emitted a false video-start error. A process-local atomic termination marker now records Stop before worker cancellation/SDL exit. It resets at each provider start. An accepted Shutdown write also marks expected termination; a rejected write remains an error. Stream failure/progress/connected callbacks are ignored during deliberate teardown. This changes no decoder, encoder, input mapping or stream packet budget.

Stream advisories now have their own controller property, clear on finish/session completion, and are visible only while streaming. General account/export/shutdown notices remain separate. 720p/1080p are standard; 1440p/4K are explicitly Experimental without changing their dimensions or provider/account checks.

Checks: the reproduced race fails before the fix and passes afterward. The provider fixture passes all 12 behaviors (14 results), including next-session unexpected inactivity and a failed shutdown write that stays an error. Local controls verify Stop queues cancellation without another shutdown and that queuing Shutdown alone does not assert write acceptance. All core fixtures pass; QML checks pass all 18 results, including advisory lifetime and 10 snapshots. The full Linux release rebuild and 1.0.2 CLI check pass; QML lint, shell syntax and whitespace checks pass. Narrow light/wide dark Experimental selections and narrow shutdown status were visually inspected without clipping.

Review: code/spec/security CLEAR within this scoped change; architecture WATCH remains for the recovered provider contract and live Mac transitions. The marker crosses the GUI/SDL/worker boundary atomically, queued calls retain the worker lifetime mutex, and only a deliberate Stop or accepted Shutdown suppresses teardown errors. Genuine failures are retained, including shutdown write rejection. Live Mac Stop/Shutdown and reconnect still require docs/mac-test.md; no Mac runtime or confirmed VM-off claim is made.


## GitHub handoff

Repository preparation adds a root ignore file that keeps local builds, SDKs, credentials, diagnostics and editor state out while retaining public source dependency Makefiles. A dry-run of `git add .` was reviewed against the ignore patterns. The README includes the recommended `antcloud-native-macos` name and About description; docs/github-publishing.md gives fresh-repository and one-time v1.0.2 release steps, with copy-ready notes. Publishing remains manual; the source package is checked for content hashes, ZIP CRC and ordered patch provenance.
