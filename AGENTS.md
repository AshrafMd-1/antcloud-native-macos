# AntCloud Native development contract

This is a personal macOS client based on pinned Moonlight 6.2.0. Preserve GPL-3.0 and upstream/submodule notices. Read README.md, setup.md, source-provenance.json and docs/validation.md before changing the native workflow.

## Scope and behavior

- Normal use is one native app: C++ provider worker, Qt account/settings UI, existing SDL streaming engine. Do not reintroduce a Python/Terminal runtime controller or file-based IPC.
- The strict physical mapping is Command→Alt, Option→Windows, Control→Ctrl, Shift→Shift. macOS owns Fn/Globe. Local stream controls use Control+Command+Shift; keep input release/focus cleanup safe.
- Native host launch precedes successful provider readiness. HTTP 400 is pending, never readiness success. Host address revalidation precedes video control. Preserve generation guards, size bounds, timeouts and certificate pinning.
- A cached online host is insufficient discovery evidence. Notify discovery on a fresh successful poll even when metadata is unchanged. Retry failed read-only adds only within the startup deadline, one in flight. Do not auto-retry pairing, shutdown or force-quit another host application.
- Disconnect sends no explicit provider shutdown. Shutdown sends closestream once after deliberate local confirmation; expected closure after the accepted request is not a connection failure. Report requested, never claim a confirmed off state without evidence.
- Start clears prior session errors, notices and power status. Successful video clears errors. Pairing completion/cancellation, provider stop and Session deletion must finish before another Connect is enabled. Host contact and pairing are separate UI stages.
- Native presets: 720p and 1080p standard; 1440p and 4K experimental. Show the experimental dimensions as 2560 × 1440 and 3840 × 2160. No 1440×900 preset. Saved resolution schema 2 migrates old index 2 to 1080p and keeps higher dimensions; changing this requires migration regressions.
- Mark local Stop intent before queuing worker cancellation or SDL exit. Mark expected Shutdown termination only after the provider write is accepted; failed writes remain errors. Reset termination intent for every new provider session. Stream advisories have their own property and end with the stream; never clear unrelated account/export/shutdown notices as a side effect.

## Privacy, storage and performance

- Token/password stay on the provider worker. Remember only the token in Keychain. Never put credentials, PINs, host addresses or raw API/socket payloads in source, logs or diagnostic history.
- History is at most 64 fixed safe messages, each at most 512 characters, in memory. Disk export is explicit and uses QSaveFile. No automatic routine log files.
- Keep networking off the streaming GUI loop. Stop host polling when launching video; stop connection-progress repaint ticks when connected. Preserve hardware decoder/audio/platform fallbacks; remove a workaround only with specific evidence.
- Build output stays in build/ and dist/, SDK in src/libs/mac/. These and private captures are ignored/excluded. Native release version lives in src/app/antcloud/version.txt; both Mac bundle version fields and the native CLI/About version derive from it. Upstream engine version remains src/app/version.txt.
- Scope cleanup to reachable defects and duplication. Preserve upstream CLI/settings paths unless deliberately removed with their callers. Do not invent broad abstractions or optimize by breaking compatibility.

## Checks and review

Run `bash Test.command` after relevant native lifecycle/input/UI changes. The Mac runner must pass the SDK library path only to test processes and target the build Mac’s OS major; standalone fixtures have no app Frameworks directory. Preserve deterministic bandwidth-window tests: byte totals and elapsed duration must cover the same completed buckets, excluding unfinished samples. Fixtures isolate real account/Keychain/provider access; host tests exercise real manager/seeker/NvHTTP against loopback. Keep regression cases for cached-host reconnect, cold-host retry/cancellation, shutdown then reconnect, one-shot mutations, heartbeat service and safe confirmation focus.

On Linux use `QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software bash Test.command`; screenshot export requires the explicit ANTCLOUD_SCREENSHOT_DIR environment variable. Check compact centering and footer reachability when taller content scrolls; section separator gaps must match. Run Qt qmllint on AntCloudMain.qml, shell syntax checks, Python build-tool syntax and git diff --check. Build the complete engine with appropriate platform dependencies. Mac Build-Mac.command additionally checks SDK digest, core fixtures, deployment/signature and CLI smoke behavior.

Review code/spec/security and architecture independently; record actual results and residual risks in docs/validation.md. Inspect narrow/wide light/dark UI states after layout changes. No tests or Linux build prove live Mac Keychain, VideoToolbox, provider pairing or VM power state: use docs/mac-test.md. Do not label an unverified/notarized local build as production-certified.

Before packaging, regenerate source-manifest.json, include all pinned public dependency source, exclude SDK/build/binaries/private captures and credentials, and verify ZIP hashes/CRC. Keep docs/native-changes.patch (original fork), docs/1.0.2-changes.patch (changes from the 0.2.0 source baseline), and docs/1.0.3-changes.patch (increment from 1.0.2) in that order. Preserve earlier patches; do not regenerate them against newer source. Current public release: 1.0.3. Verify a fresh Git staging of the archive matches its manifest and excludes build/SDK/private data. All requested documentation belongs in the source package.
