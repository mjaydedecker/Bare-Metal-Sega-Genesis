#!/usr/bin/env bash
#
# tools/mkdist.sh
#
# Bare Metal Sega Genesis
# Build ready-to-copy SD-card zips for Raspberry Pi 2, 3 and 4 (run via
# `make dist`). For each board: `make clean-all` + build, verify the result
# (CPU architecture, kernel size), stage the image with the pinned Raspberry
# Pi firmware, config.txt, README.txt and licences, and zip it into dist/.
#
# Boards are built 3 -> 4 -> 2 so the tree is left as a Pi 2 build. Every
# board needs clean-all first: a plain `make` after another board silently
# links the old board's libraries (e.g. ARMv8 code into a Pi 2 image, which
# boots to the menus but locks up when a game starts).
#
# Functions only run when executed directly, so they can be sourced for tests.
#

set -euo pipefail

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
READELF=arm-linux-gnueabihf-readelf
STAGE=$REPO/build/dist-stage
DIST=$REPO/dist
FW_CACHE_ROOT=$REPO/build/firmware
BOOT_DIR=$REPO/libs/circle/boot
LOAD_ADDR=$((0x8000))      # AArch32 kernel load address

BOARDS=(3 4 2)

die()  { echo "mkdist: ERROR: $*" >&2; exit 1; }
note() { echo "mkdist: $*"; }

target_for() {   # kernel image basename per board
    case $1 in
        2) echo kernel7 ;;
        3) echo kernel8-32 ;;
        4) echo kernel7l ;;
        *) die "unsupported board: Pi $1" ;;
    esac
}

arch_for() {     # expected ELF Tag_CPU_arch of the linked kernel
    case $1 in
        2) echo v7 ;;
        3|4) echo v8 ;;
        *) die "unsupported board: Pi $1" ;;
    esac
}

firmware_for() { # firmware files a board boots with
    case $1 in
        2|3) echo bootcode.bin start.elf fixup.dat ;;
        4)   echo start4.elf fixup4.dat bcm2711-rpi-4-b.dtb ;;
        *) die "unsupported board: Pi $1" ;;
    esac
}

# Output is captured before matching: piping readelf into grep -q / awk exit
# would SIGPIPE readelf and trip pipefail.
elf_arch() {
    local out
    out=$($READELF -A "$1" 2>/dev/null || true)
    sed -n 's/^ *Tag_CPU_arch: *//p' <<<"$out" | head -n 1
}

has_v8() {
    local out
    out=$($READELF -A "$1" 2>/dev/null || true)
    [[ $out == *"Tag_CPU_arch: v8"* ]]
}

kernel_max_size() {
    local v
    v=$(sed -n 's/^DEFINE += -DKERNEL_MAX_SIZE=\(0x[0-9a-fA-F]*\).*/\1/p' "$REPO/Makefile" | head -n 1)
    [[ -n $v ]] || die "KERNEL_MAX_SIZE not found in Makefile"
    echo $((v))
}

