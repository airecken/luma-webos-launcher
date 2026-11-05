# Personal LG C9 profile

This branch customizes funnymataleao/luma-webos-launcher v2.2.160 for a rooted
LG C9 running webOS 4. It retains the upstream MIT license and attribution.

## Changes

- All 60 weather greetings and their subtitles are profanity-free. The appearance
  settings preview is also cleaned. Standard localized greetings remain intact.
- A DirectMedia **v1** H.264 hardware-video adapter replaces the newer v2 adapter
  when building the default C9 package. No software video decoding is used.
- A bounded compressed-video queue (60 packets, at most 16 MiB queued) schedules
  submissions from the monotonic decode timeline with a 200 ms startup buffer.
  Pacing diagnostics rotate at 1 MiB. The queue is released when playback stops.
- C9 builds use a 500 ms compressed-stream lead, decode timestamps, and a 1 ms
  retry interval. An existing local video-tuning.json can override these defaults.
- Manual full-screen wallpaper and the automatic idle screensaver hand off to
  com.airecken.luma.wallpaper, a small ES5 web app using LG's normal clocked
  hardware-video pipeline. Home keeps the buffered NDL v1 background path.
  The user confirmed the full-screen path is smooth and slightly smoother than
  the background. The earlier transparent-surface VSync experiment is replaced.
- The full-screen app receives only the selected public catalog clips and shuffle
  setting. It advances through the selection, stays muted, pauses when hidden,
  and returns to Luma on a key, click or wheel input. Playback errors return Home.
  Luma stops NDL before handoff and restores its normal Home UI on return.
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

The personal branch defaults to LUMA_NDL_API=1. For an explicit upstream-style
DirectMedia v2 build instead, set LUMA_NDL_API=2 before packaging.

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
