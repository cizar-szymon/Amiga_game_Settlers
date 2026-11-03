#!/bin/sh
# The release of the Amiga port (after docker/build.ps1), number from
# port/settlers_version.h:
#   releases/amiga/Settlers/...            the public release, in git: the program,
#                                          icon, descriptions PL/EN, Install.txt/.pdf
#                                          (EN/PL/DE), the texts' icons (MultiView),
#                                          COPYING, micromod licence, empty data/amiga
#                                          with a ReadMe
#   releases/amiga/Settlers.info           the drawer's icon
#   releases/amiga/Settlers-<v>.zip        the same as an archive - no game data
#   private/Settlers-<v>-full.zip          the same + the game data (data/amiga from
#                                          the original disks): runs at once; only on
#                                          this PC (private/ is not in git)
# The archives unpack on the Amiga with UnZip:  UnZip Settlers-<v>.zip -d Work:Games
#   tools/make_release.sh [--no-full]
set -e
cd "$(dirname "$0")/.."
V=$(sed -n 's/^#define SETTLERS_VERSION "\(.*\)"/\1/p' port/settlers_version.h)
D=docs/release/amiga
R=releases/amiga

grep -a -q "\$VER: Settlers $V " build/amiga/Settlers || { echo "build/amiga/Settlers is not $V: docker/build.ps1 first" >&2; exit 1; }
grep -q '^#define SETTLERS_LABEL ""' port/settlers_version.h || echo "warning: SETTLERS_LABEL is set - a test build" >&2

# the public tree (the icon stays: releases/amiga/Settlers/Settlers.info)
mkdir -p "$R/Settlers/data/amiga"
cp build/amiga/Settlers "$R/Settlers/Settlers"
cp "$D/Settlers.txt" "$D/Settlers_en.txt" "$D/Settlers_dev.txt" "$D/Settlers_dev_en.txt" \
   "$D/Settlers_release.txt" "$D/Settlers_release_en.txt" "$R/Settlers/"
python tools/make_install.py >/dev/null	# Install.txt and Install.pdf (EN, PL, DE) from Install_src.txt
cp "$D/Install.txt" "$D/Install.pdf" "$R/Settlers/"
# the drawer (a plain one, tools/icon/drawer.png) and the texts with icons opened by
# MultiView (file:picture:x:y; the game's own icon is at 8,4 - Settlers.info)
python tools/make_doc_icon.py "$R/Settlers.info" --frame tools/icon/drawer.png --crop 0 0 46 40 --transparent --drawer >/dev/null
for t in Install.txt:doc_i:282:12 Settlers.txt:doc_pl:72:100 Settlers_en.txt:doc_en:282:100 \
         Settlers_release.txt:doc_pl:72:170 Settlers_release_en.txt:doc_en:282:170 \
         Settlers_dev.txt:doc_pl:72:240 Settlers_dev_en.txt:doc_en:282:240; do
	IFS=: read -r f pic x y <<EOT
$t
EOT
	python tools/make_doc_icon.py "$R/Settlers/$f.info" --frame "tools/icon/$pic.png" --crop 0 0 36 44 --transparent --project SYS:Utilities/MultiView --pos "$x" "$y" >/dev/null
done
cp COPYING "$R/Settlers/COPYING"				# GPL-3.0, goes with the program
cp port/thirdparty/micromod/licence.txt "$R/Settlers/micromod-licence.txt"

# the archives (older releases' archives removed; they stay in the tags)
for f in "$R"/Settlers-*.zip "$R"/Settlers-*.lha; do
	[ -e "$f" ] && [ "$f" != "$R/Settlers-$V.zip" ] && { git rm -q --cached "$f" 2>/dev/null || true; rm -f "$f"; }
done
python tools/make_zip.py "$R/Settlers-$V.zip" "$R" Settlers Settlers.info
if [ "$1" != "--no-full" ]; then
	mkdir -p private
	python tools/make_zip.py "private/Settlers-$V-full.zip" "$R" Settlers Settlers.info \
		--add data/amiga "Settlers/data/amiga"
fi
ls -l "$R/Settlers-$V.zip" private/Settlers-$V-full.zip 2>/dev/null
