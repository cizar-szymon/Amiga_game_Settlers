#!/bin/sh
# Run on the PC (Git Bash): copies the emulated Amiga to the Raspberry Pi
# over SSH (scp) and runs install-amiberry.sh there.
#   sh tools/pi/pi_deploy.sh user@address [autostart]
# Copies (none of it goes into the repository):
#   C:\amiga-dev\roms\kicka1200.rom      -> ~/amiga/roms/
#   C:\amiga-dev\winuae\System           -> ~/amiga/System   (OS 3.2, AmiAgent)
#   build/amiga/Settlers as Settlers-LAN, the release drawer, data/ -> ~/amiga/Settlers
set -e
PI=$1
[ -z "$PI" ] && { echo "usage: sh tools/pi/pi_deploy.sh user@address [autostart]"; exit 1; }
cd "$(dirname "$0")/../.."
ssh "$PI" 'mkdir -p ~/amiga/roms ~/amiga/System ~/amiga/Settlers/data ~/amiga/empty_wow ~/amiga/tools'
scp tools/pi/install-amiberry.sh tools/pi/Settlers-Pi400.uae.in "$PI:amiga/tools/"
scp /c/amiga-dev/roms/kicka1200.rom "$PI:amiga/roms/"
scp -r /c/amiga-dev/winuae/System/. "$PI:amiga/System/"
scp releases/amiga/Settlers/Settlers releases/amiga/Settlers/Settlers.info \
    releases/amiga/Settlers/*.txt "$PI:amiga/Settlers/"
scp build/amiga/Settlers "$PI:amiga/Settlers/Settlers-LAN"
scp releases/amiga/Settlers/Settlers.info "$PI:amiga/Settlers/Settlers-LAN.info"
scp build/amiga/lantest "$PI:amiga/Settlers/lantest"
scp -r data/amiga data/start.iff "$PI:amiga/Settlers/data/"
ssh "$PI" "sh ~/amiga/tools/install-amiberry.sh $2"
