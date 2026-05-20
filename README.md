# Clawdmeter (Guition JC3248W535 Port)

A small ESP32 dashboard for your desk to keep an eye on Claude Code usage.

This is a dedicated fork of [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) ported to the **Guition JC3248W535** — a 3.5" 320×480 capacitive-touch HMI board built around the ESP32-S3 (also sold as the DIYmalls 3.5" ESP32-S3 HMI). It pairs with your laptop over Bluetooth and shows your Claude Code usage in real-time.

|              Usage meter              |              Clawd animation screen              |
| :-----------------------------------: | :----------------------------------------------: |
| ![Usage meter](assets/claudeUsage.png) | ![Clawd animation screen](assets/demo.gif) |

The splash screen plays pixel-art Clawd animations that get busier when your usage rate climbs. The animations come from [claudepix](https://claudepix.vercel.app), [@amaanbuilds](https://x.com/amaanbuilds)'s library of pixel-art Clawd sprites.

## What's different in this fork

- **Target board**: ported from the Waveshare ESP32-S3-Touch-AMOLED-2.16 (480×480 AMOLED + AXP2101 PMU + QMI8658 IMU) to the **Guition JC3248W535** (320×480 IPS via AXS15231B, no PMU, no IMU).
- **Inputs**: physical-button + tap inputs replaced with **on-screen "Voice" and "Toggle" buttons** plus tap zones, since the Guition board has no side buttons.
- **Display pipeline**: 8 MB OPI PSRAM full-frame buffering with a CPU-side pixel transform in `my_flush_cb` to bypass driver rotation bugs and produce a clean 480×320 landscape image.
- **macOS host support**: in addition to the Linux systemd daemon, a Python + `bleak` daemon runs under launchd on macOS.
- **No battery telemetry yet**: the Guition board has no PMU, so battery monitoring needs an external voltage divider on an ADC pin — wiring TBD.

## Screens

- **Usage Dashboard**: session and weekly utilization, reset timers, and an activity spinner.
- **Bluetooth Status**: connection state, device name, and MAC address.
- **Splash Screen**: pixel-art Clawd animations whose pick density tracks current usage rate.

## Interaction

- **Tap the Claude logo** (top-left) — cycle between the Usage and Bluetooth screens.
- **Tap the background** — toggle the Splash animation.
- **On-screen "Voice" button** — hold to trigger Claude Code's voice mode (`Space`).
- **On-screen "Toggle" button** — tap to switch Claude Code modes (`Shift+Tab`).

The on-screen buttons act as a BLE HID keyboard, so they work against any focused terminal on the paired machine.

## Hardware

- **Guition JC3248W535**: ESP32-S3-WROOM-1, 3.5" 320×480 IPS display (AXS15231B controller).
- **8 MB OPI PSRAM**: full-frame buffer for glitch-free rendering.
- **Capacitive touch**: calibrated for landscape interaction.
- **Backlight**: PWM on GPIO 1.
- **Battery meter**: ⚠️ not wired — see note above.

## Installation

### Flash the firmware

```bash
cd firmware
pio run -e jc3248w535 -t upload
```

### Pair the device

It advertises as **"Claude Controller"**.

```bash
# Linux
bluetoothctl scan le
bluetoothctl pair <MAC>
bluetoothctl trust <MAC>
```

On macOS, pairing happens automatically the first time the daemon connects.

### Install the daemon (Linux)

```bash
./install.sh
systemctl --user start claude-usage-daemon
```

The installer drops a systemd user unit into `~/.config/systemd/user/`, points it at the bash daemon (`daemon/claude-usage-daemon.sh`), and enables it. The daemon will start automatically on login.

### Install the daemon (macOS)

```bash
./install-mac.sh
launchctl load ~/Library/LaunchAgents/com.user.claude-usage-daemon.plist
```

This sets up a Python virtualenv with `bleak`, installs a launchd `LaunchAgent`, and starts the Python daemon (`daemon/claude_usage_daemon.py`) on next login. Logs land in `~/Library/Logs/claude-usage-daemon.{out,err}.log`.

### Flash + monitor shortcut

```bash
./flash.sh        # Linux: flash + monitor in one go
./flash-mac.sh    # macOS equivalent
```

## How it works

1. The daemon reads your Claude Code OAuth token from `~/.claude/.credentials.json`.
2. It polls the Anthropic API for rate-limit headers every 60s.
3. The JSON payload is written to the ESP32 over a custom BLE GATT service (`4c41555a-…`).
4. The firmware (LVGL 9 on Arduino-ESP32 3.x via the pioarduino platform) renders the dashboard. A CPU-side pixel transform in the flush callback works around the AXS15231B's rotation limitations.
5. The on-screen buttons surface as a BLE HID keyboard, sending `Space` or `Shift+Tab` to whichever machine they're paired to.

## Repo layout

```text
firmware/    PlatformIO project (env: jc3248w535)
daemon/      claude-usage-daemon.sh (Linux), claude_usage_daemon.py (macOS)
assets/      Logos, icons, fonts, demo GIF
screenshots/ Photos of the device in operation
tools/       Pixel-art scrape + LVGL asset conversion scripts
```

## Credits

- Upstream: [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) — original Panlee SC01 Plus and Waveshare AMOLED implementations.
- Pixel-art Clawd animations by [@amaanbuilds](https://x.com/amaanbuilds), sourced from [claudepix.vercel.app](https://claudepix.vercel.app).
- Lucide icon set ([lucide.dev](https://lucide.dev), MIT) for UI glyphs.
- Guition JC3248W535 port and macOS host support adapted with AI coding assistants (Gemini CLI + Claude Code).

## Licensing gray area warning

The software in this repository uses and adheres to the Anthropic brand guidelines, and uses the same proprietary fonts that Anthropic has a license for but this software uses without permission, as well as using assets from Anthropic such as the copyrighted Clawd mascot. Even though the code in this repo is non-proprietary, it is not licensed under a copyleft license because this repo includes proprietary fonts and copyrighted assets. Please be aware of this if you fork or copy code from this repo. **You have been warned!**
