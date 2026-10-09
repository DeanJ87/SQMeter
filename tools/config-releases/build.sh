#!/bin/bash
# Writes test/fixtures/config-releases/<tag>.json: the settings each tagged
# release stores for the same set-up device, made by building that release's
# own config code on this computer (stubs replace the ESP32-only headers).
# test/test_config_migration loads every one with the current firmware.
#
# Usage, after a release: tools/config-releases/build.sh v0.2.0
# Needs `pio pkg install -e native` (for ArduinoJson) and a C++17 compiler.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=$(git rev-parse --show-toplevel)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ARDUINOJSON="$ROOT/.pio/libdeps/native/ArduinoJson"
USER_JSON='{"deviceName":"Observatory","wifi":{"ssid":"HomeNet","password":"secret-wifi","hostname":"roof-sqm"},"mqtt":{"enabled":true,"broker":"192.168.1.10","port":1883,"username":"mq","password":"mqpass","topic":"obs/sqm"},"ntp":{"timezone":"GMT0BST,M3.5.0/1,M10.5.0"},"rain":{"enabled":true},"alpaca":{"enabled":true,"cloudCoverUnsafePercent":70,"sqmMinSafe":18.5,"safeDelaySeconds":600},"cloudDetection":{"clearSkyThreshold":-16,"cloudyThreshold":-6,"humidityCorrection":0.9},"skyCalibration":{"sqmOffset":0.3},"location":{"set":true,"latitude":51.48,"longitude":-0.01},"auth":{"enabled":true,"username":"admin","password":"adminpass"}}'

for tag in "$@"; do
  src="$WORK/$tag"
  mkdir -p "$src"
  git -C "$ROOT" ls-tree -r --name-only "$tag" \
    | grep -E '^(include|lib/[A-Za-z]+/include)/.*\.h$|^src/Config\.cpp$|^lib/(ConfigModel|BleLogic)/src/.*\.cpp$' \
    | while read -r f; do
        mkdir -p "$src/$(dirname "$f")"
        git -C "$ROOT" show "$tag:$f" > "$src/$f"
      done
  includes=()
  while read -r d; do includes+=("-I$d"); done < <(find "$src" -type d -name include)
  sources=()
  while read -r f; do sources+=("$f"); done < <(find "$src" -name 'Config*.cpp' -o -name 'Ble*.cpp')
  c++ -std=c++17 -w -Istubs "${includes[@]}" -I"$ARDUINOJSON/src" -I"$ARDUINOJSON" dump.cpp "${sources[@]}" -o "$WORK/dump"
  "$WORK/dump" "$USER_JSON" | python3 -c 'import json,sys; json.dump(json.load(sys.stdin), sys.stdout, indent=2)' \
    > "$ROOT/test/fixtures/config-releases/$tag.json"
  echo "$tag -> test/fixtures/config-releases/$tag.json"
done
