# 1.0.2 cleanup and verification

Scope: native controller/provider lifecycle, host discovery, native interface, resolution choices, release metadata, build scripts and user/developer documentation. Preserve the streaming engine, physical modifier mapping, decoder/audio fallbacks and local shutdown confirmation.

Plan before implementation:

1. Lock existing input/provider/control/UI behavior; add regressions for cached-host reconnect and expected shutdown termination.
2. Repair cached-host discovery: a successful fresh poll must notify discovery even when the host's values did not change. Retry a failed first host lookup only within the existing startup window.
3. Treat provider closure after an accepted shutdown request as expected termination, preserve genuine errors, reset the current-session messages at a new connection, and label host contact separately from pairing.
4. Remove the unsupported 1440×900 choice, migrate its saved selection to 1080p, and keep higher saved resolutions at their original dimensions.
5. Reduce unnecessary idle/UI work, retain bounded diagnostics and isolate shutdown information visually. Remove duplicate Mac plist packaging and set one native release version.
6. Run relevant fixtures/lint/build and inspect light/dark interface states; write plain-language README/setup and a development contract in AGENTS.md.

Fallback inventory: HTTP 400 readiness remains pending (provider boundary), host certificate pinning remains strict, native decoder/audio platform fallbacks remain intact. Retrying read-only host discovery is bounded and never substitutes a cached online value for a fresh response. Pairing/shutdown mutations remain one-shot; no protocol bypass or silent success is introduced.

Verification results will be recorded after the checks run. The attached Mac build log validates the previous release's compilation/signature/core fixtures. The user reports working native streaming; the new release still requires a Mac/live reconnect check.

Stop/shutdown follow-up plan: reproduce a provider closure arriving between local Stop selection and queued cancellation; record intent before the SDL exit and preserve genuine next-session errors. Keep shutdown write rejection visible. Separate transient stream advisories from general notices, clear them on teardown, and label only 1440p/4K experimental. Validate provider/control/QML regressions and rebuild the unchanged streaming engine.

Results: the prior-source cached-host and expected-shutdown regressions failed as predicted, then passed with the fixes. Core protocol/input/control assertions, 9 provider behaviors, 3 real host discovery behaviors and 13 UI behavior/data cases passed; full Linux release build and version smoke check passed. QML lint and source/shell/Python checks passed. Narrow light and wide dark shutdown screens and the red error screen were visually inspected. The additional review fixed seeker-before-manager teardown and canceled pending readiness work after shutdown. No benchmark claim or removal of engine compatibility fallbacks is made. Live Mac shutdown/reconnect remains the release smoke check.


Follow-up scope: the new Mac evidence exposed an app-relative OpenSSL load name in a plain test executable; the runner now supplies a scoped SDK search path and the current OS deployment target to fixtures. Bitrate auditing also reproduced and corrected a partial-bucket averaging undercount, with deterministic rate/idle/expiry regressions. Requests and recovery budgets are unchanged. Details and limitations are recorded in validation.md and stream-statistics.md. Version remains 1.0.2; this replaces the earlier source package.

Stop/shutdown follow-up results: the queued closure race reproduced before the fix. All 12 provider behaviors, local control routing, the core suite and 18 QML results pass; full Linux release rebuild, version, lint and syntax checks pass. Stream advisory teardown and standard/experimental resolution labels are covered. Live Mac verification remains required.
