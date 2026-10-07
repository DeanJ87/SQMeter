#!/usr/bin/env bash
# Builds ./alpaca-sim from the repo root. Needs ArduinoJson, which
# `pio test -e native` (or `pio pkg install -e native`) fetches.
set -euo pipefail
cd "$(dirname "$0")/../.."
ARDUINOJSON=.pio/libdeps/native/ArduinoJson/src
[ -d "$ARDUINOJSON" ] || pio pkg install -e native
c++ -std=gnu++17 -O1 -Wall -Wextra \
    -Ilib/AlpacaLogic/include -I"$ARDUINOJSON" \
    lib/AlpacaLogic/src/*.cpp tools/alpaca-sim/main.cpp \
    -o alpaca-sim
echo "Built ./alpaca-sim"
