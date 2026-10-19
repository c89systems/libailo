#!/usr/bin/env python3
"""Generate deterministic, small image fixtures; no test execution lives here."""
import pathlib
import struct
import sys
import zlib
import math


def chunk(kind, value):
    return (struct.pack(">I", len(value)) + kind + value
            + struct.pack(">I", zlib.crc32(kind + value)))


root = pathlib.Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
for name, color in (("red", b"\xff\x00\x00"), ("blue", b"\x00\x00\xff")):
    width = height = 256
    raw = (b"\x00" + color * width) * height
    image = (b"\x89PNG\r\n\x1a\n"
             + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
             + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
    (root / (name + ".png")).write_bytes(image)

raw = b"\x00" * (2 * 1024 * 1024)
bomb = (b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
(root / "inflate.png").write_bytes(bomb)


def wav(format_id, bits, payload, channels=1, rate=16000):
    fmt = struct.pack("<HHIIHH", format_id, channels, rate, rate * channels * bits // 8,
                      channels * bits // 8, bits)
    body = b"WAVEfmt " + struct.pack("<I", len(fmt)) + fmt
    body += b"data" + struct.pack("<I", len(payload)) + payload
    return b"RIFF" + struct.pack("<I", len(body)) + body


tone = b"".join(struct.pack("<h", int(1000 * math.sin(i * math.tau * 440 / 16000)))
                for i in range(32000))
(root / "tone.wav").write_bytes(wav(1, 16, tone))
(root / "nan.wav").write_bytes(wav(3, 32, struct.pack("<f", float("nan")) * 64))
stereo = b"".join(struct.pack("<hh", int(1000 * math.sin(i * math.tau * 440 / 8000)),
                            int(1000 * math.sin(i * math.tau * 660 / 8000)))
                  for i in range(800))
(root / "stereo.wav").write_bytes(wav(1, 16, stereo, 2, 8000))
animated = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
            + chunk(b"acTL", struct.pack(">II", 2, 0))
            + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0")) + chunk(b"IEND", b""))
(root / "animated.png").write_bytes(animated)
