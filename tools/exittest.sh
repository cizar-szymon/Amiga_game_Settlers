#!/bin/sh
# Starts and stops the game N times (Ctrl-C after a few seconds) and stops
# at the first "Software Failure" requester, to reproduce crashes at exit.
# usage: sh tools/exittest.sh [config] [runs] [script] [seconds] [program]
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
RUNS=${2:-10}
SCRIPT=${3:-run-fps}
SECS=${4:-12}
PROG=${5:-Settlers}
R="python tools/amiga_remote.py --config $CFG"
sh tools/amiga_free.sh "$CFG" || exit 1
i=1
while [ $i -le $RUNS ]; do
  $R start "Execute WorkSD:Games/Settlers/$SCRIPT" >/dev/null
  sleep "$SECS"
  $R stop "$PROG" >/dev/null
  sleep 6
  t=$(PYTHONIOENCODING=utf-8 timeout 30 $R titles 2>&1)
  case "$t" in
    *"Software Failure"*) echo "run $i: SOFTWARE FAILURE"; exit 1 ;;
  esac
  # The game's own screen (a Workbench drawer named Settlers is not it).
  if echo "$t" | grep -q "^Settlers |"; then
    echo "run $i: still running after Ctrl-C"; exit 2
  fi
  echo "run $i: ok"
  i=$((i + 1))
done
