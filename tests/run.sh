#!/bin/sh
# Plays songs through Godot (a real AudioStreamPlayer, the mixer, the master
# bus) and checks what comes out against the taudplay CLI, sample for sample.
#
#   GODOT=/path/to/godot sh tests/run.sh [SONG.taud ...] [-t SECONDS]
#
# Build the extension first (scons). Default song: demo/slumberjack.taud.
# Each song runs in real time, for SECONDS (default 10) or until it ends.
set -e
cd "$(dirname "$0")/.."
GODOT=${GODOT:-godot}
SECONDS_=10
SONGS=""
while [ $# -gt 0 ]; do
    case "$1" in
        -t) SECONDS_=$2; shift 2 ;;
        *) SONGS="$SONGS $1"; shift ;;
    esac
done
[ -n "$SONGS" ] || SONGS=demo/slumberjack.taud

OUT=build/tests
mkdir -p "$OUT"
TAUD=thirdparty/taudplay-c
if [ ! -x "$OUT/taudplay" ] || [ "$TAUD/src/player.c" -nt "$OUT/taudplay" ]; then
    ${CC:-cc} -O2 -std=c99 -ffp-contract=off -I$TAUD/include -o "$OUT/taudplay" $TAUD/tools/taudplay.c $TAUD/src/*.c -lm
fi
# First open registers the extension with the test project.
[ -f tests/godot/.godot/extension_list.cfg ] || "$GODOT" --headless --editor --quit-after 60 --path tests/godot >/dev/null 2>&1 || true

status=0
for song in $SONGS; do
    name=$(basename "$song" .taud)
    abs=$(cd "$(dirname "$song")" && pwd)/$(basename "$song")
    echo "== $name"
    "$OUT/taudplay" "$song" -r 48000 -t "$SECONDS_" --raw "$OUT/$name.cli.f32" --interrupts >"$OUT/$name.cli.log" 2>&1 || true
    "$GODOT" --headless --path tests/godot -- "$abs" "$(pwd)/$OUT/$name.godot.f32" "$SECONDS_" >/dev/null 2>&1
    python3 tests/compare.py "$OUT/$name.godot.f32" "$OUT/$name.cli.f32" || status=1
    heard=$(grep -c '^int' "$OUT/$name.godot.f32.log" || true)
    fired=$(grep -c 'Int[0-9A-F]' "$OUT/$name.cli.log" || true)
    echo "interrupts: $heard heard, $fired fired$(grep '^finished' "$OUT/$name.godot.f32.log" | sed 's/^finished.*/; finished/')"
done
exit $status
