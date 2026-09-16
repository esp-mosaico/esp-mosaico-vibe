#!/usr/bin/env python3
"""Generate deterministic, compact tactical feedback sounds."""
from pathlib import Path
import math, random, struct, wave

RATE = 24000
ROOT = Path(__file__).resolve().parent

def save(name, samples):
    pcm = b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 32767))))
                   for v in samples)
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1); out.setsampwidth(2); out.setframerate(RATE)
        out.writeframes(pcm)

def rifle():
    rng = random.Random(98); count = int(RATE * .24); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; noise = rng.uniform(-1, 1); low = low * .82 + noise * .18
        crack = noise * math.exp(-t * 48.0)
        body = math.sin(2 * math.pi * (105 - 55 * t) * t) * math.exp(-t * 13.0)
        result.append(.55 * crack + .42 * body + .16 * low * math.exp(-t * 9.0))
    return result

def impact():
    rng = random.Random(762); count = int(RATE * .11); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; low = low * .68 + rng.uniform(-1, 1) * .32
        result.append((.35 * low + .22 * math.sin(2 * math.pi * 145 * t)) *
                      math.exp(-t * 31.0))
    return result

def confirm():
    count = int(RATE * .22); result = []
    for i in range(count):
        t = i / RATE; freq = 660 if t < .085 else 880
        local = t if t < .085 else t - .085
        result.append(.20 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 13.0))
    return result

def hurt():
    rng = random.Random(311); count = int(RATE * .28); result = []; low = 0.0
    for i in range(count):
        t = i / RATE
        low = low * .88 + rng.uniform(-1, 1) * .12
        thump = math.sin(2 * math.pi * (74 - 22 * t) * t) * math.exp(-t * 10.0)
        crack = low * math.exp(-t * 19.0)
        result.append(.48 * thump + .25 * crack)
    return result

save("rifle.wav", rifle())
save("impact.wav", impact())
save("confirm.wav", confirm())
save("hurt.wav", hurt())
