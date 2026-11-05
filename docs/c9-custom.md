# Personal LG C9 profile

This branch customizes funnymataleao/luma-webos-launcher v2.2.160 for a rooted
LG C9 running webOS 4. It retains the upstream MIT license and attribution.

## Changes

- All 60 weather greetings and their subtitles are profanity-free. The appearance
  settings preview is also cleaned. Standard localized greetings remain intact.
- The default C9 background uses StarfishMediaAPIs with the webOS 4 ACB video
  plane. Hardware decoding and LG's playback clock run behind Luma's transparent
  interface. The source is demuxed without software decoding or RGB copies.
- Source presentation timestamps are preserved, including reordered B-frames.
  One second of compressed video is fed ahead to absorb network/thread jitter;
  the pipeline controls display timing. Actual EOS drains the queued tail before
  the next clip starts. ACB is finalized when playback stops.
- The older buffered DirectMedia v1 adapter remains available with LUMA_NDL_API=1.
  It keeps its prior decode-timestamp timing and bounded packet queue. Its local
  video-tuning.json settings do not override the clocked backend's PTS schedule.
- Manual full-screen wallpaper and the automatic idle screensaver hand off to
  com.airecken.luma.wallpaper, a small ES5 web app using LG's normal clocked
  hardware-video pipeline. Home uses the clocked SMP/ACB background path.
  The user confirmed both paths are smooth. The earlier transparent-surface
  VSync experiment is replaced.
- The full-screen app receives only the selected public catalog clips and shuffle
  setting. It advances through the selection, stays muted, pauses when hidden,
  and returns to Luma on a key, click or wheel input. Playback errors return Home.
  Luma stops its background decoder before handoff and restores its normal Home UI on return.
- Amazon HEVC clips are excluded when C9 preferences are sanitized. The adapter
  cannot play those clips; Apple and community H.264 collections remain available.
- Existing app identity com.denisefremov.luma is retained. The companion player
  remains com.denisefremov.luma.player. This is separate from com.ecklf.luma.
- The renderer's unstable match-guard syntax is expressed using stable Rust,
  preserving its fallback behavior with the documented Rust 1.93 toolchain.

## Build

Follow the upstream SDK and dependency setup in README.md, then run:

```sh
export WEBOS_NDK="$PWD/.build/toolchain/arm-webos-linux-gnueabi_sdk-buildroot"
pnpm run package
```

Install the native launcher IPK and com.airecken.luma.wallpaper_0.0.1_all.ipk
from dist. The separate com.denisefremov.luma.player package is still needed only
for integrated Plex/Jellyfin playback. Packaging and the Homebrew feed include all
three; the deployment script installs the wallpaper helper before the launcher.

The personal branch defaults to LUMA_NDL_API=smp-c9. For the older buffered
C9 backend, set LUMA_NDL_API=1. For an explicit upstream-style DirectMedia v2
build instead, set LUMA_NDL_API=2 before packaging.

## Home button and Gallery

extras/c9/home-watch.js contains the ES5 Home foreground router used on the C9.
It redirects both LG Home app IDs to com.denisefremov.luma, with a one-shot native
Home bypass and fallback when Luma cannot launch. The separate Gallery integration
is conditional on its existing enable flag and installed app. This file is a
reference for that existing root integration; an ordinary IPK installation does
not install or replace system hooks.

The existing persistent startup hook and Gallery installation were preserved.
The previous launcher remains installed for rollback. Do not also enable another
Home takeover mechanism: use one router at a time.

## Streaming and storage

Video backgrounds stream from their original HTTPS sources each time. The adapter
buffers compressed packets in RAM, not in a persistent on-TV video cache. Artwork
and posters are separate. No NAS cache is included in this branch.

## Validation and limits

The C9 adapter was compiled with warnings treated as errors and tested on the TV.
Hardware frame-completion counters increased, two screen captures showed visible
full-screen motion, and a sustained timing sample reported no feed errors or
retries. The user confirmed playback was smoother after buffering was added.
Home routing was verified from the old Luma and HDMI; Gallery routing was checked.
The full-screen handoff was verified on the C9 with all 177 selected clips in its
payload, an advancing native media clock, actual end-of-clip rotation, remote-key
return to the restored Home UI, and a second successful handoff. Tests cover both
manual and automatic idle activation of the same ambient state, URL validation,
muted playlist behavior, hidden-app pause and playback-error return to Home.

The Plex companion player has not been validated for parity with native Plex on
the C9. These wallpaper changes do not expand movie codec or HDR support.
Upstream updates can overwrite local modifications unless this branch is rebuilt.
No personal preferences, credentials, LAN addresses, SSH keys, or TV backups are
included in the repository.

The clocked Home background was visually confirmed smooth on the C9. Remote-key
checks verified two clip changes, full-screen handoff, and returning to clocked
Home playback. Sustained feed diagnostics reported no submission errors or
retries. The pipeline frame callbacks are sampled notifications; their count is
not a measurement of display FPS. Host native tests exercise delayed load
completion, ACB state ordering, B-frame PTS, backpressure, EOS and cleanup.
