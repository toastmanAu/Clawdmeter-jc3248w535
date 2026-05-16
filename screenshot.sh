#!/bin/bash
# Take a screenshot from the display via LVGL snapshot.
# Usage: ./screenshot.sh [output.png] [port]

OUTPUT="${1:-screenshot.png}"
PORT="${2:-/dev/ttyACM0}"

TMPRAW=$(mktemp /tmp/screenshot_XXXXXX.raw)
trap "rm -f '$TMPRAW'" EXIT

echo "Taking screenshot from $PORT..."

# Python script to capture raw data over serial
READ_RESULT=$(python3 - "$PORT" "$TMPRAW" << 'PYEOF'
import serial, sys

port_path, raw_path = sys.argv[1], sys.argv[2]

try:
    port = serial.Serial(port_path, 115200, timeout=10)
    port.reset_input_buffer()
    port.write(b"screenshot\n")
    port.flush()
except Exception as e:
    print(f"Error opening port: {e}", file=sys.stderr)
    sys.exit(1)

w, h = 0, 0
while True:
    line = port.readline().decode("utf-8", errors="replace").strip()
    if line.startswith("SCREENSHOT_START"):
        parts = line.split()
        w, h, raw_size = int(parts[1]), int(parts[2]), int(parts[3])
        break
    if line == "SCREENSHOT_ERR":
        print("Device reported screenshot error", file=sys.stderr)
        sys.exit(1)

data = b""
while len(data) < raw_size:
    chunk = port.read(min(4096, raw_size - len(data)))
    if not chunk:
        print(f"Timeout: got {len(data)} of {raw_size} bytes", file=sys.stderr)
        sys.exit(1)
    data += chunk

with open(raw_path, "wb") as f:
    f.write(data)

for _ in range(10):
    line = port.readline().decode("utf-8", errors="replace").strip()
    if line == "SCREENSHOT_END":
        break

port.close()
print(f"{w}x{h}")
PYEOF
)

if [ $? -ne 0 ]; then
    echo "Screenshot capture failed"
    exit 1
fi

# Get dimensions from python output
DIMENSIONS=$READ_RESULT
echo "Captured $DIMENSIONS"

# Use ffmpeg to convert raw RGB565 to PNG
ffmpeg -y -f rawvideo -pixel_format rgb565le -video_size "$DIMENSIONS" \
    -i "$TMPRAW" -update 1 -frames:v 1 "$OUTPUT" 2>/dev/null || true

if [ -f "$OUTPUT" ]; then
    echo "Saved: $OUTPUT"
else
    echo "Error: conversion failed"
    exit 1
fi
