# Hardware Verification Checklist — 2026-06-20

One-session pass to confirm the features that are code-complete on `main` but not
yet verified on the Pi 2. Work top to bottom; each item is self-contained.

**Setup once before starting**
- SD card with current `kernel7.img` flashed, `/roms` populated, `SD:/settings.txt` present.
- TV + HDMI (audio over HDMI), at least **two USB controllers** for the multi-pad tests.
- Default pause hotkey is **Start+Select**; the pause menu draws on the game screen.
- Settings live in `SD:/settings.txt`. After tests that change settings, you can
  pull the card and confirm the written values (noted where relevant).

Mark each box ✅ pass / ❌ fail (note what you saw).

---

## A. Controller hotplug-removal
*(just merged — `Poll()` reconcile loop)*

- [x] **A1 — No ghost input on unplug.** Start a game, hold a D-pad direction so
  the character keeps moving, and **unplug the pad mid-press**.
  **Expect:** on-screen motion stops within ~a frame. No stuck/repeating input.
- [x] **A2 — Re-plug restores same port.** Plug the same pad back into the same hub/port.
  **Expect:** it controls the game again, as **Player 1** (same port it had).
- [x] **A3 — P2 unplug doesn't disturb P1.** With both pads active in a 2-player
  game (or both able to move a cursor), unplug **P2**.
  **Expect:** P1 keeps working normally; only P2 goes neutral.
- [x] **A4 — P1 unplug doesn't disturb P2.** Re-plug P2, then unplug **P1**.
  **Expect:** P2 keeps working; P1 goes neutral.
- [x] **A5 — Menu still works after a cycle.** After an unplug/re-plug cycle,
  press **Start+Select** on either pad.
  **Expect:** pause menu opens (proves the handler re-registered).

---

## B. Two controllers (P1 / P2)

- [x] **B1 — Independent players.** In a 2-player game, confirm each pad drives its
  own player simultaneously (move both at once).
  **Expect:** no cross-talk, no doubled inputs.
- [x] **B2 — Either pad opens the menu.** Press the pause hotkey on **P1**, close,
  then on **P2**.
  **Expect:** both open the pause menu (`MenuButtons()` ORs both pads).
- [x] **B3 — One-pad fallback.** Boot with only **one** pad connected.
  **Expect:** P1 works normally; no hang waiting for P2.

---

## C. Per-player remapping (Controls screen)

- [x] **C1 — Open Controls.** Pause → **Settings** → **Controls...** (open with **Start**).
  **Expect:** P1/P2 selector + 8 cycle rows (a,b,x,y,l,r,start,select).
- [ ] **C2 — Remap P1.** Change one P1 button (e.g. swap A and B), back out, resume.
  **Expect:** in-game, that button now does the remapped action.
- [ ] **C3 — Persisted.** Power-cycle (or pull card) and confirm `controller_1_map`
  in `SD:/settings.txt` reflects the change; on reboot the remap still applies.
- [ ] **C4 — P2 independent.** Remap a P2 button; confirm it affects only P2
  (`controller_2_map`), P1 unchanged.

---

## D. Audio settings (volume + mute)

- [x] **D1 — Volume scales.** Pause → Settings → set **volume** to a low value
  (e.g. 20), resume.
  **Expect:** audibly quieter, not just on/off.
- [x] **D2 — Volume 100.** Set back to 100.
  **Expect:** full level returns.
- [x] **D3 — Mute = silence (not skip).** Toggle **mute on**.
  **Expect:** complete silence, and the game keeps running at normal speed/pitch
  (mute writes silence at the same rate — no slowdown or pitch change). Unmute → audio returns.
- [ ] **D4 — Persisted.** Confirm `volume`/`mute` in `SD:/settings.txt` after a change.

---

## E. SRAM / battery saves

- [ ] **E1 — Auto-save on play.** Load a battery-backed game (e.g. a Sonic with
  save, an RPG). Create/modify in-game save data, then play a bit so the periodic
  autosave fires.
  **Expect:** `SD:/saves/<rom>.srm` exists after (pull card to check).
- [ ] **E2 — Save on ROM change.** Trigger a save, then return to the browser and
  switch ROMs.
  **Expect:** `.srm` written on the way out.
- [ ] **E3 — Persistence across boot.** Power-cycle, reload the same ROM.
  **Expect:** in-game battery save data is still there.

