# Region + Auto-Launch Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `region` (auto/ntsc/pal) and `auto_launch_rom` settings, wired into the existing settings subsystem, the core's region variable, the Settings screen, and the boot flow.

**Architecture:** `region` extends `Settings` with an enum mapped to the core's `genesis_plus_gx_wide_region_detect` variable (via the existing `GET_VARIABLE`/`g_*` mechanism), applied on ROM reload. `auto_launch_rom` is a path string set/cleared by a Settings-screen "Auto-launch this game" toggle (using the kernel-supplied current ROM path) and honored on the first boot pass.

**Tech Stack:** C++ (bare-metal, Circle), Genesis-Plus-GX-Wide libretro core, host-side g++ unit tests.

## Global Constraints

- Pure files (`src/settings/settings.{h,cpp}`) stay Circle-free (host-testable).
- `region` file values: `auto`/`ntsc`/`pal`. Core option strings: `auto`/`ntsc-u`/`pal` (the core also has `ntsc-j`; not exposed).
- Core region variable key: `genesis_plus_gx_wide_region_detect`.
- `auto_launch_rom` is a path up to 255 chars; empty means unset.
- Region applies on **next ROM load** (NTSC↔PAL changes fps, re-read only at load).
- Auto-launch fires only on the **first** browse→play pass; a missing/unreadable path falls back to the ROM browser (never bricks boot).
- Build `-Wall -Wextra`; member-init order matches declaration order (`-Wreorder`).
- New host test additions go in the existing `test/test_settings.cpp`.

---

## File Structure

**Modified files only:**
- `src/settings/settings.h` / `.cpp` — `Region` enum, `region` + `auto_launch_rom` fields, `region_file_value` / `region_core_value`, parse/serialize.
- `src/settings/settings_store.cpp` — enlarge the serialize buffer (a path makes the file exceed 256 bytes).
- `test/test_settings.cpp` — region + auto_launch coverage.
- `src/libretro/environment.h` / `.cpp` — `g_region_value` + region case in `GET_VARIABLE`.
- `src/menu/settings_screen.h` / `.cpp` — Region + Auto-launch rows, `SetCurrentRom`.
- `src/kernel.cpp` — boot apply region, first-boot auto-launch, `SetCurrentRom` per load.

No Makefile changes (no new files).

---

## Task 1: Settings model — region + auto_launch_rom

**Files:**
- Modify: `src/settings/settings.h`
- Modify: `src/settings/settings.cpp`
- Modify: `src/settings/settings_store.cpp`
- Modify: `test/test_settings.cpp`

**Interfaces:**
- Produces:
  - `enum class Region { Auto, NTSC, PAL };`
  - `Settings` gains `Region region;` (default `Auto`) and `char auto_launch_rom[256];` (default `""`).
  - `const char *region_file_value(Region r);` → `"auto"`/`"ntsc"`/`"pal"`
  - `const char *region_core_value(Region r);` → `"auto"`/`"ntsc-u"`/`"pal"`

- [ ] **Step 1: Write the failing test additions**

In `test/test_settings.cpp`, add before the `printf("All settings tests passed\n");` line:

```cpp
    // Region defaults + parse + round-trip.
    assert(d.region == Region::Auto);
    assert(parse_settings("region=pal\n").region == Region::PAL);
    assert(parse_settings("region=ntsc\n").region == Region::NTSC);
    assert(parse_settings("region=bogus\n").region == Region::Auto);

    // Region file + core value mappings.
    assert(strcmp(region_file_value(Region::Auto), "auto") == 0);
    assert(strcmp(region_file_value(Region::NTSC), "ntsc") == 0);
    assert(strcmp(region_file_value(Region::PAL),  "pal")  == 0);
    assert(strcmp(region_core_value(Region::NTSC), "ntsc-u") == 0);
    assert(strcmp(region_core_value(Region::PAL),  "pal")    == 0);
    assert(strcmp(region_core_value(Region::Auto), "auto")   == 0);

    // auto_launch_rom defaults empty; parses a path with spaces; round-trips.
    assert(d.auto_launch_rom[0] == '\0');
    Settings al = parse_settings("auto_launch_rom=SD:/roms/Streets of Rage.md\n");
    assert(strcmp(al.auto_launch_rom, "SD:/roms/Streets of Rage.md") == 0);

    Settings rs; rs.region = Region::PAL;
    const char *p = "SD:/roms/Sonic 2.md";
    for (unsigned i = 0; p[i]; i++) rs.auto_launch_rom[i] = p[i];
    char rbuf[512];
    serialize_settings(rs, rbuf, sizeof rbuf);
    Settings rrt = parse_settings(rbuf);
    assert(rrt.region == Region::PAL);
    assert(strcmp(rrt.auto_launch_rom, "SD:/roms/Sonic 2.md") == 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd test && make test_settings`
