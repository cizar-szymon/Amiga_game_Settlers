#!/bin/sh
# Run on the Raspberry Pi (Raspberry Pi OS 64-bit): installs Amiberry from
# its GitHub releases, writes the Settlers config with this user's home and
# optionally starts it with the desktop.
#   sh install-amiberry.sh [autostart]
# The files of the emulated Amiga come from the PC (pi_deploy.sh) into
# ~/amiga: roms/, System/, Settlers/, empty_wow/.
set -e
HOME_DIR=$HOME
CONF_DIR=$HOME_DIR/amiga
mkdir -p "$CONF_DIR/roms" "$CONF_DIR/System" "$CONF_DIR/Settlers" \
         "$CONF_DIR/empty_wow"

if ! command -v amiberry >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y curl ca-certificates unzip
  CODENAME=$(. /etc/os-release && echo "$VERSION_CODENAME")
  ARCH=$(dpkg --print-architecture)
  # The newest release has a zip with a .deb for each Debian and
  # architecture: amiberry-v<ver>-debian-<codename>-<arch>.zip.
  URL=$(curl -fsSL https://api.github.com/repos/BlitterStudio/amiberry/releases/latest |
        grep -o '"browser_download_url": *"[^"]*"' |
        sed 's/.*"\(http[^"]*\)"/\1/' |
        grep "debian-$CODENAME-$ARCH\.zip$" | head -1 || true)
  if [ -z "$URL" ]; then
    echo "No Amiberry for Debian $CODENAME $ARCH - see"
    echo "https://github.com/BlitterStudio/amiberry/releases"
    exit 1
  fi
  echo "Amiberry: $URL"
  rm -rf /tmp/amiberry && mkdir -p /tmp/amiberry
  curl -fL -o /tmp/amiberry/amiberry.zip "$URL"
  unzip -o -q /tmp/amiberry/amiberry.zip -d /tmp/amiberry
  sudo apt-get install -y /tmp/amiberry/*.deb
fi
amiberry --version 2>/dev/null || true

sed "s|@HOME@|$HOME_DIR|g" "$(dirname "$0")/Settlers-Pi400.uae.in" \
  > "$CONF_DIR/Settlers-Pi400.uae"
echo "config: $CONF_DIR/Settlers-Pi400.uae"

if [ "$1" = "autostart" ]; then
  mkdir -p "$HOME_DIR/.config/autostart"
  cat > "$HOME_DIR/.config/autostart/amiberry-settlers.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Amiberry Settlers
Exec=amiberry -G -config=$CONF_DIR/Settlers-Pi400.uae
EOF
  echo "starts with the desktop"
fi
echo "start: amiberry -G -config=$CONF_DIR/Settlers-Pi400.uae"
