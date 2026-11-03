#!/bin/sh
# The magnified map view (Z): the old way of sending it to the card (the
# part of a row from the first to the last change, ZOOMGAP=100000) and the
# new one (only the changed pieces, ZOOMGAP=16), 0.104. For each mode the
# game loads a save, then FPS at x1, x2, x3 (two samples each).
# (0.104 needed WIDEZOOM for the zoom on 1280x720; since 0.105 always on.)
#   [MODES="800x600 1280x720"] [GAPS="100000 16"] [EXTRA=VERIFY]
#   sh tools/zoombench.sh [config] [save on the Amiga]
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
SAVE=${2:-WorkSD:Games/Settlers/freeserf/saves/aaazii.save}
R="python tools/amiga_remote.py --config $CFG"
TMP=$(mktemp)

stats() { sh tools/getstats.sh "$CFG" | head -1; }
sample() {
  sleep 10
  echo "  $1: $(stats)"
  sleep 4
  echo "  $1: $(stats)"
}
quit_game() {
  $R stop Settlers >/dev/null 2>&1
  for i in 1 2 3 4 5 6 7 8 9 10; do
    sleep 2
    $R run Status | grep -qi settlers || return 0
    $R stop Settlers >/dev/null 2>&1
  done
  echo "game did not quit"; exit 1
}

$R run "Copy \"$SAVE\" RAM:zoom.save" >/dev/null || exit 1
for mode in ${MODES:-800x600 1280x720}; do
  for gap in ${GAPS:-100000 16}; do
    sh tools/amiga_free.sh "$CFG" || exit 1
    printf 'Stack 1000000\nDelete RAM:Settlers-stats.txt QUIET\nCD WorkSD:Games/Settlers\nWorkSD:Games/Settlers/Settlers LOAD=RAM:zoom.save MODE=%s STATS ZOOMGAP=%s %s\n' $mode $gap "$EXTRA" > $TMP
    $R put $TMP RAM:run-zoom >/dev/null
    $R start "Execute RAM:run-zoom" >/dev/null
    sleep 15
    echo "$mode ZOOMGAP=$gap $EXTRA"
    sample x1
    $R key z --window Settlers >/dev/null
    sample x2
    $R key z --window Settlers >/dev/null
    sample x3
    quit_game
  done
done
$R run "Delete RAM:zoom.save RAM:run-zoom RAM:Settlers-stats.txt QUIET" >/dev/null
rm -f $TMP
