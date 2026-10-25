#!/bin/sh
# Exits 1 (and says why) when Settlers runs on the Amiga: a CLI process or
# a game screen (a start from the icon is not in Status). Every test that
# starts the game begins with:  sh tools/amiga_free.sh [config] || exit 1
CFG=${1:-C:/amiga-dev/amiagent.cfg}
R="python tools/amiga_remote.py --config $CFG"
if $R run Status | grep -qi settlers; then
  echo "Settlers runs (Shell) - not starting a test"
  exit 1
fi
if $R titles | grep -q "^Settlers |"; then
  echo "a Settlers screen is open (the user's game?) - not starting a test"
  exit 1
fi
exit 0
