#!/bin/sh
# Uploads build/amiga/Settlers.s and measures it on the Amiga in the scene
# WorkSD:Games/Settlers/scene1.save, once per start script given (run-fps: only
# STATS, run-scene: STATS=FULL probes, run-verify: STATS=FULL VERIFY).
# The game runs from the start; do not press P (it pauses).
# usage: sh tools/measure.sh [config] [wait-seconds] [scripts...]
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
WAIT=${2:-25}
shift 2 2>/dev/null
SCRIPTS=${*:-run-fps run-scene run-verify}
R="python tools/amiga_remote.py --config $CFG"

$R stop Settlers >/dev/null 2>&1
sleep 3
$R put build/amiga/Settlers.s WorkSD:Games/Settlers/Settlers >/dev/null || exit 1
$R run "Version WorkSD:Games/Settlers/Settlers" | head -1
for sc in $SCRIPTS; do
  $R start "Execute WorkSD:Games/Settlers/$sc" >/dev/null
  sleep "$WAIT"
  echo "== $sc"
  sh tools/getstats.sh "$CFG"
  $R stop Settlers >/dev/null 2>&1
  sleep 3
done