---

## F. Region switch (NTSC / PAL)

- [x] **F1 — Set PAL.** Pause → Settings → **region** = PAL. Region applies on ROM
  reload, so reload the ROM (browser → relaunch, or power-cycle).
  **Expect:** game runs in PAL (50 Hz — typically slightly slower / different
  border timing on region-sensitive titles).
- [x] **F2 — Back to NTSC/auto.** Set region = NTSC (or auto), reload.
  **Expect:** 60 Hz behavior returns.
- [ ] **F3 — Persisted.** Confirm `region` in `SD:/settings.txt`.

---

## G. auto_launch_rom

- [ ] **G1 — Set auto-launch.** Load a ROM, pause → Settings → **Auto-launch this
  game** toggle on. Power-cycle.
  **Expect:** that ROM boots directly without the browser.
- [ ] **G2 — Browser still reachable.** From the auto-launched game, pause → Return.
  **Expect:** ROM browser opens.
- [ ] **G3 — Clear it.** Turn the toggle off (or clear `auto_launch_rom`), reboot.
  **Expect:** boots to the browser again.

---

## H. menu_hotkey presets

- [ ] **H1 — Change hotkey.** Settings → **menu_hotkey** to a non-default preset
  (e.g. Start+A, Start+B, or L+R).
  **Expect:** the new combo opens the pause menu; old combo (Start+Select) no
  longer does (read live each frame, so should apply without reboot).
- [ ] **H2 — Persisted.** Confirm `menu_hotkey` in `SD:/settings.txt`; survives reboot.
- [ ] **H3 — Restore.** Set back to Start+Select.

---

## I. HDMI mode picker — AUTO-REVERT only
*(apply+confirm already verified — only the safety countdown remains)*

- [x] **I1 — Auto-revert on no input.** Settings → **Video Mode...** → pick a
  *different* mode → it applies and shows the "Keep this mode? A=keep" ~15s
  countdown. **Do nothing** — do not press A.
  **Expect:** after ~15s it reverts to the previous mode on its own (no black
  screen lock-up). This is the safety net that makes a rejected mode recoverable.
- [ ] **I2 — Not persisted on revert.** Confirm `video_mode` in `SD:/settings.txt`
  is **unchanged** (only a confirmed mode is ever written).

---

## J. Analog (3.5mm) audio output

- [ ] **J1 — Switch to analog.** Pause → Settings → **Audio out** = Analog.
  Plug headphones/speakers into the Pi's 3.5mm jack and **reboot** (applies on
  boot, not live). **Expect:** game audio comes out of the 3.5mm jack; HDMI is
  silent.
- [ ] **J2 — Volume/mute still work on analog.** With Analog selected, change
  volume and toggle mute. **Expect:** they scale/silence the analog output the
  same as HDMI.
- [ ] **J3 — Persisted.** Confirm `audio_output=analog` in `SD:/settings.txt`.
- [ ] **J4 — Back to HDMI.** Set Audio out = HDMI, reboot.
  **Expect:** audio returns to HDMI; `audio_output=hdmi` in the file.
- [ ] **J5 — I2S DAC output.** Connect a PCM5102 I2S DAC HAT. Settings → **Audio
  out** = I2S DAC → reboot. **Expect:** game audio comes from the DAC; HDMI and
  the 3.5 mm jack are silent. Confirm `audio_output=i2s` in `SD:/settings.txt`.
- [ ] **J6 — I2S volume/mute + clean.** With I2S selected, change volume and
  toggle mute (they should scale/silence the DAC like the other outputs).
  **Expect:** audio is clean — no choppiness/dropouts. If choppy, note it: the
  fix is a smaller explicit `nChunkSize` in the `CI2SSoundBaseDevice` ctor.
- [ ] **J7 — Back to HDMI from I2S.** Set Audio out = HDMI, reboot. **Expect:**
  audio returns to HDMI; `audio_output=hdmi` in the file.

---

## K. Tear-free output (vsync)

- [ ] **K1 — No tearing.** Play a horizontally-scrolling game (e.g. Sonic).
  **Expect:** no tear line across the screen during scroll (vsync defaults on).
- [ ] **K2 — Toggle proves the path.** Pause → Settings → **Vsync** = Off, resume.
  **Expect:** tearing returns (off == old single-buffer path). Set back to On →
  tearing gone. (Live, no reboot.)
