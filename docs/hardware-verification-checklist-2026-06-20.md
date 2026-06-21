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

- [ ] **A1 — No ghost input on unplug.** Start a game, hold a D-pad direction so
  the character keeps moving, and **unplug the pad mid-press**.
  **Expect:** on-screen motion stops within ~a frame. No stuck/repeating input.
- [ ] **A2 — Re-plug restores same port.** Plug the same pad back into the same hub/port.
  **Expect:** it controls the game again, as **Player 1** (same port it had).
- [ ] **A3 — P2 unplug doesn't disturb P1.** With both pads active in a 2-player
  game (or both able to move a cursor), unplug **P2**.
  **Expect:** P1 keeps working normally; only P2 goes neutral.
- [ ] **A4 — P1 unplug doesn't disturb P2.** Re-plug P2, then unplug **P1**.
  **Expect:** P2 keeps working; P1 goes neutral.
- [ ] **A5 — Menu still works after a cycle.** After an unplug/re-plug cycle,
  press **Start+Select** on either pad.
  **Expect:** pause menu opens (proves the handler re-registered).

---

## B. Two controllers (P1 / P2)

- [ ] **B1 — Independent players.** In a 2-player game, confirm each pad drives its
  own player simultaneously (move both at once).
  **Expect:** no cross-talk, no doubled inputs.
- [ ] **B2 — Either pad opens the menu.** Press the pause hotkey on **P1**, close,
  then on **P2**.
  **Expect:** both open the pause menu (`MenuButtons()` ORs both pads).
- [ ] **B3 — One-pad fallback.** Boot with only **one** pad connected.
  **Expect:** P1 works normally; no hang waiting for P2.

---

## C. Per-player remapping (Controls screen)

- [ ] **C1 — Open Controls.** Pause → **Settings** → **Controls...** (open with **Start**).
  **Expect:** P1/P2 selector + 8 cycle rows (a,b,x,y,l,r,start,select).
- [ ] **C2 — Remap P1.** Change one P1 button (e.g. swap A and B), back out, resume.
  **Expect:** in-game, that button now does the remapped action.
- [ ] **C3 — Persisted.** Power-cycle (or pull card) and confirm `controller_1_map`
  in `SD:/settings.txt` reflects the change; on reboot the remap still applies.
- [ ] **C4 — P2 independent.** Remap a P2 button; confirm it affects only P2
  (`controller_2_map`), P1 unchanged.

---

## D. Audio settings (volume + mute)

- [ ] **D1 — Volume scales.** Pause → Settings → set **volume** to a low value
  (e.g. 20), resume.
  **Expect:** audibly quieter, not just on/off.
- [ ] **D2 — Volume 100.** Set back to 100.
  **Expect:** full level returns.
- [ ] **D3 — Mute = silence (not skip).** Toggle **mute on**.
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

- [ ] **F1 — Set PAL.** Pause → Settings → **region** = PAL. Region applies on ROM
  reload, so reload the ROM (browser → relaunch, or power-cycle).
  **Expect:** game runs in PAL (50 Hz — typically slightly slower / different
  border timing on region-sensitive titles).
- [ ] **F2 — Back to NTSC/auto.** Set region = NTSC (or auto), reload.
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

- [ ] **I1 — Auto-revert on no input.** Settings → **Video Mode...** → pick a
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

## Results summary

| Group | Pass | Notes |
|-------|------|-------|
| A. Hotplug-removal | | |
| B. Two controllers | | |
| C. Remapping | | |
| D. Audio settings | | |
| E. SRAM saves | | |
| F. Region switch | | |
| G. auto_launch_rom | | |
| H. menu_hotkey | | |
| I. Mode auto-revert | | |
| J. Analog audio | | |
| K. Tear-free (vsync) | | |
| L. Aspect scaling | | |

Anything that fails: note the exact symptom and which ROM, and we'll debug it
systematically (root cause first).
