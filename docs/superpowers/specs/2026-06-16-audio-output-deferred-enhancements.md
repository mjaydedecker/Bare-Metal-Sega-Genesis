# Audio Output — Deferred / Higher-Order Enhancements

**Status:** backlog — revisit after the M6 base concept is proven on hardware.

## Context

The M6 base design picks the simplest path that gets clean Genesis audio out of a
Raspberry Pi 2 through the TV:

- **HDMI** output (`CHDMISoundBaseDevice`) — sound via the same TV used for video.
- **Write-queue** driver model; the core's **signed-16 stereo** samples are fed
  directly with no conversion.
- **Audio-driven pacing** — the audio queue is the frame clock (busy-wait on
  `GetQueueFramesAvail()`), replacing M5's timer placeholder.
- Minimal scope: audible + paced, fixed buffer, no volume/config.

During brainstorming we considered richer alternatives, deferred to keep the first
milestone small. Each notes **what it adds**, **why it was deferred**, and **where
it would plug in**.

---

## 1. Alternate / selectable output devices (PWM, I2S)

**What it adds:** Support the Pi's 3.5mm analog jack (`CPWMSoundDevice`, which was
the originally-wired member) and/or an I2S DAC HAT (`CI2SSoundBaseDevice`), and
let the output be chosen at runtime/config. Useful for analog setups, higher
fidelity, or HDMI displays without speakers.

**Why deferred:** HDMI alone covers the TV bring-up case; multiple back ends mean
device-selection plumbing and per-device testing.

**Where it plugs in:** `AudioDriver` already hides the device behind a small
interface (`Initialize/Write/QueuedFrames/IsReady`); add a device-type parameter
and instantiate the chosen `CSoundBaseDevice` subclass. All share the same Write
queue API.

## 2. Volume control

**What it adds:** Master volume / mute, scaling samples before `Write()` or via a
sound controller (`GetController()` where supported).

**Why deferred:** Not needed to prove audio; needs UI/input (menu or hotkeys,
later milestones).

**Where it plugs in:** a gain step in `AudioDriver::Write()`, or the device's
`CSoundController`.

## 3. config.txt-driven audio settings

**What it adds:** Drive sample rate, queue/buffer size (latency), and output
device from `config.txt` (ties into FSD §4.9 configuration), instead of the
hard-coded `QUEUE_MS` and core-reported rate.

**Why deferred:** Needs the config parser (a later milestone) and per-setup
testing.

**Where it plugs in:** pass parsed values into `AudioDriver::Initialize()` and the
output-device selection (#1).

## 4. Resampling / dynamic rate control

**What it adds:** Handle the case where the core's sample rate isn't natively
supported by the device (resample to 48 kHz), and/or **dynamic rate control** —
nudge the resampling ratio slightly to keep the queue centered, eliminating
long-term drift without hard busy-waits (the polished libretro sync technique).

**Why deferred:** The base feeds the core's rate (44.1 kHz) directly, which HDMI
supports, and queue-level pacing is sufficient. Resampling adds real DSP cost on
the Pi 2.

**Where it plugs in:** a resampler stage between the callback and
`AudioDriver::Write()`; DRC adjusts that resampler's ratio from the queue level.

## 5. Latency tuning, underrun/overrun metrics

**What it adds:** Make `QUEUE_MS`/`target` tunable and measured — count underruns
(queue hit empty) and overruns (dropped writes), expose them over serial to tune
latency vs. stability.

**Why deferred:** The base uses fixed, hand-picked sizes; metrics are an
optimization once basic audio is confirmed.

**Where it plugs in:** counters in `AudioDriver` around `Write()` and
`QueuedFrames()`; periodic serial log.

## 6. GetChunk pull / generator driver model

**What it adds:** The alternative Circle driver model — subclass the sound device
and override `GetChunk()` so the device pulls samples from a ring buffer we fill
from the core (instead of pushing via `Write()`).

**Why deferred:** The push/Write model fits a libretro source cleanly; the pull
model needs a custom subclass and lock-free sharing between the IRQ/DMA context
and the main loop. Only worth it if the Write model proves limiting.

**Where it plugs in:** replace `CHDMISoundBaseDevice` Write usage with a
`GetChunk` override feeding from a shared ring buffer.

---

## Cross-references

- FSD §4.5 (Audio Output), §4.9 (Configuration) — `Documents/Bare-Metal-Sega-Genesis-FSD.md`
- Implementation Plan §M6 — `Documents/Bare-Metal-Sega-Genesis-Implementation-Plan-Phase1.md`
- M6 base design — `docs/superpowers/specs/2026-06-16-m6-audio-output-design.md`
