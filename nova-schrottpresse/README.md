# NOVA Schrottpresse

Ein Multiband-Zerstörer für Schranz und harten Techno, bedient wie ein großer Verzerrer mit XY-Pads.
Als **VST3**, **Audio Unit** und **Standalone-App** für macOS (Apple Silicon und Intel).

---

## Schnellstart: GitHub baut das Plugin für dich

Du brauchst dafür nichts zu installieren. GitHub kompiliert auf einem Mac in der Cloud.

**1. Repository anlegen.** Auf github.com: oben rechts **+** > *New repository*, Name zum Beispiel `schrottpresse`, privat geht auch. Ohne README anlegen.

**2. Ordner hochladen.** Im Terminal (im Schrottpresse-Ordner):

```bash
cd ~/Downloads/Schrottpresse
git init
git add .
git commit -m "NOVA Schrottpresse erste Version"
git branch -M main
git remote add origin https://github.com/DEIN-NAME/schrottpresse.git
git push -u origin main
```

> Lädst du stattdessen über die Webseite hoch, fehlt oft der versteckte Ordner `.github`, und dann baut GitHub nichts. Im Finder machst du ihn mit **Cmd + Shift + .** sichtbar. GitHub Desktop nimmt ihn automatisch mit.

**3. Warten (ca. 10 Minuten).** Im Repository auf **Actions** klicken. Der Lauf *macOS-Build* wird grün, sobald alles gebaut und geprüft ist.

**4. Herunterladen.** Den Lauf öffnen, unten bei **Artifacts** auf *NOVA-Schrottpresse-macOS* klicken und zweimal entpacken (GitHub packt die Zip noch einmal ein).

**5. Installieren.** Terminal öffnen und eintippen:

```bash
bash ~/Downloads/Schrottpresse/Installieren.command
```

Das Skript entfernt die Download-Sperre von macOS und kopiert VST3 und AU nach `~/Library/Audio/Plug-Ins`.

**6. In Ableton testen.** *Einstellungen > Plug-ins*: VST3-Systemordner und Audio Units einschalten, *Neu scannen*. Die Schrottpresse liegt im Browser unter *Plug-ins > NOVA*. Auf eine Kick, einen Drum-Bus oder einen Loop ziehen.

Eine feste Version mit einzelner Zip-Datei bekommst du mit `git tag v0.1.0` und `git push --tags`.

---

## Selbst auf dem Mac bauen

Voraussetzungen: Xcode-Befehlszeilenwerkzeuge (`xcode-select --install`) und CMake (`brew install cmake`).

```bash
bash scripts/build-macos.sh
```

---

## Bedienung

| Bereich | Was es tut |
|---|---|
| **Multiband** | 1 bis 3 Bänder. Trennfrequenzen im Spektrum ziehen, Band antippen zum Bearbeiten. Jedes Band hat eigene Einstellungen, Solo (S) und Stumm (M). |
| **Presse** | Vier Verzerrer in den Ecken des XY-Pads, der Punkt mischt sie. Ecken-Namen antippen zum Tauschen (25 Typen in 7 Kategorien). Drive, Tilt vor der Verzerrung, Mix, Stil pro Ecke. Die Kennlinie im Pad zeigt live, was passiert. |
| **Ziel** | Setzt einen zweiten Punkt. Die Hüllkurve schiebt den Mix dorthin, „Hüllk. → Ziel“ bestimmt wie weit. |
| **Faltung** | Vier Räume, Kisten oder Lautsprecher im XY-Pad (16 Impulsantworten). Mix, Stereobreite, Vorverzögerung. |
| **⇄** | Reihenfolge von Presse und Faltung pro Band tauschen. |
| **Hüllkurve** | Folgt dem Eingang und bewegt Drive, Ziel und den Cutoff des Kreisch-Filters. Bei Kicks pumpt das im Takt. |
| **Kreisch-Filter** | Tief-, Band- oder Hochpass mit sehr viel Resonanz und eigener Sättigung. |
| **Ausgang** | Eingang, Mix, Ausgang, Auto-Gain (hält die Lautstärke beim Drehen gleich), Limiter. |
| **Original** | Schaltet zum Vergleich kurz alles ab. |
| **Würfel** | Oben: zufälliges Preset. In Presse und Faltung: vier zufällige Verzerrer oder Räume. |

**31 Presets** in den Kategorien Kick, Rumble & Bass, Loops & Drums, Perc & Hats, Bus & Master, Krach & FX und Basis. Sie stehen auch im Preset-Menü der DAW. Alle Regler lassen sich automatisieren und werden mit dem Song gespeichert.

Die Verzerrung läuft mit 2-fachem Oversampling. Die Latenz von 49 Samples meldet das Plugin der DAW, die gleicht sie automatisch aus.

---

## Testen

| Was | Wie |
|---|---|
| Ohne DAW | `Schrottpresse.app` starten, unter *Options > Audio/MIDI Settings* Ein- und Ausgang wählen |
| Audio Unit | `auval -v aufx Schr Nova`, am Ende muss **AU VALIDATION SUCCEEDED** stehen |
| Selbsttest | `build/SchrottTest_artefacts/Release/SchrottTest build/render` prüft Latenzausgleich, alle Presets und Rechenzeit und schreibt Klangbeispiele |

GitHub führt Selbsttest, `auval` und [pluginval](https://github.com/Tracktion/pluginval) bei jedem Push automatisch aus.

---

## Aufbau

```
Schrottpresse/
├── CMakeLists.txt            Build-Beschreibung (lädt JUCE automatisch)
├── Source/
│   ├── dsp/
│   │   ├── Shapes.h          die 25 Verzerrer-Kennlinien
│   │   ├── IRs.h             die 16 Impulsantworten, rechnerisch erzeugt
│   │   ├── Params.h          alle 112 Parameter
│   │   └── Presets.h         die 31 Presets
│   ├── Engine.*              Multiband, Oversampling, Faltung, Hüllkurve, Filter, Ausgang
│   ├── PluginProcessor.*     Verbindung zur DAW
│   └── PluginEditor.*        Oberfläche
├── fonts/                    Schriften (SIL Open Font License)
├── tests/schrott_test.cpp    Selbsttest
├── scripts/                  Build- und Installationsskripte
├── web/index.html            der Browser-Prototyp
└── .github/workflows/        automatischer Mac-Build bei GitHub
```

---

## Gut zu wissen

- **Nicht signiert:** Für die eigene Nutzung reicht die Ad-hoc-Signatur. Zum Weitergeben oder Verkaufen brauchst du ein Apple-Entwicklerkonto (Signieren und Notarisieren).
- **Lizenzen:** Die Schrottpresse nutzt [JUCE](https://juce.com). Für private Projekte und Open Source passt die freie AGPLv3-Lizenz von JUCE. Für einen Verkauf solltest du dir die Bedingungen von JUCE und Steinberg (VST3) genau ansehen. Die Schriften Big Shoulders Stencil und IBM Plex stehen unter der SIL Open Font License (siehe `fonts/`).
- **Eigenständig:** Name, Optik, Verzerrer und Impulsantworten sind eigene Entwicklungen. Die Bedienidee mit XY-Pads ist an bekannte Multiband-Verzerrer angelehnt.