# Check the freshly built image for board $1 before the next clean-all wipes it.
verify_board() {
    local n=$1 t want got
    t=$(target_for "$n")
    want=$(arch_for "$n")
    local elf=$REPO/$t.elf img=$REPO/$t.img map=$REPO/$t.map

    [[ -s $img && -s $elf && -s $map ]] || die "Pi $n: $t.img/.elf/.map missing"

    got=$(elf_arch "$elf")
    [[ $got == "$want" ]] || die "Pi $n: $t.elf is CPU arch '$got', expected '$want'"

    if [[ $n == 2 ]]; then
        # Any ARMv8 code crashes the Pi 2's Cortex-A7.
        local f bad=()
        while IFS= read -r f; do
            has_v8 "$f" && bad+=("${f#"$REPO"/}")
        done < <(find "$REPO/libs" -name '*.a'; find "$REPO/src" -name '*.o')
        (( ${#bad[@]} == 0 )) || die "Pi 2: ARMv8 code in: ${bad[*]}"
    fi

    local end max
    end=$(sed -n 's/^ *\(0x[0-9a-fA-F]*\) *_end = \..*/\1/p' "$map" | head -n 1)
    [[ -n $end ]] || die "Pi $n: _end not found in $t.map"
    max=$(kernel_max_size)
    (( end - LOAD_ADDR <= max )) ||
        die "Pi $n: kernel footprint $(printf 0x%x $((end - LOAD_ADDR))) exceeds KERNEL_MAX_SIZE $(printf 0x%x "$max")"

    note "Pi $n: $t.img OK (arch $got, _end $end)"
}

build_board() {
    local n=$1 log=$STAGE/build-pi$1.log
    note "Pi $n: clean-all + build (log: ${log#"$REPO"/})"
    if ! { make -C "$REPO" clean-all && make -C "$REPO" -j"$(nproc)" RASPPI="$n"; } >"$log" 2>&1; then
        tail -n 20 "$log" >&2
        die "Pi $n: build failed"
    fi
}

# Download the firmware revision Circle pins (FIRMWARE/BASEURL in
# libs/circle/boot/Makefile) once into build/firmware/<rev>/ — outside
# libs/circle, so clean-all doesn't wipe it. Only the files we ship are
# fetched, each download is checked (Circle's own loop ignores wget failures),
# and the cache is filled atomically. Echoes the cache dir.
fetch_firmware() {
    local mk=$BOOT_DIR/Makefile rev base dir f
    rev=$(sed -n 's/^FIRMWARE ?= *\([0-9a-f]*\).*/\1/p' "$mk" | head -n 1)
    base=$(sed -n 's/^BASEURL *= *//p' "$mk" | head -n 1)
    [[ -n $rev && -n $base ]] || die "FIRMWARE/BASEURL not found in libs/circle/boot/Makefile"
    base=${base//'$(FIRMWARE)'/$rev}
    dir=$FW_CACHE_ROOT/$rev

    local need=(LICENCE.broadcom COPYING.linux bootcode.bin start.elf fixup.dat
                start4.elf fixup4.dat bcm2711-rpi-4-b.dtb)
    local missing=0
    for f in "${need[@]}"; do [[ -s $dir/$f ]] || missing=1; done

    if (( missing )); then
        note "downloading Raspberry Pi firmware from $base" >&2
        local tmp=$dir.partial
        rm -rf "$tmp"
        mkdir -p "$tmp"
        for f in "${need[@]}"; do
            wget -q -O "$tmp/$f" "$base/$f" || { rm -rf "$tmp"; die "download failed: $base/$f"; }
            [[ -s $tmp/$f ]] || { rm -rf "$tmp"; die "downloaded $f is empty"; }
        done
        for f in start.elf start4.elf; do
            [[ $(head -c 4 "$tmp/$f" | od -An -c | tr -d ' ') == '177ELF' ]] ||
                { rm -rf "$tmp"; die "firmware $f is not an ELF (bad download?)"; }
        done
        rm -rf "$dir"
        mv "$tmp" "$dir"
    fi
    echo "$dir"
}

write_config() {   # $1 board, $2 kernel image, $3 output path
    {
        if [[ $1 != 2 ]]; then
            echo "#"
            echo "# Packaged for Raspberry Pi $1 by tools/mkdist.sh. The notes below were"
            echo "# written for the Pi 2 build and apply unchanged; the kernel= line at the"
            echo "# end selects this board's image ($2)."
        fi
        cat "$REPO/boot/config.txt"
        if [[ $1 != 2 ]]; then
            echo
            echo "# Raspberry Pi $1 image."
            echo "kernel=$2"
        fi
    } >"$3"
}

write_readme() {   # $1 board, $2 kernel image, $3 version, $4 output path
    local origin
    origin=$(git -C "$REPO" remote get-url origin 2>/dev/null || echo "(no origin remote)")
    cat >"$4" <<EOF
Bare Metal Sega Genesis - Raspberry Pi $1 SD card
=================================================

Version:  $3
Built:    $(date -u +%Y-%m-%d) (UTC)
Kernel:   $2
Source:   $origin
Tested on Raspberry Pi $1 hardware.

SETUP
1. Format a microSD card as FAT32.
2. Copy EVERYTHING in this zip to the root of the card (keep the roms/ folder).
3. Put your Genesis / Mega Drive ROMs (.bin .md .gen) in roms/ (subfolders
   are fine). ROMs are not included - use your own legally-obtained dumps.
4. Insert the card, connect HDMI and a USB or Sega controller, and power on.

settings.txt, controllers.txt and saves/ are created on the card at runtime.

CONTROLS
- Menus: D-pad to move, START to confirm, B to go back.
- In game: Start+Select opens the pause menu (changeable in Settings).
- Settings > Test Controllers shows live input from every controller.

LICENCES
See licenses/. Genesis Plus GX may not be sold or used commercially.
Full source code: $origin
EOF
}

make_zip() {       # $1 output zip, $2 staged directory
    python3 - "$1" "$2" <<'PY'
import os, sys, zipfile
out, root = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for d, dirs, files in os.walk(root):
        dirs.sort()
        rel = os.path.relpath(d, root)
        if rel != "." and not dirs and not files:          # keep empty roms/
            info = zipfile.ZipInfo(rel.replace(os.sep, "/") + "/")
            info.external_attr = (0o40775 << 16) | 0x10
            z.writestr(info, "")
        for f in sorted(files):
            p = os.path.join(d, f)
            z.write(p, os.path.relpath(p, root).replace(os.sep, "/"))
with zipfile.ZipFile(out) as z:
    bad = z.testzip()
    if bad:
        sys.exit("corrupt entry: " + bad)
PY
}

stage_board() {    # $1 board, $2 firmware dir, $3 version
    local n=$1 t dir f
    t=$(target_for "$n")
    dir=$STAGE/pi$n
    mkdir -p "$dir/roms" "$dir/licenses"

    cp "$STAGE/images/$t.img" "$dir/"
    for f in $(firmware_for "$n"); do cp "$2/$f" "$dir/"; done
    write_config "$n" "$t.img" "$dir/config.txt"
    write_readme "$n" "$t.img" "$3" "$dir/README.txt"

    cp "$2/LICENCE.broadcom"                          "$dir/licenses/LICENCE.broadcom"
    cp "$REPO/libs/genesis-plus-gx-wide/LICENSE.txt"  "$dir/licenses/Genesis-Plus-GX-LICENSE.txt"
    cp "$REPO/libs/circle/LICENSE"                    "$dir/licenses/Circle-LICENSE.txt"
    cp "$REPO/tools/fonts/OFL-PressStart2P.txt"       "$dir/licenses/"
    cp "$REPO/tools/fonts/OFL-VT323.txt"              "$dir/licenses/"
    [[ $n == 4 ]] && cp "$2/COPYING.linux"            "$dir/licenses/COPYING.linux"   # .dtb

    mkdir -p "$DIST"
    local zip=$DIST/bare-metal-genesis-pi$n-$3.zip
    rm -f "$zip"
    make_zip "$zip" "$dir"
    note "wrote ${zip#"$REPO"/} ($(du -h "$zip" | cut -f1))"
}

main() {
    local t
    for t in "$READELF" make python3 wget git; do
        command -v "$t" >/dev/null || die "required tool not found: $t"
    done

    local version
    version=$(git -C "$REPO" describe --always --dirty)
    [[ $version == *-dirty ]] && note "WARNING: working tree has uncommitted changes ($version)"

    rm -rf "$STAGE"
    mkdir -p "$STAGE/images"

    local n
    for n in "${BOARDS[@]}"; do
        build_board "$n"
        verify_board "$n"
        cp "$REPO/$(target_for "$n").img" "$STAGE/images/"
    done

    local fw
    fw=$(fetch_firmware)

    for n in 2 3 4; do
        stage_board "$n" "$fw" "$version"
    done

    note "done: $(ls "$DIST"/*-"$version".zip | sed "s|$REPO/||" | tr '\n' ' ')"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
