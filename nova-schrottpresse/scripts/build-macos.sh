#!/bin/bash
# Baut NOVA Schrottpresse direkt auf deinem Mac und installiert VST3 + AU in ~/Library/Audio/Plug-Ins.
# Voraussetzungen: Xcode-Befehlszeilenwerkzeuge und CMake.
set -e
cd "$(dirname "$0")/.."

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Die Xcode-Befehlszeilenwerkzeuge fehlen. Das Installationsfenster öffnet sich jetzt."
  echo "Danach dieses Skript noch einmal starten."
  xcode-select --install || true
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake fehlt. Installiere es mit Homebrew:  brew install cmake"
  echo "(oder lade es von https://cmake.org/download/)"
  exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" -DSP_COPY_AFTER_BUILD=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Fertig. Schrottpresse liegt in ~/Library/Audio/Plug-Ins (VST3 und Components)."
echo "Standalone-App: build/Schrottpresse_artefacts/Release/Standalone/Schrottpresse.app"
echo "Klangbeispiele: build/render/*.wav"
