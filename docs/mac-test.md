# Live Mac check for 1.0.3

Validate this build on the target Mac before replacing the working installation or publishing a binary.

1. Quit the old app/controller and end any active Chrome stream. Build 1.0.3 with `bash Build-Mac.command`; confirm signature verification and the final dist/AntCloudNative.app message.
2. Open that app; close Terminal. Confirm Help → About shows 1.0.3. Remembered login may need Keychain access after the locally signed rebuild.
3. Check there are four resolution options, with no 1440×900. Only 2560 × 1440 and 3840 × 2160 must be marked Experimental. Check both labels at narrow/wide window sizes. A previously saved custom preset becomes 1080p; previous 1440p/4K selections retain their dimensions.
4. Connect at 720p/60 FPS/6 Mbps. Provider/boot/host/pairing/video stages must match the actual work. Host contact must not say pairing before a host responds.
5. Check audio/video, unlimited relative camera movement, top-edge clicks and focus recovery. Test Control+A/C/V in Windows; Command→Alt and Option→Windows remain the strict mapping. The text-typing clipboard shortcut depends on host support.
6. Press Control+Command+Shift+P. Resume recaptures input; Stop stream returns to the app without exiting. A transient stream advisory must disappear, with no false red connection error. Repeat with Control+Command+Shift+Q. Reconnect in the same app.
7. Save Windows work. Open local controls and confirm Shutdown VM. The app should say Disconnected with a separate VM power request, not a false red connection error. Check the dashboard for final power state.
8. When Connect is enabled, connect again **without Cmd+Q**. Repeat shutdown/reconnect twice. A stopped VM may need a new boot, but fresh successful host polling must advance to pairing/video even if cached host metadata is unchanged.
9. Check a real failed attempt shows a red issue. Retry: the old issue and power status clear at the new attempt, and successful video clears errors. A still-failing attempt must show its new issue.
10. Check UI at narrow/wide sizes and light/dark appearance. Short forms should have balanced top/bottom space; tall connection views must scroll to the local-controls hint. Section separator gaps must be consistent. Return on the default Cancel button must not send shutdown; explicitly selecting Shutdown must send only once.
11. Quit/reopen: saved login is validated. Sign out, quit/reopen: sign-in is required. Run `bash Test.command` for local fixtures with the Mac control style.

Help → Export diagnostic report provides the bounded redacted session summary. Do not share the macOS preferences file or Keychain contents; preferences contain the native pairing identity. Provider VM power transitions, Keychain and VideoToolbox decoding cannot be verified by Linux fixtures.
