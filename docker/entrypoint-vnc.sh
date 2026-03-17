#!/bin/bash

check_ripes() {
  if pgrep -x "Ripes" > /dev/null; then
    return 0
  else
    return 1
  fi
}

echo "Start X-server"
Xvfb :0 -screen 0 1280x800x24 -ac &
sleep 3

echo "Start Fluxbox"
fluxbox &
sleep 2

echo "start VNC-server"
x11vnc -display :0 -forever -nopw -rfbport 5900 -shared &
sleep 3

echo "Start noVNC"
websockify --web /usr/share/novnc 80 localhost:5900 &
sleep 3

echo "VNC on port 5900"
echo "noVNC on port 80"
echo "Work window: http://localhost:6080/vnc.html"

# Check ripes
if ! command -v Ripes > /dev/null; then
  echo "Ripes miss PATH"
  exit 1
fi

echo "Start Ripes"
cd /home/developer
Ripes --platform xcb &

sleep 5

if check_ripes; then
  echo "Ripes success"
else
  echo "FAIL START"
fi

tail -f /dev/null