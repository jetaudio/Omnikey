# Omnikey for Linux

Omnikey for Linux runs as an IBus input method, so it works on Wayland and X11 (GNOME, Ubuntu).
It types Vietnamese, and simplified Chinese Pinyin through librime.

- `ibus-engine-omnikey`: the IBus engine, built on the shared Omnikey engine in `../engine`.
- `omnikey-setup`: the settings app, built with GTK 4 and libadwaita.

Settings are stored in `~/.config/omnikey/omnikey.conf` and macros in `~/.config/omnikey/macros.txt`.
The engine applies changes immediately.

## Build

```bash
sudo apt install build-essential cmake pkg-config libibus-1.0-dev libadwaita-1-dev \
  librime-dev rime-data-pinyin-simp rime-prelude rime-essay
bash scripts/build-linux.sh        # tests, builds, writes dist/ibus-omnikey_<version>_amd64.deb
```

## Install

```bash
sudo apt install ./dist/ibus-omnikey_*_amd64.deb
ibus restart
```

Then add Omnikey in **Settings → Keyboard → Input Sources → + → Vietnamese → Omnikey**.
You can also click **Add Omnikey** in the Omnikey settings app.

## Output modes

- **Underline the current word** (default): the word stays as preedit text until a space or punctuation mark. This works in every app, including terminals.
- **Edit in place**: text is committed as you type and fixed using surrounding text, or BackSpace when the app does not support surrounding text. This is closest to Omnikey on macOS and Windows, but some apps may produce wrong text.

The Ctrl + Shift switch (or Ctrl + Space / Alt + Z) cycles Vietnamese → English → 中 → Vietnamese.
The V / E / 中 symbol in the top bar shows the current mode, and its menu selects one directly.

## Chinese (中)

Keys go to Rime with the `pinyin_simp` schema from Ubuntu's `rime-data-pinyin-simp` package;
IBus shows the Pinyin as preedit text and the candidates in its own window.
Space commits the first candidate, 1–7 or a click selects another, −/= or Page Up/Page Down change the page,
Esc cancels, and Caps Lock types Latin letters. Shift + Delete or Ctrl + K forgets a learned phrase.
Rime keeps what it learns in `~/.local/share/omnikey/rime`.
The first time 中 is selected, Rime prepares its data in the background; keys type Latin letters until it is ready.
