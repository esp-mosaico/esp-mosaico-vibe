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
    rng = random.Random(98); count = int(RATE * .32); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; noise = rng.uniform(-1, 1); low = low * .82 + noise * .18
        crack = noise * math.exp(-t * 52.0)
        body = math.sin(2 * math.pi * (105 - 55 * t) * t) * math.exp(-t * 13.0)
        tail = math.sin(2 * math.pi * 180 * t) * math.exp(-t * 4.6) * (0.0 if t < .04 else 1.0)
        bolt = 0.0
        if .11 < t < .18:
            local = t - .11
            bolt = rng.uniform(-1, 1) * math.exp(-local * 48.0) * .22
            bolt += math.sin(2 * math.pi * 2400 * t) * math.exp(-local * 36.0) * .16
        result.append(.50 * crack + .36 * body + .14 * tail + bolt + .12 * low * math.exp(-t * 8.0))
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

def empty():
    rng = random.Random(404); count = int(RATE * .09); result = []
    for i in range(count):
        t = i / RATE
        click = rng.uniform(-1, 1) * math.exp(-t * 70.0)
        ring = math.sin(2 * math.pi * 2100 * t) * math.exp(-t * 38.0)
        result.append(.22 * click + .18 * ring)
    return result

def pickup():
    count = int(RATE * .18); result = []
    for i in range(count):
        t = i / RATE
        freq = 520 if t < .07 else 780
        local = t if t < .07 else t - .07
        result.append(.22 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 16.0))
    return result

def step(seed, pitch):
    rng = random.Random(seed)
    count = int(RATE * .10)
    result = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low = low * .70 + noise * .30
        thud = math.sin(2 * math.pi * (pitch - 36 * t) * t) * math.exp(-t * 26.0)
        grit = noise * math.exp(-t * 58.0) * (1.0 if t < .016 else .14)
        result.append(.40 * thud + .26 * grit + .14 * low * math.exp(-t * 20.0))
    return result

def alert():
    count = int(RATE * .20); result = []
    for i in range(count):
        t = i / RATE
        freq = 980 if t < .08 else 620
        local = t if t < .08 else t - .08
        result.append(.24 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 14.0))
    return result

save("rifle.wav", rifle())
save("impact.wav", impact())
save("confirm.wav", confirm())
save("hurt.wav", hurt())
save("empty.wav", empty())
save("pickup.wav", pickup())
save("alert.wav", alert())
save("step_l.wav", step(19, 88))
save("step_r.wav", step(41, 104))
