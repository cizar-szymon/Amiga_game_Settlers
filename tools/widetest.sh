#!/bin/sh
# The wide screen modes on the Amiga (0.99-0.103): for each mode the game loads a
# save, shows the FPS (STATS), a screenshot; and the zoom
# (since 0.103 none on 16:9: Z three times and the wheel up and down must
# leave the mode as it is).
#   [MODES="640x360 ..."] [GAME=SETTLERS:] sh tools/widetest.sh [config] [save] [out dir]
# GAME: the drawer of the game (WorkSD:Settlers, WinUAE SETTLERS:).
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
GAME=${GAME:-WorkSD:Settlers}
case "$GAME" in *:) P=$GAME ;; *) P=$GAME/ ;; esac
SAVE=${2:-${P}freeserf/saves/aaazii.save}
OUT=${3:-.}
R="python tools/amiga_remote.py --config $CFG"
TMP=$(mktemp)

stats() { sh tools/getstats.sh "$CFG" | head -1; }
quit_game() {
  $R stop Settlers >/dev/null 2>&1
  for i in 1 2 3 4 5 6 7 8 9 10; do
    sleep 2
    $R run Status | grep -qi settlers || return 0
    $R stop Settlers >/dev/null 2>&1
  done
  echo "game did not quit"; exit 1
}

$R run "Copy \"$SAVE\" RAM:wide.save" >/dev/null || exit 1
for mode in ${MODES:-640x360 960x540}; do
  sh tools/amiga_free.sh "$CFG" || exit 1
  printf 'Stack 1000000\nDelete RAM:Settlers-stats.txt QUIET\nCD %s\n%sSettlers LOAD=RAM:wide.save MODE=%s STATS FPS\n' "$GAME" "$P" $mode > $TMP
  $R put $TMP RAM:run-wide >/dev/null
  $R start "Execute RAM:run-wide" >/dev/null
  sleep 25
  echo "$mode: $(stats)"
  echo "  shot $($R shot "$OUT/wide-$mode.png")"
  for z in 1 2 3; do
    $R key z --window Settlers >/dev/null
    sleep 8
    echo "$mode after Z #$z: $(stats)"
    echo "  shot $($R shot "$OUT/wide-$mode-z$z.png")"
  done
  for k in 0x7a 0x7b; do
    $R key $k --window Settlers >/dev/null
    sleep 8
    echo "$mode after wheel $k: $(stats)"
    echo "  shot $($R shot "$OUT/wide-$mode-w$k.png")"
  done
  quit_game
done
$R run "Delete RAM:wide.save RAM:run-wide RAM:Settlers-stats.txt QUIET" >/dev/null
rm -f $TMP
