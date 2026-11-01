#!/usr/bin/env bash
# Install user systemd unit for lab controller autostart.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT/deploy/usb-analyzer-controller.service"
UNIT_DST="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/usb-analyzer-controller.service"
mkdir -p "$(dirname "$UNIT_DST")" "$ROOT/logs"
# Stop ad-hoc runner so ports are free for systemd
"$ROOT/tools/run_lab_controller.sh" stop 2>/dev/null || true
pkill -f 'python3 -m controller.run' 2>/dev/null || true
sleep 0.5
# Rewrite WorkingDirectory/Environment to this checkout
sed "s|%h/projects/usb-analyzer|$ROOT|g" "$UNIT_SRC" >"$UNIT_DST"
systemctl --user daemon-reload
systemctl --user enable --now usb-analyzer-controller.service
sleep 0.8
systemctl --user --no-pager --lines=20 status usb-analyzer-controller.service || true
curl -sS -m 3 "http://127.0.0.1:8080/api/v1/health" || true
echo
echo "OK — enabled user service usb-analyzer-controller"
echo "UI: http://$(hostname -I | awk '{print $1}'):8080/"