- [ ] **K3 — No pacing regression.** With Vsync on, confirm the game runs full
  speed and the periodic underrun/overrun log doesn't climb vs. Vsync off.
- [ ] **K4 — Menus clean.** Open/close the pause menu several times during play.
  **Expect:** no menu remnants or letterbox-bar artifacts after resuming.
- [ ] **K5 — Persisted.** Confirm `vsync=on` (or `off`) in `SD:/settings.txt`;
  survives reboot.

---

## L. Aspect-correct scaling

- [ ] **L1 — Selectable.** Pause → Settings → **Video Scale**. Left/Right cycles
  Integer → Stretch → Aspect; the `< Aspect >` label shows.
- [ ] **L2 — Live + persisted.** Selecting Aspect applies without reboot; confirm
  `video_scale=aspect` in `SD:/settings.txt` and that it survives a reboot.
- [ ] **L3 — Equal width / correct proportions.** Compare an H40/320-wide game
  (e.g. Sonic) and an H32/256-wide game. **Expect:** both fill the SAME display
  width and look correctly proportioned (not thin/tall).
- [ ] **L4 — Crisp vertical.** **Expect:** vertical edges are sharp (integer
  scale); no excessive horizontal blur.
- [ ] **L5 — Acceptable speed.** Frame rate at the chosen video_mode is acceptable
  vs. stretch (cap to 720p if 1080p is not smooth).

---

## M. Diagnostics HUD (debug_overlay)

- [x] **M1 — Toggle.** Pause → Settings → **Debug Overlay** = On, resume.
  **Expect:** a small text box (top-left) shows FPS, U/O, AQ, ROM, mode/scale.
- [x] **M2 — Live values.** FPS reads ~60 on a game that keeps up; the U/O and
  AQ numbers update over time and match the ~5 s `audio underruns/overruns` log.
- [x] **M3 — Context correct.** ROM name and `mode  scale` line match the loaded
  game and current settings (e.g. `1080p  aspect`).
- [x] **M4 — Clean off.** Set Debug Overlay = Off; **Expect:** the HUD disappears
  with no ghost text left in the letterbox bars or over the game.
- [x] **M5 — No regression.** With the HUD on, confirm FPS and the underrun count
  do not worsen versus HUD off.
- [x] **M6 — Persisted.** Confirm `debug_overlay=on` in `SD:/settings.txt`;
  survives reboot (HUD shows from boot).

---

## N. In-game hotkeys + toasts

- [ ] **N1 — Quick-save/load.** In-game: `Select+X` shows `Quick-saved`; `Select+Y`
  shows `Quick-loaded` and restores the state. On a fresh game `Select+Y` (empty
  slot 1) shows `No quick save`.
- [ ] **N2 — Volume.** `Select+Up`/`Select+Down` change volume live; toast shows
  `Volume NN`; the level persists across reboot.
- [ ] **N3 — HUD + mute.** `Select+A` toggles the diagnostics HUD (toast `HUD on`/
  `HUD off`, no ghost when turned off); `Select+B` toggles mute (`Muted`/
  `Unmuted`). Both persist across reboot.
- [ ] **N4 — Suppression.** While Select is held, the game receives no player-1
  input; releasing Select restores normal play. Player 2 is unaffected.
- [ ] **N5 — Toast lifetime.** Each toast appears bottom-center and disappears
  after ~2 s with no leftover pixels over the game.
- [ ] **N6 — No menu collision.** The configured `menu_hotkey` still opens the
  pause menu and does not trigger an in-game action.

---

## O. Hotkey remapping

- [ ] **O1 — Remap works.** Settings → `Hotkeys...`; change an action's key (and/or
  hold). The new combo performs the action in-game; the old combo no longer does.
- [ ] **O2 — Persisted.** The `hotkey_*` keys appear in `SD:/settings.txt` and the
  remap survives reboot.
- [ ] **O3 — Conflict flag.** Set two actions to the same combo: both rows show a
  red `!`; the higher-priority action (earlier in the list) still works.
- [ ] **O4 — Suppression follows hold.** Change an action's hold to L; while L is
  held, player-1 input is masked from the game. Select-held still masks if any
  action uses Select.