Expected: FAIL — `'struct Settings' has no member named 'region'` (and `region_file_value` undeclared).

- [ ] **Step 3: Extend the `Settings` struct and declare the helpers**

In `src/settings/settings.h`, replace the `enum class ScaleMode ...` + `struct Settings { ... };` block with:

```cpp
enum class ScaleMode { Integer, Stretch };
enum class Region    { Auto, NTSC, PAL };

struct Settings
{
    ScaleMode scale_mode;   // video_scale: integer | stretch
    bool      widescreen;   // widescreen:  on | off
    unsigned  volume;       // 0-100 master volume
    bool      mute;         // audio mute
    Region    region;       // region: auto | ntsc | pal
    char      auto_launch_rom[256];   // ROM path to boot into ("" = unset)

    Settings(void)
    :   scale_mode(ScaleMode::Integer), widescreen(false),
        volume(100), mute(false), region(Region::Auto)
    {
        auto_launch_rom[0] = '\0';
    }
};
```

Then, just below the `serialize_settings` declaration, add:

```cpp
// Region as written to the settings file ("auto" | "ntsc" | "pal").
const char *region_file_value(Region r);

// Region as the core's option value ("auto" | "ntsc-u" | "pal").
const char *region_core_value(Region r);
```

- [ ] **Step 4: Parse the new keys**

In `src/settings/settings.cpp`, in `parse_settings`, replace the
`else if (ieq(key, "mute"))` clause:

```cpp
        else if (ieq(key, "mute"))
            s.mute = truthy(val);
        // unknown keys: ignored
```

with:

```cpp
        else if (ieq(key, "mute"))
            s.mute = truthy(val);
        else if (ieq(key, "region"))
        {
            if      (ieq(val, "ntsc")) s.region = Region::NTSC;
            else if (ieq(val, "pal"))  s.region = Region::PAL;
            else                       s.region = Region::Auto;
        }
        else if (ieq(key, "auto_launch_rom"))
        {
            unsigned i = 0;
            for (; val[i] && i < sizeof(s.auto_launch_rom) - 1; i++)
                s.auto_launch_rom[i] = val[i];
            s.auto_launch_rom[i] = '\0';
        }
        // unknown keys: ignored
```

- [ ] **Step 5: Add the region helpers and serialize the new keys**

In `src/settings/settings.cpp`, add the helpers just above `serialize_settings`
(after `append_uint`):

```cpp
const char *region_file_value(Region r)
{
    switch (r)
    {
    case Region::NTSC: return "ntsc";
    case Region::PAL:  return "pal";
    default:           return "auto";
    }
}

const char *region_core_value(Region r)
{
    switch (r)
    {
    case Region::NTSC: return "ntsc-u";
    case Region::PAL:  return "pal";
    default:           return "auto";
    }
}
```

Then in `serialize_settings`, replace the mute tail:

```cpp
    appendz(out, out_size, "\nmute=");
    appendz(out, out_size, s.mute ? "on" : "off");
    appendz(out, out_size, "\n");
```

with:

```cpp
    appendz(out, out_size, "\nmute=");
    appendz(out, out_size, s.mute ? "on" : "off");
    appendz(out, out_size, "\nregion=");
    appendz(out, out_size, region_file_value(s.region));
    appendz(out, out_size, "\nauto_launch_rom=");
    appendz(out, out_size, s.auto_launch_rom);
    appendz(out, out_size, "\n");
```

- [ ] **Step 6: Enlarge the serialize buffer in SettingsStore**

In `src/settings/settings_store.cpp`, in `SettingsStore::Save`, change:

```cpp
    char text[256];
```

to:

```cpp
    char text[512];   // a ROM path pushes the file past 256 bytes
```

- [ ] **Step 7: Run the test to verify it passes**

Run: `cd test && make test_settings && ./test_settings`
Expected: `All settings tests passed`

- [ ] **Step 8: Build the kernel to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 9: Commit**

```bash
git add src/settings/settings.h src/settings/settings.cpp src/settings/settings_store.cpp test/test_settings.cpp
git commit -m "Settings: add region + auto_launch_rom fields"
```

---

## Task 2: Region core variable

