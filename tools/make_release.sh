#!/bin/sh
# The release of the Amiga port (after docker/build.ps1), number from
# port/settlers_version.h. Made again from scratch each time:
#   releases/amiga/Settlers/Settlers       the program and its icon (docs/release/amiga/Settlers.info)
#   releases/amiga/Settlers/doc/           ALL texts and PDFs: descriptions PL/EN, version
#                                          history, Install.txt/.pdf (EN/PL/DE), COPYING,
#                                          micromod licence; the texts with MultiView icons
#   releases/amiga/Settlers/doc.info       the doc drawer's icon
#   releases/amiga/Settlers/data/amiga/    empty, a ReadMe: which files of the disks go there
#   releases/amiga/Settlers/data/intro/    intro.script (the recorded intro of the port,
#                                          build/intro/intro.script) and a ReadMe: the
#                                          intro files of disks 1 and 2
#   releases/amiga/Settlers.info           the game drawer's icon
#   releases/amiga/Settlers-<v>.zip        the same as an archive - no game data (in git)
#   private/Settlers-<v>-full.zip          the same + the game data (data/amiga from
#                                          the original disks) and the intro files of
#                                          disks 1 and 2 (build/intro/files): runs at
#                                          once; only on this PC (private/ is not in git)
# The public tree may hold only the files listed in PUBLIC below (and doc/): the
# script refuses when anything else - a file of the original disks - gets in.
# Older archives: releases/amiga -> out of git, to private/old; private -> private/old.
# The archives unpack on the Amiga with UnZip:  UnZip Settlers-<v>.zip -d Work:Games
#   tools/make_release.sh [--no-full]
set -e
cd "$(dirname "$0")/.."
V=$(sed -n 's/^#define SETTLERS_VERSION "\(.*\)"/\1/p' port/settlers_version.h)
D=docs/release/amiga
R=releases/amiga
G="$R/Settlers"

grep -a -q "\$VER: Settlers $V " build/amiga/Settlers || { echo "build/amiga/Settlers is not $V: docker/build.ps1 first" >&2; exit 1; }
grep -q '^#define SETTLERS_LABEL ""' port/settlers_version.h || echo "warning: SETTLERS_LABEL is set - a test build" >&2
INTRO=build/intro/intro.script	# python tools/intro/make_script.py
[ -s "$INTRO" ] || { echo "$INTRO is missing" >&2; exit 1; }

# the public tree, made again
rm -rf "$G"
mkdir -p "$G/doc" "$G/data/amiga" "$G/data/intro"
cp build/amiga/Settlers "$G/Settlers"
cp "$D/Settlers.info" "$G/Settlers.info"	# the game's icon (Tool Types, stack), at 8,4
cp "$D/data_ReadMe.txt" "$G/data/amiga/ReadMe.txt"
cp "$D/data_intro_ReadMe.txt" "$G/data/intro/ReadMe.txt"
cp "$INTRO" "$G/data/intro/intro.script"
# The translations (Options -> Advanced -> Language): Polish, German.
mkdir -p "$G/po"
cp port/po/pl.po port/po/de.po "$G/po/"
python tools/make_install.py >/dev/null	# Install.txt and Install.pdf (EN, PL, DE) from Install_src.txt
for f in Settlers.txt Settlers_en.txt Settlers_dev.txt Settlers_dev_en.txt \
         Settlers_release.txt Settlers_release_en.txt Install.txt Install.pdf; do
	cp "$D/$f" "$G/doc/$f"
done
cp COPYING "$G/doc/COPYING"				# GPL-3.0, goes with the program
cp port/thirdparty/micromod/licence.txt "$G/doc/micromod-licence.txt"
# the drawers (a plain one, tools/icon/drawer.png): the game's and doc next to the game's icon
cp tools/icon/warp-drawer.info "$R/Settlers.info"	# the game drawer: the WARP logo (tools/icon/warp-drawer.info, ../WARP-drawer.info)
python tools/make_doc_icon.py "$G/doc.info" --frame tools/icon/drawer.png --crop 0 0 46 40 --transparent --drawer --window 430 330 --pos 170 12 >/dev/null
# the texts with icons opened by MultiView (file:picture:x:y)
for t in Install.txt:doc_i:72:10 Settlers.txt:doc_pl:72:80 Settlers_en.txt:doc_en:282:80 \
         Settlers_release.txt:doc_pl:72:150 Settlers_release_en.txt:doc_en:282:150 \
         Settlers_dev.txt:doc_pl:72:220 Settlers_dev_en.txt:doc_en:282:220; do
	IFS=: read -r f pic x y <<EOT
$t
EOT
	python tools/make_doc_icon.py "$G/doc/$f.info" --frame "tools/icon/$pic.png" --crop 0 0 36 44 --transparent --project SYS:Utilities/MultiView --pos "$x" "$y" >/dev/null
done

# only these files in the public tree - no data of the original disks
PUBLIC=" Settlers/Settlers Settlers/Settlers.info Settlers/doc.info Settlers/data/amiga/ReadMe.txt Settlers/data/intro/ReadMe.txt Settlers/data/intro/intro.script Settlers/po/pl.po Settlers/po/de.po "
bad=$(cd "$R" && find Settlers -type f | while read -r f; do
	case "$PUBLIC" in *" $f "*) continue ;; esac
	case "$f" in Settlers/doc/*) continue ;; esac
	echo "$f"
done)
# and none named like a file of the disks, wherever it is
data=$(cd "$R" && find Settlers -type f | grep -E '/(gfx[a-z]*|sounds|music|extro|FHRS|GA[0-9A-Z][0-9]|KMSK|KPIC|MUSI|PI[BC][0-9]|SML[0-9]|SMP[0-9]|SPR[0-9]|US0[0-9])$' || true)
if [ -n "$bad$data" ]; then
	echo "refused: not for the public archive:" $bad $data >&2
	exit 1
fi

# the archives: older public ones out of git into private/old (kept)
mkdir -p private/old
for f in "$R"/Settlers-*.zip "$R"/Settlers-*.lha; do
	[ -e "$f" ] && [ "$f" != "$R/Settlers-$V.zip" ] && { git rm -q --cached "$f" 2>/dev/null || true; mv "$f" private/old/; }
done
python tools/make_zip.py "$R/Settlers-$V.zip" "$R" Settlers Settlers.info
if [ "$1" != "--no-full" ]; then
	mkdir -p private/old
	# only the current full archive in private/, the older ones in private/old (kept)
	for f in private/Settlers-*-full.zip; do
		[ -e "$f" ] && [ "$f" != "private/Settlers-$V-full.zip" ] && mv "$f" private/old/
	done
	# the intro files of the disks (build/intro/files without intro.script, which
	# is in the tree already)
	ID=build/intro/disk-files
	rm -rf "$ID"
	mkdir -p "$ID"
	for f in build/intro/files/*; do
		[ "$(basename "$f")" = intro.script ] || cp "$f" "$ID/"
	done
	[ "$(ls "$ID" | wc -l)" -eq 30 ] || { echo "build/intro/files: not the 30 files of the intro (tools/intro/make_intro_dir.py)" >&2; exit 1; }
	cmp -s build/intro/files/intro.script "$INTRO" || { echo "build/intro/files/intro.script differs from $INTRO" >&2; exit 1; }
	python tools/make_zip.py "private/Settlers-$V-full.zip" "$R" Settlers Settlers.info \
		--add data/amiga "Settlers/data/amiga" --add "$ID" "Settlers/data/intro"
fi
ls -l "$R/Settlers-$V.zip" private/Settlers-$V-full.zip 2>/dev/null
