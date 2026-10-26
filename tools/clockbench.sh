#!/bin/sh
# Speed of the game at a CPU clock of the Warp (WarpTool CPUFreqHz, takes
# effect at once): the logic (simtest, 6000 steps) and the frames per second
# of scene1 and scene2 at every screen mode, at 800x600 also magnified x2
# and x3. Appends CSV lines to the output file.
# Only when asked: the measurements of the port are at 100 MHz.
# usage: sh tools/clockbench.sh MHZ OUT.csv [config]
MHZ=$1
OUT=$2
CFG=${3:-C:/amiga-dev/amiagent.cfg}
R="python tools/amiga_remote.py --config $CFG"
TMP=${TMPDIR:-/tmp}/clockbench.$$
sh tools/amiga_free.sh "$CFG" || exit 1

$R run "WarpTool CPUFreqHz=${MHZ}000000" >/dev/null
clk=$($R run "WarpTool Diag" | grep -a "clk freq" | sed 's/.*: //; s/\x1b\[[0-9;]*m//g' | tr -d ' ')
echo "clock set: $clk"

# The logic: 6000 steps of mission 0 with the castle (10 minutes of play).
printf 'Stack 1000000\nWorkSD:Settlers/simtest -m 0 -c -n 6000 -k 6000 >RAM:simclk.txt\n' > $TMP
$R put $TMP WorkSD:Settlers/simclk >/dev/null
$R run "Execute WorkSD:Settlers/simclk" >/dev/null
logic=$($R run "Type RAM:simclk.txt" | sed -n 's/^done: 6000 steps in \([0-9]*\) ms.*/\1/p')
echo "$MHZ,logic,6000 steps ms,$logic" >> $OUT
echo "logic: $logic ms"

stats() { sh tools/getstats.sh "$CFG"; }
fps_avg() {
  # three samples a few seconds apart
  s=0; n=0
  for k in 1 2 3; do
    v=$(stats | sed -n 's/^FPS \([0-9]*\)\.\([0-9]\).*/\1\2/p')
    [ -n "$v" ] && { s=$((s + v)); n=$((n + 1)); }
    sleep 3
  done
  [ $n -gt 0 ] && echo "$((s / n / 10)).$((s / n % 10))" || echo "?"
}
gaps() { stats | sed -n 's/.*gaps \([0-9]*\).*/\1/p'; }
wait_gone() {
  $R stop Settlers >/dev/null 2>&1
  for i in 1 2 3 4 5 6 7 8 9 10; do
    sleep 2
    $R run Status | grep -qi settlers || return 0
    $R stop Settlers >/dev/null 2>&1
  done
  echo "game did not quit"; exit 1
}

for scene in scene1 scene2; do
  for mode in 400x300 640x400 640x480 800x600; do
    sh tools/amiga_free.sh "$CFG" || exit 1
    printf 'Stack 1000000\nDelete RAM:Settlers-stats.txt QUIET\nCD WorkSD:Settlers\nWorkSD:Settlers/Settlers LOAD=WorkSD:Settlers/%s.save MODE=%s STATS\n' $scene $mode > $TMP
    $R put $TMP WorkSD:Settlers/run-clk >/dev/null
    $R start "Execute WorkSD:Settlers/run-clk" >/dev/null
    sleep 25
    f=$(fps_avg); g=$(gaps)
    echo "$MHZ,$scene,$mode,$f,gaps $g" >> $OUT
    echo "$scene $mode: $f fps (sound gaps $g)"
    if [ $mode = 800x600 ] && [ $scene = scene2 ]; then
      for z in 2 3; do
        $R key 0x7a --window Settlers >/dev/null
        sleep 6
        f=$(fps_avg)
        echo "$MHZ,$scene,$mode x$z,$f," >> $OUT
        echo "$scene $mode x$z: $f fps"
      done
    fi
    wait_gone
  done
done
rm -f $TMP