**Files:**
- Modify: `src/libretro/environment.h`
- Modify: `src/libretro/environment.cpp`

**Interfaces:**
- Produces: `extern const char *g_region_value;` (default `"auto"`), served for `genesis_plus_gx_wide_region_detect`.

- [ ] **Step 1: Declare the global**

In `src/libretro/environment.h`, after the `extern bool g_variables_dirty;`
declaration, add:

```cpp
// Genesis region for the core's "genesis_plus_gx_wide_region_detect" variable:
// "auto" | "ntsc-u" | "pal". Set by the kernel/Settings screen.
extern const char *g_region_value;
```

- [ ] **Step 2: Define the global**

In `src/libretro/environment.cpp`, after `bool g_variables_dirty = false;`, add:

```cpp
const char *g_region_value = "auto";
```

- [ ] **Step 3: Serve the region variable in GET_VARIABLE**

In `src/libretro/environment.cpp`, replace the `GET_VARIABLE` body:

```cpp
            retro_variable *var = reinterpret_cast<retro_variable *>(data);
            if (var != 0 && var->key != 0 &&
                strcmp(var->key, "genesis_plus_gx_wide_h40_extra_columns") == 0)
            {
                var->value = g_widescreen ? "10" : "0";
                return true;
            }
            return false;   // other options: core keeps its defaults
```

with:

```cpp
            retro_variable *var = reinterpret_cast<retro_variable *>(data);
            if (var != 0 && var->key != 0)
            {
                if (strcmp(var->key,
                           "genesis_plus_gx_wide_h40_extra_columns") == 0)
                {
                    var->value = g_widescreen ? "10" : "0";
                    return true;
                }
                if (strcmp(var->key,
                           "genesis_plus_gx_wide_region_detect") == 0)
                {
                    var->value = g_region_value;
                    return true;
                }
            }
            return false;   // other options: core keeps its defaults
```

- [ ] **Step 4: Build to verify it compiles**

Run: `make`
Expected: builds to `kernel7.img`, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/libretro/environment.h src/libretro/environment.cpp
git commit -m "Video/core: region via environment GET_VARIABLE"
```

---

## Task 3: Settings screen — Region + Auto-launch rows

**Files:**
- Modify: `src/menu/settings_screen.h`
- Modify: `src/menu/settings_screen.cpp`

**Interfaces:**
- Consumes: `Settings.region` / `Settings.auto_launch_rom`, `region_core_value` (Task 1); `g_region_value` / `g_variables_dirty` (Task 2).
- Produces: `void SettingsScreen::SetCurrentRom(const char *path);` (stores a pointer used by the auto-launch row).

- [ ] **Step 1: Declare SetCurrentRom + the current-ROM pointer**

In `src/menu/settings_screen.h`, after `void Run(void);`, add:

```cpp
    // Tell the screen which ROM is currently running (for the auto-launch row).
    void SetCurrentRom(const char *path) { m_pRomPath = path; }
```

In the `private:` member list, after `AudioDriver   *m_pAudio;`, add:

```cpp
    const char    *m_pRomPath;
```

- [ ] **Step 2: Initialize the pointer and include string.h**

In `src/menu/settings_screen.cpp`, add after the existing includes:

```cpp
#include <string.h>   // strcmp, strncpy for the auto-launch row
```

In the constructor init list, append `m_pRomPath`:

```cpp
:   m_pCanvas(pCanvas), m_pGamepad(pGamepad), m_pUSBHCI(pUSBHCI),
    m_pSettings(pSettings), m_pStore(pStore), m_pDisplay(pDisplay),
    m_pAudio(pAudio), m_pRomPath(0)
{
}
```

- [ ] **Step 3: Apply region live in Apply()**

In `src/menu/settings_screen.cpp`, replace `Apply`:

```cpp
void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_variables_dirty = true;                            // ...on next poll/reset
    m_pAudio->SetVolume(m_pSettings->volume);            // live
    m_pAudio->SetMute(m_pSettings->mute);                // live
}
```

with:

```cpp
void SettingsScreen::Apply(void)
{
    m_pDisplay->SetScaleMode(m_pSettings->scale_mode);   // live
    g_widescreen      = m_pSettings->widescreen;         // core re-reads...
    g_region_value    = region_core_value(m_pSettings->region);
    g_variables_dirty = true;                            // ...on next poll/reset
    m_pAudio->SetVolume(m_pSettings->volume);            // live
    m_pAudio->SetMute(m_pSettings->mute);                // live
}
```

- [ ] **Step 4: Grow Render to 6 rows**

In `src/menu/settings_screen.cpp`, change `#define NUM_ROWS 4` to:

```cpp
#define NUM_ROWS 6
```

Replace the value/label setup in `Render`:

```cpp
    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *muteVal  = m_pSettings->mute ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal };
```

with:

```cpp
    char volVal[8];
    fmt_volume(volVal, m_pSettings->volume);
    const char *scaleVal = m_pSettings->scale_mode == ScaleMode::Stretch
                               ? "< Stretch >" : "< Integer >";
    const char *wideVal  = m_pSettings->widescreen ? "< On >" : "< Off >";
    const char *muteVal  = m_pSettings->mute ? "< On >" : "< Off >";
    const char *regionVal =
        m_pSettings->region == Region::NTSC ? "< NTSC >" :
        m_pSettings->region == Region::PAL  ? "< PAL >"  : "< Auto >";
    bool autoOn = m_pRomPath != 0 &&
                  strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
    const char *autoVal = autoOn ? "< On >" : "< Off >";
    const char *labels[NUM_ROWS] = { "Video Scale:", "Widescreen:",
                                     "Volume:", "Mute:",
                                     "Region:", "Auto-launch:" };
    const char *values[NUM_ROWS] = { scaleVal, wideVal, volVal, muteVal,
                                     regionVal, autoVal };
```

- [ ] **Step 5: Update the footer notes**

In `src/menu/settings_screen.cpp`, replace the footer note line:

```cpp
    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 4),
                        "Widescreen applies on reset.  B: back", WHITE, BOX);
```

with:

```cpp
    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 3),
                        "Widescreen: on reset.  Region: on reload.",
                        WHITE, BOX);
    m_pCanvas->DrawText(boxX + cw, boxY + ch * (NUM_ROWS + 4),
                        "Auto-launch boots this game.  B: back", WHITE, BOX);
```

- [ ] **Step 6: Handle the Region + Auto-launch edits in Run()**

In `src/menu/settings_screen.cpp`, in `Run`, replace the `case 3:` mute block
and the closing brace of the `switch` (i.e. the existing
`case 3: ... m_pSettings->mute = !m_pSettings->mute; break; }`) — specifically,
replace:

```cpp
            case 3:   // Mute (toggle)
                m_pSettings->mute = !m_pSettings->mute;
                break;
            }
```

with:

```cpp
            case 3:   // Mute (toggle)
                m_pSettings->mute = !m_pSettings->mute;
                break;
            case 4:   // Region (cycle Auto -> NTSC -> PAL)
            {
                int r = (int) m_pSettings->region + dir;
                if (r < 0) r = 2;
                if (r > 2) r = 0;
                m_pSettings->region = (Region) r;
                break;
            }
            case 5:   // Auto-launch this game (toggle)
            {
                bool on = m_pRomPath != 0 &&
                          strcmp(m_pSettings->auto_launch_rom, m_pRomPath) == 0;
                if (on)
                {
                    m_pSettings->auto_launch_rom[0] = '\0';
                }
                else if (m_pRomPath != 0)
                {
                    strncpy(m_pSettings->auto_launch_rom, m_pRomPath,
                            sizeof(m_pSettings->auto_launch_rom) - 1);
                    m_pSettings->auto_launch_rom[
                        sizeof(m_pSettings->auto_launch_rom) - 1] = '\0';
                }
                break;
            }
            }
```

- [ ] **Step 7: Build to verify it compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors. (The kernel does not call
`SetCurrentRom` yet — added in Task 4 — so the auto-launch row reads Off for now;
this still builds and links.)

- [ ] **Step 8: Commit**

```bash
git add src/menu/settings_screen.h src/menu/settings_screen.cpp
git commit -m "Settings: Region + Auto-launch rows in the Settings screen"
```

---

## Task 4: Kernel — boot region apply, first-boot auto-launch, current ROM

**Files:**
- Modify: `src/kernel.cpp`

**Interfaces:**
- Consumes: `region_core_value` / `Settings.region` / `Settings.auto_launch_rom` (Task 1); `g_region_value` (Task 2); `SettingsScreen::SetCurrentRom` (Task 3); `Storage::Exists`.

- [ ] **Step 1: Apply region at boot**

In `src/kernel.cpp`, after the settings-apply block, replace:

```cpp
	m_Audio.SetVolume (m_Settings.volume);
	m_Audio.SetMute (m_Settings.mute);
```

with:

```cpp
	m_Audio.SetVolume (m_Settings.volume);
	m_Audio.SetMute (m_Settings.mute);
	g_region_value = region_core_value (m_Settings.region);
```

- [ ] **Step 2: Add the first-boot flag**

In `src/kernel.cpp`, before the `for (;;)   // browse <-> play` loop, replace:

```cpp
	boolean audioInited = FALSE;
```

with:

```cpp
	boolean audioInited = FALSE;
	boolean firstBoot   = TRUE;
```

- [ ] **Step 3: Honor auto-launch on the first browse pass**

In `src/kernel.cpp`, replace the browse block:

```cpp
		// --- Browse ---
		char romPath[300];
		if (!m_RomMenu.Run (romPath, sizeof romPath))
		{
			m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
			return ShutdownHalt;   // RomMenu already drew the message
		}
```

with:

```cpp
		// --- Browse (or auto-launch on the very first pass) ---
		char romPath[300];
		boolean autoLaunched = FALSE;
		if (firstBoot && m_Settings.auto_launch_rom[0] != '\0' &&
		    m_Storage.Exists (m_Settings.auto_launch_rom))
		{
			unsigned i = 0;
			for (; m_Settings.auto_launch_rom[i] && i < sizeof romPath - 1; i++)
				romPath[i] = m_Settings.auto_launch_rom[i];
			romPath[i] = '\0';
			autoLaunched = TRUE;
		}
		firstBoot = FALSE;

		if (!autoLaunched && !m_RomMenu.Run (romPath, sizeof romPath))
		{
			m_Logger.Write (FromKernel, LogPanic, "No ROMs found in /roms");
			return ShutdownHalt;   // RomMenu already drew the message
		}
```

- [ ] **Step 4: Tell the Settings screen the current ROM**

In `src/kernel.cpp`, after the existing per-game setup, replace:

```cpp
		m_SaveState.SetGame (romPath);   // save/load target for this game
		m_Sram.SetGame (romPath);
		m_Sram.Load ();                  // restore battery SRAM if present
```

with:

```cpp
		m_SaveState.SetGame (romPath);   // save/load target for this game
		m_Sram.SetGame (romPath);
		m_Sram.Load ();                  // restore battery SRAM if present
		m_SettingsScreen.SetCurrentRom (romPath);   // auto-launch toggle target
```

- [ ] **Step 5: Build to verify everything compiles and links**

Run: `make`
Expected: builds to `kernel7.img`, no errors or `-Wreorder` warnings.

- [ ] **Step 6: Run the full host test suite**

Run: `cd test && make run`
Expected: all suites pass, including `All settings tests passed`.

- [ ] **Step 7: Commit**

```bash
git add src/kernel.cpp
git commit -m "Kernel: apply region at boot + first-boot auto-launch + current ROM"
```

---

## Hardware Verification (manual, after Task 4)

Not a code task — perform on the Pi:

- [ ] In a game, open Settings → set Region = PAL → Return to ROM Browser → reload the game; confirm it runs at 50 Hz (visibly/audibly slower). Set NTSC → ≈60 Hz.
- [ ] Open Settings → set "Auto-launch: On" for the current game; power-cycle → boots straight into that ROM (no browser).
- [ ] From the auto-launched game, pause → "Return to ROM Browser" reaches the browser.
- [ ] Set "Auto-launch: Off"; power-cycle → the ROM browser appears at boot.
- [ ] Inspect `SD:/settings.txt` → `region` and `auto_launch_rom` persisted.

---

## Self-Review Notes

- **Spec coverage:** `region` enum + file/core mappings + parse/serialize (Task 1); core variable served via `GET_VARIABLE` (Task 2); Region row applied-on-reload + Auto-launch toggle using current ROM (Task 3); boot region apply + first-boot auto-launch with browser fallback + `SetCurrentRom` (Task 4); host tests for region/auto_launch/mappings (Task 1); buffer enlarged so the path-bearing file serializes fully (Task 1); hardware checklist incl. PAL fps, auto-launch boot, escape hatch, persistence. On-screen text entry and `ntsc-j`/`menu_hotkey` are out of scope per the spec — no task.
- **Type consistency:** `Region`, `region`, `auto_launch_rom`, `region_file_value`, `region_core_value` defined in Task 1 are used unchanged in Tasks 3–4; `g_region_value` defined in Task 2 used in Tasks 3–4; `SetCurrentRom` defined in Task 3 used in Task 4.
- **Green builds:** Task 3 builds even before the kernel calls `SetCurrentRom` (defaults to Off); every task ends with a clean full `make`.