- [ ] **O5 — Defaults.** With a fresh `settings.txt`: Quick-save=Select+X,
  load=Select+Y, HUD=Select+A, mute=Select+B, vol+=Select+L, vol-=Select+R.
- [ ] **O6 — No menu collision.** The configured `menu_hotkey` still opens the
  pause menu.

---

## P. Audio latency presets

- [ ] **P1 — Live change.** Pause → Settings → **Audio Latency**. Left/Right cycles
  Low / Medium / High; selecting applies on resume without reboot.
- [ ] **P2 — Low is clean.** Set Low on a demanding game. **Expect:** audio stays
  clean — the underrun (U) counter in the HUD / ~5 s log does not climb steadily.
- [ ] **P3 — High reflects in HUD.** Set High. **Expect:** the HUD `target` value
  rises (more frames buffered); audio still clean.
- [ ] **P4 — Medium == before.** Set Medium. **Expect:** behaves exactly as prior
  to this feature.
- [ ] **P5 — Persisted.** Confirm `audio_latency` in `SD:/settings.txt`; survives reboot.

---

## Q. Boot splash screen

- [x] **Q1 — Embedded logo on boot.** Power on. **Expect:** the placeholder logo
  (three colored bars on a dark field) appears early and stays through the USB/SD
  wait, then the ROM browser replaces it. No corruption or hang.
- [ ] **Q2 — SD override.** Make `splash.raw` with
  `python3 tools/mksplash.py --placeholder --raw splash.raw` (or from a PNG),
  copy it to the SD root, reboot. **Expect:** after the card mounts, that image
  replaces the embedded logo for the rest of boot.
- [x] **Q3 — Bad/absent override is harmless.** Remove or truncate
  `SD:/splash.raw`, reboot. **Expect:** the embedded logo shows for the whole
  boot; no hang.

---

## R. Pad type (3/6-button)

- [ ] **R1 — Switch to 3-button.** Pause → Settings → **Pad Type** = 3-button,
  reload the game (browser relaunch or power-cycle). **Expect:** the game now sees
  a 3-button pad; X/Y/Z/Mode no longer register. A title that misbehaved with a
  6-button pad now plays correctly.
- [ ] **R2 — Back to 6-button.** Set Pad Type = 6-button, reload. **Expect:** the
  X/Y/Z/Mode buttons work again.
- [ ] **R3 — Persisted.** Confirm `pad_type` in `SD:/settings.txt`; survives reboot.

---

## S. Controller calibration (auto-mapping)

- [x] **S1 — Calibrate a non-8BitDo pad.** Plug a different USB pad as P1 (buttons
  wrong/dead). Settings → **Calibrate Controller...** → press each prompted button.
  **Expect:** after the 8th, "Saved"; in-game all 8 buttons now work correctly.
- [ ] **S2 — Persisted.** `SD:/controllers.txt` has a `vid:pid=...` line; power-cycle
  and the pad works without re-calibrating.
- [ ] **S3 — Skip / cancel.** D-pad **Down** skips a button (leaves it dead);
  D-pad **Left** cancels the screen with no change saved.
- [ ] **S4 — Fallback intact.** An 8BitDo (uncalibrated) still works out of the box.

---

## Results summary

| Group | Pass | Notes |
|-------|------|-------|
| A. Hotplug-removal | ✅ | A1–A5 pass |
| B. Two controllers | ✅ | B1–B3 pass |
| C. Remapping | partial | C1 pass; C2–C4 not yet run |
| D. Audio settings | partial | D1–D3 pass; D4 (persist) not yet run |
| E. SRAM saves | | |
| F. Region switch | partial | F1–F2 pass; F3 (persist) not yet run |
| G. auto_launch_rom | | |
| H. menu_hotkey | | |
| I. Mode auto-revert | partial | I1 pass; I2 (not-persisted) not yet run |
| J. Analog audio | | |
| K. Tear-free (vsync) | | |
| L. Aspect scaling | | |
| M. Diagnostics HUD | ✅ | M1–M6 pass |
| N. Hotkeys + toasts | | |
| O. Hotkey remapping | | |
| P. Audio latency | | |
| Q. Boot splash | partial | Q1, Q3 pass; Q2 (SD override) not yet run |
| R. Pad type | | |
| S. Controller calib | partial | S1 pass; S2–S4 not yet run |

Anything that fails: note the exact symptom and which ROM, and we'll debug it
systematically (root cause first).
