#!/bin/sh
# Puts the whole Settlers setup into WorkSD:Games/Settlers on another Amiga: the
# release drawer, the start picture, the test programs, the run scripts and
# the measuring scene of the user (scene2). scene1 is made there by mkscene.
# The game data (data/amiga) must already be there.
# usage: sh tools/install1.sh [amiagent config] [scene2.save]
CFG=${1:-C:/amiga-dev/amiagent.cfg}
SCENE2=$2
R="python tools/amiga_remote.py --config $CFG"
D=WorkSD:Games/Settlers
set -e
$R run "MakeDir $D/data/amiga ALL" >/dev/null || true
for f in Settlers Settlers.info Settlers.txt Settlers_en.txt Settlers_dev.txt \
         Settlers_dev_en.txt Settlers_release.txt Settlers_release_en.txt COPYING; do
  $R put releases/amiga/Settlers/$f $D/$f
done
$R put releases/amiga/Settlers/data/amiga/ReadMe.txt $D/data/amiga/ReadMe.txt
[ -f data/start.iff ] && $R put data/start.iff $D/data/start.iff
for f in simtest loadtest loadtest-fm Settlers-hg datacheck modelist; do
  $R put build/amiga/$f.s $D/$f
done
for f in tools/amiga/*; do
  case $f in *README.txt) ;; *) $R put $f $D/$(basename $f) ;; esac
done
[ -n "$SCENE2" ] && $R put "$SCENE2" $D/scene2.save
$R run "Version $D/Settlers"
