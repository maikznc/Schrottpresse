#!/bin/bash
# Installiert NOVA Schrottpresse (VST3, Audio Unit, Standalone-App) für den aktuellen Benutzer.
# Aufruf im Terminal:  bash Installieren.command
set -e
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"

for f in Schrottpresse.vst3 Schrottpresse.component; do
  if [ ! -d "$f" ]; then
    echo "Fehler: $f fehlt. Starte das Skript im entpackten Schrottpresse-Ordner."
    exit 1
  fi
done

echo "Entferne die Download-Sperre von macOS ..."
xattr -dr com.apple.quarantine Schrottpresse.vst3 Schrottpresse.component Schrottpresse.app 2>/dev/null || true

mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"
rm -rf "$VST3_DIR/Schrottpresse.vst3" "$AU_DIR/Schrottpresse.component"
cp -R Schrottpresse.vst3 "$VST3_DIR/"
cp -R Schrottpresse.component "$AU_DIR/"
echo "VST3        -> $VST3_DIR/Schrottpresse.vst3"
echo "Audio Unit  -> $AU_DIR/Schrottpresse.component"

if [ -d Schrottpresse.app ]; then
  rm -rf "$APP_DIR/Schrottpresse.app"
  cp -R Schrottpresse.app "$APP_DIR/"
  echo "App         -> $APP_DIR/Schrottpresse.app"
fi

killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Prüfe die Audio Unit ..."
if auval -v aufx Schr Nova >/tmp/schrottpresse-auval.txt 2>&1; then
  echo "Audio Unit: in Ordnung."
else
  echo "Audio Unit: auval meldet ein Problem, Details in /tmp/schrottpresse-auval.txt"
fi

echo ""
echo "Fertig. In Ableton: Einstellungen > Plug-ins > 'Neu scannen'."
