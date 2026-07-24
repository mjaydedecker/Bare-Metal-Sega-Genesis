# Pi 3/4 genesis.mk Parameterization — Design

## Context

The build currently targets Raspberry Pi 2 only: the top-level `Makefile`
hardcodes `AARCH = 32` and `RASPPI = 2`, and forwards `RASPPI`/`AARCH` to
Circle's own build. Circle's `Rules.mk` already supports `RASPPI=3`
(cortex-a53, `kernel8-32.img`) and `RASPPI=4` (cortex-a72, `kernel7l.img`)
in 32-bit mode — no changes needed there.

The blocker is `libs/genesis.mk`, which builds the Genesis-Plus-GX-Wide
emulator core into `libs/libgenesis.a`. Its `CFLAGS` hardcode Pi 2's CPU
flags (`-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard`)
unconditionally — it is the only piece of the build not parameterized by
board.

This spec covers **Pi 3 and Pi 4 in 32-bit (AArch32) mode only**. Pi 5
requires AArch64 (no 32-bit target exists for it in Circle), which means a
separate, larger rework of `genesis.mk`'s CFLAGS (dropping
`-marm`/`-mfpu`/`-mfloat-abi`, verifying 64-bit cleanliness, switching
toolchain prefix). That is explicitly out of scope here and is expected to
be a follow-up piece of work.

## Goals

- `make RASPPI=3` and `make RASPPI=4` (with `AARCH=32`, the default) produce
  working cross-compiled builds of the full kernel image, including the
  emulator core, using CPU flags matched to that board.
- `make` with no arguments continues to build for Pi 2 exactly as today
  (no regression).
- Attempting an unsupported combination (`RASPPI` other than 2/3/4, or
  `AARCH=64`) fails the build loudly with a clear error, rather than
  silently compiling with wrong flags.

## Non-goals

- Pi 5 / AArch64 support (separate follow-up).
- Runtime board detection or a single multi-board binary — one build
  targets one board, matching how the rest of the toolchain already works.
- Namespacing build output by board (see Design decisions).
- Hardware verification on physical Pi 3/4 boards — this task is a
  cross-compile/link verification only (no hardware in hand).

## Design decisions

**Board selection**: via existing `AARCH`/`RASPPI` make variables, made
overridable (`?=` instead of `=`) rather than adding new named targets
(e.g. `make pi3`). This matches how Circle's own `Rules.mk` already exposes
board selection and avoids adding new Makefile surface.

**Build isolation**: switching `RASPPI` reuses the same output paths
(`build/genesis/`, `libs/libgenesis.a`, and Circle's own per-library `.o`
files are not namespaced by board either). This is already true of the
existing build — Circle's libs aren't board-namespaced today — so this
change doesn't newly introduce the sharp edge, it just makes it reachable
by a documented variable rather than a Makefile edit. A `make clean` is
required when switching boards; this will be documented (Makefile comment
+ README note) rather than solved with new output-path plumbing.

**CFLAGS mapping**: copied directly from Circle's `Rules.mk` `ARCHCPU`
cases for AArch32, so `genesis.mk` always matches the flags Circle itself
uses for that board:

| RASPPI | ARCHCPU flags |
|---|---|
| 2 (default) | `-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard` |
| 3 | `-mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard` |
| 4 | `-mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard` |
| other / `AARCH=64` | `$(error ...)` — explicitly unsupported for now |

## Changes

1. **Top-level `Makefile`**
   - `AARCH = 32` → `AARCH ?= 32`
   - `RASPPI = 2` → `RASPPI ?= 2`
   - `libs/libgenesis.a` recipe: forward `RASPPI=$(RASPPI)` (and `AARCH=$(AARCH)`
     for the out-of-scope guard) to the `libs/genesis.mk` invocation, alongside
     the existing `PREFIX=$(PREFIX)`.
   - Short comment noting `make clean` is required when switching `RASPPI`.

2. **`libs/genesis.mk`**
   - Accept `RASPPI ?= 2` and `AARCH ?= 32`.
   - Replace the hardcoded `ARCHCPU` portion of `CFLAGS` with a RASPPI-keyed
     conditional (2/3/4 per the table above), erroring on anything else or
     on `AARCH=64`.

3. **Docs**: brief README note (or Makefile header comment) documenting
   `make RASPPI=3` / `make RASPPI=4` usage and the `make clean` requirement
   when switching boards.

## Verification

No unit tests apply — this is build plumbing, not application logic.
Verification is a compile/link pass, no hardware required:

1. `make clean && make` (default, RASPPI=2) — regression check, confirms
   `kernel7.img` still builds identically.
2. `make clean && make RASPPI=3` — confirms `kernel8-32.img` builds with
   cortex-a53 flags.
3. `make clean && make RASPPI=4` — confirms `kernel7l.img` builds with
   cortex-a72 flags.

Physical Pi 3/4 boot/runtime verification is deferred (no hardware
available) and should be tracked the same way prior board/perf changes in
this project have been (pending-hardware-verify note).
