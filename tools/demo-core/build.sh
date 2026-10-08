#!/usr/bin/env bash
# Builds the SQMeter device core (lib/ + tools/demo-core/bridge.cpp) to
# WebAssembly for the demo: web/src/demo/core/sqm-core.mjs (+ .wasm).
# Needs Emscripten (see VERSION). The output is committed, so web
# contributors don't need it; CI checks SOURCE_HASH is current.
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT=web/src/demo/core
mkdir -p "$OUT"

# ArduinoJson (header-only) from PlatformIO's native env.
AJ=$(find .pio/libdeps/native -maxdepth 3 -type d -path "*ArduinoJson/src" 2>/dev/null | head -1)
if [ -z "$AJ" ]; then
  pio pkg install -e native >/dev/null
  AJ=$(find .pio/libdeps/native -maxdepth 3 -type d -path "*ArduinoJson/src" | head -1)
fi

SOURCES=(tools/demo-core/bridge.cpp
  lib/DeviceCore/src/*.cpp lib/ConfigModel/src/*.cpp lib/SkyLogic/src/*.cpp lib/RainLogic/src/*.cpp
  lib/AlertLogic/src/*.cpp lib/AlpacaLogic/src/*.cpp lib/Readings/src/*.cpp lib/SafetyHistoryLogic/src/*.cpp
  lib/BleLogic/src/*.cpp)
INCLUDES=()
for d in lib/*/include; do INCLUDES+=("-I$d"); done

em++ -std=gnu++17 -O2 -Wall -Wextra \
  "${INCLUDES[@]}" "-I$AJ" \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createSqmCore \
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0 \
  -sDYNAMIC_EXECUTION=0 \
  --bind "${SOURCES[@]}" -o "$OUT/sqm-core.mjs"

# Hash of everything that went in, so CI can tell the output is current.
cat "${SOURCES[@]}" lib/*/include/*.h lib/*/include/*/*.h tools/demo-core/build.sh 2>/dev/null | shasum -a 256 | cut -d' ' -f1 > "$OUT/SOURCE_HASH"
echo "Built $OUT/sqm-core.mjs ($(wc -c < "$OUT/sqm-core.wasm") bytes wasm)"
