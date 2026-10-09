# Reading the stream statistics

## Ping versus how responsive the game feels

RTT is an estimated round-trip network time between the Mac and streaming host: data goes there and a reply comes back. It is similar to ping, but comes from the streaming connection rather than an ICMP ping utility. It is not the ping between the Windows game and a multiplayer game server.

Overall input-to-picture latency adds several stages: input transmission, game frame generation, host capture/encoding, video transmission, Mac decoding and display. Native Moonlight can reduce some processing/buffering overhead compared with a browser, but it cannot remove physical distance and Internet routing delay. A different streaming route could change RTT; there is no measured Chrome/native comparison in the supplied build log. Browser statistics may also measure a different endpoint or average. Compare sequential streams with the same VM/settings; do not open both streams together.

## Our compact overlay

| Field | What it measures |
| --- | --- |
| RTT, ms | Estimated round-trip time on the native control connection; lower and stable is preferable. N/A means no usable estimate. |
| FPS | Incoming video frame rate averaged across roughly two seconds. This differs from the game's rendered FPS shown by Afterburner. |
| Loss, % | Missing video frames inferred from frame-number gaps, divided by total frames. This is not raw packet loss; recovery can repair lost packets. Rounded to one decimal place. |
| Mbps | Recent compressed video payload received, averaged over completed 250 ms buckets covering up to 2.5 seconds. Excludes audio, packet headers and recovery traffic. |
| Dimensions | Decoder-reported dimensions when available, otherwise requested dimensions. These are separate from Mac screen size. |

FPS may be lower on static desktop content when a host uses variable frame rate. Check during actual movement before treating it as a gameplay fault. Loss should remain close to zero; a displayed 0.0% is rounded and does not prove that no individual packet was lost.

## Why requested and received bitrates differ

The native interface sends the chosen Kbps value to both AntCloud's session request and Moonlight's StreamConfig. Moonlight's common protocol engine reserves bandwidth for recovery and, for remote streams, audio/control. Its compatibility encoding budget is 80% of the selected value minus 500 Kbps for remote streams. For example, 20 Mbps gives a 15.5 Mbps compatibility video budget. Sunshine-compatible hosts also receive the original configured bitrate separately, so this example is not a promise of what AntCloud's encoder will actually output.

An encoder may need much less data for an unmoving desktop than a fast-moving detailed game. The selected request does not mean the overlay must remain exactly at that number. Actual video output can also be capped by the host/provider; our source check does not prove the VM honors every requested rate.

The earlier 1.0.2 meter had a smaller additional discrepancy: it divided completed samples by a duration including part of the unfinished sample. This could understate a steady rate by up to roughly 10%. The revised source package corrects that calculation. It does not increase traffic, change the requested stream budget or bypass any host limit.

## A useful comparison

1. Keep resolution, FPS and network connection unchanged. Choose 12 Mbps, connect, then watch 20–30 seconds of moving gameplay; note requested bitrate, received Mbps, RTT, FPS, Loss and visible quality.
2. Disconnect, choose 20 Mbps and repeat the same kind of movement. Setting changes take effect on a new connection, not during an existing stream. If useful, repeat at 40 Mbps.
3. Low received bitrate with a static scene, stable FPS and negligible loss is not sufficient evidence of a fault. If the received rate stays pinned low during complex motion at all settings, particularly with blocky quality, investigate a host/provider cap or request handling. If raising the request increases RTT, loss or stutter, investigate congestion rather than forcing more bitrate.

The attached Mac build log contains no live stream measurements. To diagnose the particular VM, compare the settings and observed overlay values during motion. This audit validates local request forwarding and measurement code, not a live bandwidth test of your account.

Primary references: [Moonlight FAQ](https://github.com/moonlight-stream/moonlight-docs/wiki/Frequently-Asked-Questions), bundled src/moonlight-common-c/moonlight-common-c/src/SdpGenerator.c, src/app/streaming/video/ffmpeg.cpp and src/app/streaming/bandwidth.cpp. The FAQ cautions that statistics from different clients may not measure the same thing.
