#!/usr/bin/env bash
# OTA flash analyzer-node onto a lab ESP that still runs hu-probe (/ota-upload).
#
# Usage:
#   ./tools/flash_lab_ota.sh 192.168.178.88
#   ./tools/flash_lab_ota.sh 192.168.178.89
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IP="${1:-}"
BIN_SRC="${BIN_SRC:-$ROOT/firmware/analyzer-node/.pio/build/analyzer-s3-host/firmware.bin}"
FW_NAME="${FW_NAME:-usb-analyzer.0.1.0.esp32s3.bin}"
DIST="$ROOT/firmware/dist"

die() { echo "Fehler: $*" >&2; exit 1; }
[ -n "$IP" ] || die "IP fehlt (z.B. 192.168.178.88)"
[ -f "$BIN_SRC" ] || die "Bin fehlt: $BIN_SRC — zuerst: cd firmware/analyzer-node && pio run -e analyzer-s3-host"

mkdir -p "$DIST"
cp -f "$BIN_SRC" "$DIST/$FW_NAME"
echo "==> target http://$IP/ota-upload"
echo "    file $DIST/$FW_NAME ($(wc -c <"$DIST/$FW_NAME") bytes)"

code="$(curl -sS -m 3 -o /dev/null -w '%{http_code}' "http://$IP/api/status" || true)"
[ "$code" = "200" ] || die "ESP $IP nicht erreichbar (HTTP $code) — noch hu-probe?"

tmp="$(mktemp)"
set +e
http="$(curl -sS -m 180 -o "$tmp" -w '%{http_code}' \
  -F "firmware=@${DIST}/${FW_NAME};filename=${FW_NAME}" \
  "http://${IP}/ota-upload")"
set -e
echo "    HTTP $http  body=$(head -c 200 "$tmp")"
rm -f "$tmp"
[ "$http" = "200" ] || die "OTA fehlgeschlagen"

echo "    Upload OK — ESP rebootet."
echo "    Hinweis: nach Host-FW kein hu-probe HTTP mehr — warte auf discovery HELLO."
sleep 6
# LXC often lacks CAP_NET_RAW for ping; ARP/neigh is enough.
if ip neigh show "$IP" 2>/dev/null | rg -q 'REACHABLE|DELAY|STALE'; then
  echo "    neigh: $IP visible (check discovery log for HELLO)"
else
  echo "    neigh: $IP not yet in ARP — check discovery / Serial"
fi
exit 0
