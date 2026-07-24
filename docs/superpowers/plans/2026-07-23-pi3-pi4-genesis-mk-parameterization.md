# Pi 3/4 genesis.mk Parameterization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `make RASPPI=3` and `make RASPPI=4` produce working cross-compiled kernel images (in 32-bit/AArch32 mode), while `make` with no arguments keeps building for Pi 2 exactly as today.

**Architecture:** Circle's own `Rules.mk` already supports `RASPPI=3`/`RASPPI=4` in AArch32 — only `libs/genesis.mk` (the emulator core build) hardcodes Pi 2's CPU flags. Make the top-level `Makefile`'s `AARCH`/`RASPPI` variables command-line-overridable and forward them into `genesis.mk`, then branch `genesis.mk`'s `CFLAGS` on `RASPPI` using the same per-board `-mcpu`/`-mfpu` values Circle's `Rules.mk` uses for that board, so our core objects and Circle's libraries always share the same ABI.

**Tech Stack:** GNU Make, `arm-linux-gnueabihf-gcc` cross-toolchain, Circle bare-metal framework (git submodule), Genesis-Plus-GX-Wide libretro core (git submodule).

## Global Constraints

- Scope is **RASPPI=2/3/4 in AArch32 only**. RASPPI=5 (Pi 5) has no AArch32 target in Circle at all, and AArch64 needs a separate CFLAGS rework — both must fail loudly with a clear `$(error ...)`, not silently compile with wrong flags.
- `make` with no arguments must keep producing an identical Pi 2 build (`kernel7.img`, `-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard`) — no regression.
- No output-path namespacing by board — switching `RASPPI` requires a full clean first (see Task 1's `clean-all` target). This matches how Circle's own per-library `.a` files already aren't board-namespaced.
- Verification is cross-compile/link only — no Pi hardware is available for this task, so no boot testing.
- Full spec: `docs/superpowers/specs/2026-07-23-pi3-pi4-genesis-mk-parameterization-design.md`.

---

## Discovered during planning: `make clean` alone is not enough

The approved spec says switching boards requires `make clean`. While mapping out the exact verification commands, I found this is insufficient on its own: the top-level `clean` target (inherited from Circle's `Rules.mk`) only removes files in the repo root plus `EXTRACLEAN` (our `src/`, `build/genesis/`, `libs/libgenesis.a`). It does **not** touch Circle's own per-library build artifacts under `libs/circle/lib/*/`, `libs/circle/addon/*/` (e.g. `libusb.a`, `libsound.a` and their `.o`/`.d` files). Those `.a` files are only rebuilt by their `$(MAKE) -C ...` recipe when missing — Make has no way to know `RASPPI` changed — so a plain `make clean && make RASPPI=3` would silently relink Pi-2-arch objects from a prior build into a "Pi 3" image.

Task 1 below adds a `clean-all` target that also runs `git -C libs/circle clean -fdx` to remove every untracked build artifact in the Circle submodule (verified safe: `git -C libs/circle status --porcelain` is empty and a dry-run `clean -fdxn` shows only `.o`/`.d`/`.a`/generated `Config.mk` — nothing that isn't a build product). This is what the verification steps in Task 2 use between board switches.

---

### Task 1: Parameterize the top-level Makefile for board selection

**Files:**
- Modify: `Makefile:9-12` (AARCH/RASPPI declaration)
- Modify: `Makefile:138-139` (`libs/libgenesis.a` recipe)
- Modify: `Makefile` (add `clean-all` target near the end, before `-include $(DEPS)`)

**Interfaces:**
- Produces: `AARCH` and `RASPPI` become command-line-overridable (`make RASPPI=3`), and are forwarded to `libs/genesis.mk` as `AARCH=$(AARCH) RASPPI=$(RASPPI)` — Task 2 relies on `genesis.mk` receiving these.
- Produces: `make clean-all` target — Task 2's verification steps use this between board switches.

- [ ] **Step 1: Edit the board-selection variables**

In `Makefile`, replace:

```makefile
# Target: Raspberry Pi 2, AArch32
# Produces: kernel7.img
AARCH   = 32
RASPPI  = 2
```

with:

```makefile
# Target board — override on the command line, e.g. `make RASPPI=3`.
# Default: Raspberry Pi 2, AArch32, producing kernel7.img.
# Supported: RASPPI=2 (kernel7.img), RASPPI=3 (kernel8-32.img),
# RASPPI=4 (kernel7l.img). AARCH=64 / RASPPI=5 (Pi 5) not yet supported.
# Switching RASPPI requires `make clean-all` first (see below) — a plain
# `make clean` does not remove Circle's own per-library build artifacts,
# which aren't rebuilt automatically when the target board changes.
AARCH   ?= 32
RASPPI  ?= 2
```

- [ ] **Step 2: Forward AARCH/RASPPI to the genesis core build**

In `Makefile`, replace:

```makefile
libs/libgenesis.a:
	$(MAKE) -f libs/genesis.mk PREFIX=$(PREFIX)
```

with:

```makefile
libs/libgenesis.a:
	$(MAKE) -f libs/genesis.mk PREFIX=$(PREFIX) AARCH=$(AARCH) RASPPI=$(RASPPI)
```

- [ ] **Step 3: Add the `clean-all` target**

In `Makefile`, immediately before the final `-include $(DEPS)` line, add:

```makefile
# `make clean` (from Circle's Rules.mk) only removes objects in this
# directory plus EXTRACLEAN — it does not touch Circle's own per-library
# build artifacts under $(CIRCLEHOME)/lib/*, $(CIRCLEHOME)/addon/*. Those
# aren't rebuilt when RASPPI/AARCH change (Make has no flag-based
# dependency tracking), so switching boards with a plain `make clean`
# silently relinks stale-arch objects into the new image. `clean-all`
# clears everything, including Circle's generated build artifacts
# (git-clean is safe here: Circle is a submodule and its build output is
# entirely untracked).
.PHONY: clean-all
clean-all: clean
	git -C $(CIRCLEHOME) clean -fdx
```

- [ ] **Step 4: Verify no regression on the default (Pi 2) build**

Run:

```sh
make clean-all >/dev/null 2>&1
make 2>&1 | tail -20
```

Expected: build completes with no errors, ending with lines like `  WC    kernel7.img => <size>`. Confirm the artifact exists:

```sh
test -f kernel7.img && echo OK
```

Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add Makefile
git commit -m "$(cat <<'EOF'
build: make AARCH/RASPPI overridable, forward to genesis.mk build

Lets `make RASPPI=3` / `make RASPPI=4` select the target board from the
command line instead of editing the Makefile. Adds clean-all since
Circle's per-library build artifacts aren't rebuilt automatically when
the board changes and a plain `make clean` leaves them stale.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Parameterize `libs/genesis.mk` CFLAGS per board

**Files:**
- Modify: `libs/genesis.mk:15-70`

**Interfaces:**
- Consumes: `AARCH`, `RASPPI` make variables forwarded from the top-level `Makefile` (Task 1, Step 2). Defaults to `AARCH=32 RASPPI=2` when invoked standalone (unchanged from today).
- Produces: `libs/libgenesis.a` built with board-matched CPU flags; `$(error ...)` for any unsupported `RASPPI`/`AARCH` combination.

- [ ] **Step 1: Branch CFLAGS on RASPPI, guard unsupported combinations**

In `libs/genesis.mk`, replace:

```makefile
PREFIX   ?= arm-linux-gnueabihf-
CC        = $(PREFIX)gcc
AR        = $(PREFIX)ar
```

with:

```makefile
PREFIX   ?= arm-linux-gnueabihf-
CC        = $(PREFIX)gcc
AR        = $(PREFIX)ar

AARCH    ?= 32
RASPPI   ?= 2
```

Then replace:

```makefile
# ---------------------------------------------------------------------------
# Compile flags — Cortex-A7 hard-float, same ABI as Circle
# ---------------------------------------------------------------------------
CFLAGS = \
    -mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard \
    -O2 -std=gnu99 -fsigned-char \
```

with:

```makefile
# ---------------------------------------------------------------------------
# Per-board CPU flags — mirrors Circle's Rules.mk ARCHCPU cases for AArch32
# (see libs/circle/Rules.mk) so this core always matches the -mcpu/-mfpu
# Circle itself uses for that board. RASPPI=5 (Pi 5) has no AArch32 target
# in Circle at all — it's AArch64-only — and AArch64 needs its own CFLAGS
# rework here (drops -marm/-mfpu/-mfloat-abi). Both unsupported for now.
# ---------------------------------------------------------------------------
ifneq ($(strip $(AARCH)),32)
$(error libs/genesis.mk only supports AARCH=32 currently (got AARCH=$(AARCH)))
endif

ifeq ($(strip $(RASPPI)),2)
ARCHCPU = -mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard
else ifeq ($(strip $(RASPPI)),3)
ARCHCPU = -mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(RASPPI)),4)
ARCHCPU = -mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else
$(error libs/genesis.mk only supports RASPPI=2, 3 or 4 currently (got RASPPI=$(RASPPI)))
endif

# ---------------------------------------------------------------------------
# Compile flags — same ABI as Circle for the selected board
# ---------------------------------------------------------------------------
CFLAGS = \
    $(ARCHCPU) \
    -O2 -std=gnu99 -fsigned-char \
```

The rest of the `CFLAGS` block (from `-U_FORTIFY_SOURCE` through `$(INCFLAGS)`) is unchanged.

- [ ] **Step 2: Verify RASPPI=2 regression (default board)**

```sh
make clean-all >/dev/null 2>&1
make 2>&1 | tee /tmp/build-pi2.log | tail -20
grep -c 'mcpu=cortex-a7 ' /tmp/build-pi2.log
test -f kernel7.img && echo OK
```

Expected: build succeeds, the `grep -c` count is greater than 0 (confirms the genesis-core compile lines used cortex-a7 flags), and `OK` is printed.

- [ ] **Step 3: Verify RASPPI=3 build**

```sh
make clean-all >/dev/null 2>&1
make RASPPI=3 2>&1 | tee /tmp/build-pi3.log | tail -20
grep -c 'mcpu=cortex-a53 ' /tmp/build-pi3.log
test -f kernel8-32.img && echo OK
```

Expected: build succeeds, `grep -c` count > 0, `OK` is printed (confirms `kernel8-32.img` — Circle's own `TARGET` default for `RASPPI=3` in AArch32).

- [ ] **Step 4: Verify RASPPI=4 build**

```sh
make clean-all >/dev/null 2>&1
make RASPPI=4 2>&1 | tee /tmp/build-pi4.log | tail -20
grep -c 'mcpu=cortex-a72 ' /tmp/build-pi4.log
test -f kernel7l.img && echo OK
```

Expected: build succeeds, `grep -c` count > 0, `OK` is printed (confirms `kernel7l.img` — Circle's own `TARGET` default for `RASPPI=4` in AArch32).

- [ ] **Step 5: Verify the unsupported-board guard fires**

Use `RASPPI=1`, not `RASPPI=5`, for this check: Circle's own `Rules.mk` already
rejects `RASPPI=5` in AArch32 mode by itself (`RASPPI must be set to 1, 2, 3
or 4`) before the build ever reaches `genesis.mk`, so it wouldn't exercise
our new guard. `RASPPI=1` is a value Circle's `Rules.mk` *does* accept in
AArch32 (it's Pi 1/Zero), so parsing succeeds and the build reaches our
`libs/libgenesis.a` recipe, where our guard fires:

```sh
make clean-all >/dev/null 2>&1
make RASPPI=1 2>&1 | tail -5
```

Expected: build fails, output includes `libs/genesis.mk only supports RASPPI=2, 3 or 4 currently (got RASPPI=1)`.

For the AArch64 guard, use a `RASPPI` value Circle's `Rules.mk` accepts in
AArch64 mode (3, 4, or 5) so parsing again succeeds and the build reaches
our guard instead of Circle's:

```sh
make clean-all >/dev/null 2>&1
make AARCH=64 RASPPI=3 2>&1 | tail -5
```

Expected: build fails, output includes `libs/genesis.mk only supports AARCH=32 currently (got AARCH=64)`.

- [ ] **Step 6: Restore the default build state**

```sh
make clean-all >/dev/null 2>&1
make 2>&1 | tail -5
test -f kernel7.img && echo OK
```

Expected: `OK` — leaves the working tree built for the default Pi 2 target, matching the state before this task started.

- [ ] **Step 7: Commit**

```bash
git add libs/genesis.mk
git commit -m "$(cat <<'EOF'
build(genesis): parameterize CFLAGS by RASPPI for Pi 2/3/4

Branches the emulator core's -mcpu/-mfpu flags on RASPPI, matching
Circle's own Rules.mk ARCHCPU cases, so `make RASPPI=3` / `make
RASPPI=4` build with cortex-a53 / cortex-a72 flags instead of always
cortex-a7. Guards RASPPI=5 and AARCH=64 with a clear error since Pi 5
needs a separate AArch64 CFLAGS rework.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Document board selection in the README

**Files:**
- Modify: `README.md:33-72` (Hardware requirements + Compile sections)

**Interfaces:**
- None (documentation only).

- [ ] **Step 1: Update the Hardware requirements section**

In `README.md`, replace:

```markdown
## Hardware requirements

- **Raspberry Pi 2** (Model B, ARMv7 / `kernel7.img`).
```

with:

```markdown
## Hardware requirements

- **Raspberry Pi 2** (Model B, ARMv7 / `kernel7.img`) by default. Pi 3
  (`kernel8-32.img`) and Pi 4 (`kernel7l.img`) are also supported in
  32-bit mode — see [Building](#building) — but have not yet been
  verified on physical hardware. Pi 5 is not yet supported (it has no
  32-bit target; it needs a separate 64-bit build).
```

- [ ] **Step 2: Update the Compile section**

In `README.md`, replace:

```markdown
### Compile

```sh
make
```

This builds the Circle sub-libraries, the Genesis core (`libgenesis.a`), and
links everything into **`kernel7.img`**. The Makefile is already configured for
`RASPPI=2`, `AARCH=32`, and the `arm-linux-gnueabihf-` prefix.
```

with:

```markdown
### Compile

```sh
make
```

This builds the Circle sub-libraries, the Genesis core (`libgenesis.a`), and
links everything into **`kernel7.img`**. The Makefile defaults to
`RASPPI=2`, `AARCH=32`, and the `arm-linux-gnueabihf-` prefix.

To target a Raspberry Pi 3 or 4 instead (both still 32-bit; Pi 5 is not yet
supported), override `RASPPI` on the command line:

```sh
make RASPPI=3   # Pi 3 -> kernel8-32.img
make RASPPI=4   # Pi 4 -> kernel7l.img
```

Switching boards requires a full clean first, since Circle's own
per-library build artifacts aren't rebuilt automatically when the target
changes:

```sh
make clean-all
make RASPPI=3
```
```

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
docs(readme): document RASPPI=3/4 build support

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
