#!/bin/sh
# Reboots the Amiga through AmiAgent (C:Reboot) after a crashed test and
# waits until AmiAgent answers again. The user allowed remote reboots.
# usage: sh tools/reboot.sh [amiagent config]
CFG=${1:-C:/amiga-dev/amiagent-2.cfg}
R="python tools/amiga_remote.py --config $CFG"
timeout 20 $R start "C:Reboot" || echo "(no answer to the reboot command)"
sleep 20
n=0
until timeout 15 $R ping 2>/dev/null | grep -q OK; do
  n=$((n + 1))
  if [ $n -gt 30 ]; then
    echo "the Amiga did not come back"
    exit 1
  fi
  sleep 10
done
timeout 15 $R ping
