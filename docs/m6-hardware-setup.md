# M6 Audio Output — Acceptance

## Setup
- Flash the M6 `kernel7.img` to the card (same card as M5; no config.txt HDMI
  changes — native display mode is still used).
- TV connected over HDMI with its speakers/volume on (audio is embedded in the
  HDMI signal).
- Optional: USB-TTL on the serial UART (115200) to see the audio init log line
  ("Audio: HDMI <rate> Hz ..." or "Audio disabled ...").

## Acceptance checklist
- [ ] Sonic demo: music AND sound effects are audible through the TV.
- [ ] No persistent crackle, buzzing, or dropouts during sustained play.
- [ ] Game runs at correct speed and audio/video stay in sync (audio is the
      pacing clock).
- [ ] A few minutes of play show no progressive drift or growing underruns.
- [ ] Fallback: if HDMI audio fails to init, serial shows "Audio disabled" and
      video still runs (timer-paced).

## If audio is absent or glitchy
- Check the serial log for the audio init line (disabled => init failed).
- Crackle/dropouts => tune AudioDriver::QUEUE_MS and the loop `target` (buffer
  depth vs latency).
- Confirm the TV input volume isn't muted and HDMI audio is selected on the TV.
