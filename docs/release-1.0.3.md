# 1.0.3 scope and cleanup plan

Scope: native resolution labels, account/connection layout, one redundant cancellation queue, release metadata and repository documentation. Keep the provider protocol, native streaming engine, physical modifier mapping, decoder/audio compatibility and confirmed shutdown interaction intact.

Behavior lock: run the current protocol/input/provider/control/discovery/bandwidth/UI fixtures before editing; add only the layout regression needed to verify centering and scrolling after the layout change.

Plan before implementation:

1. Show `2560 × 1440 · Experimental` and `3840 × 2160 · Experimental` while preserving the resolution indices/dimensions and account limits.
2. Replace cosmetic spacer items with explicit vertical padding, center short forms inside the viewport, and keep taller connection states scrollable. Make separator-to-section and heading-to-body spacing consistent.
3. Remove the redundant Stop queued by Disconnect before finish queues the same cancellation. Set intent before the one remaining queue so expected closure handling remains safe.
4. Derive both Mac bundle version fields from the native version file. Keep old patch/release records historical and add an incremental 1.0.3 patch.
5. Update README/setup/AGENTS, publishing instructions and a single copy-ready v1.0.3 release. Verify the complete source package and fresh Git staging, then deliver it for a Mac build.

Fallback inventory: HTTP 400 readiness remains pending, never successful. Saved setting migration/bitrate normalization, strict account parsing, host certificate pinning and platform decoder/audio paths are grounded compatibility boundaries. Retain their tests and evidence; no protocol bypass or new fallback is needed. No unused native runtime path was confirmed; do not delete the retained upstream CLI/platform features merely because they are not in the normal account screen.

Results and the separate code/spec/security and architecture reviews are recorded below after verification.

## Results

- The centering regression failed against the previous layout, then passed with explicit viewport padding and scrolling. Label-width checks also caught clipping after widening the resolution field; the final grid preserves room for all three selectors.
- Qt UI verification passes 20 results, including 11 light/dark snapshots, exact experimental labels, compact centering and footer reachability. Binding-loop warnings fail the fixture; none remain. Linux-rendered narrow/wide views were inspected.
- The complete protocol/input/provider/control/discovery/bandwidth suite passes. The full Linux release engine builds and reports `AntCloudNative 1.0.3`.
- A real incremental-build check reproduced an outdated CLI version after changing only the version file. Embedding that file as a resource fixes the dependency: changing it temporarily to 1.0.4 and building without qmake/main.cpp edits updates the binary; restoring it rebuilds 1.0.3 correctly.
- The Mac builder refreshes subproject makefiles, derives both bundle version fields from the same file and checks the staged CLI version before replacement. A mocked shell check with spaces in the path verifies ordering, both plist values and rejection of an old version without moving the previous generated app. This does not run Apple signing, dyld or deployment.
- Scoped cleanup removes the redundant Disconnect cancellation queue, an unused QML identifier, an unused Python import and the obsolete compiler version define. Retained upstream runtime/platform features and recovery paths have callers; no speculative pruning or decoder rewrite was made. No FPS, latency or memory benchmark improvement is claimed.
- QML lint, shell/Python syntax and whitespace checks pass. Source manifest/ZIP integrity, ordered patches and fresh Git staging are checked as part of final packaging.

## Code/spec/security review

CLEAR for the scoped source change. Preset indices, dimensions, migration and account limits remain unchanged. Layout changes preserve focus/confirmation and lifecycle behavior. Stop intent is recorded before the single cancellation queue. The version resource is part of the executable and is read once at startup; it introduces no external file or network dependency. Build staging preserves the previous generated app on a version mismatch. No credentials, automatic logs or new provider mutations were added.

## Architecture review

WATCH for public distribution. The native engine/provider-thread architecture and its established compatibility paths remain appropriate for the reported working Mac stream. The UI adds no per-frame work; centering is a layout binding. Build version consistency is now an explicit gate.

Strongest remaining limitation: Linux fixtures and a mocked builder cannot certify target-Mac styling, Keychain, decoding, live pairing or shutdown/reconnect. The recovered provider workflow can change outside this source. Follow docs/mac-test.md on the target Mac; public binaries also need Developer ID signing/notarization. Deliver a complete tested source release without claiming production certification.
