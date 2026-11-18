"""Maakt alle geluidseffecten zelf (synthese, geen samples van anderen) -> Resources/sfx_*.wav
22050 Hz, mono, 16 bit. Gebruik: py -3.12 tools/make_sounds.py
"""
import os
import wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 22050
rng = np.random.default_rng(7)


def t_(dur):
    return np.arange(int(SR * dur)) / SR


def env(n, attack=0.005, decay=None, dur=None):
    t = np.arange(n) / SR
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    if decay:
        a = a * np.exp(-t / decay)
    return a


def lowpass(x, cut):
    a = np.exp(-2 * np.pi * cut / SR)
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def bandpass(x, f, q=5.0):
    w0 = 2 * np.pi * f / SR
    alpha = np.sin(w0) / (2 * q)
    b0, b1, b2 = alpha, 0, -alpha
    a0, a1, a2 = 1 + alpha, -2 * np.cos(w0), 1 - alpha
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def tone(freq, dur, shape="sine"):
    t = t_(dur)
    ph = 2 * np.pi * np.cumsum(np.broadcast_to(freq, t.shape) if np.ndim(freq) else np.full(t.shape, freq)) / SR
    if shape == "square":
        return np.sign(np.sin(ph)) * 0.6
    if shape == "saw":
        return 2 * ((ph / (2 * np.pi)) % 1.0) - 1
    return np.sin(ph)


def noise(dur):
    return rng.uniform(-1, 1, int(SR * dur))


def save(name, x, gain=0.9):
    x = np.asarray(x, dtype=np.float64)
    m = np.max(np.abs(x)) or 1.0
    x = x / m * gain
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(os.path.join(ROOT, "Resources", "sfx_%s.wav" % name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


def gunshot(dur, cut, thump, tail):
    n = noise(dur)
    n = lowpass(n, cut) * env(len(n), 0.001, tail)
    b = tone(thump, dur) * env(int(SR * dur), 0.001, tail * 0.6)
    crack = noise(dur) * env(int(SR * dur), 0.0005, 0.008)
    return n * 1.0 + b * 0.8 + crack * 0.6


# --- schoten
save("pistol", gunshot(0.32, 3500, 95, 0.06))
save("smg", gunshot(0.16, 4500, 120, 0.03), 0.75)
s = gunshot(0.6, 2200, 60, 0.12) + gunshot(0.6, 1500, 45, 0.18) * 0.6
save("shotgun", s)
save("copshot", gunshot(0.3, 2800, 85, 0.05), 0.65)
c = np.zeros(int(SR * 0.12))
c[:200] += noise(200 / SR) * env(200, 0.0005, 0.002)
c[1400:1600] += noise(200 / SR) * env(200, 0.0005, 0.002) * 0.7
save("empty", bandpass(c, 3000, 2), 0.5)

# --- mensen
t = t_(0.7)
f0 = 520 + 380 * np.sin(np.pi * np.clip(t / 0.7, 0, 1)) + 25 * np.sin(2 * np.pi * 7 * t)
v = tone(f0, 0.7, "saw")
v = bandpass(v, 900, 3) * 1.2 + bandpass(v, 1500, 4) * 0.8 + bandpass(v, 2600, 6) * 0.4
save("scream", v * env(len(v), 0.03, 0.45), 0.7)
t = t_(0.3)
v = tone(200 - 90 * t / 0.3, 0.3, "saw")
save("hurt", lowpass(v, 900) * env(len(v), 0.01, 0.12), 0.8)

# --- voertuigen en wereld
n = lowpass(noise(0.7), 1200) * env(int(SR * 0.7), 0.002, 0.15)
ring = (tone(340, 0.7) + tone(587, 0.7) * 0.6 + tone(910, 0.7) * 0.3) * env(int(SR * 0.7), 0.002, 0.18)
save("crash", n + ring * 0.5)
save("door", lowpass(noise(0.18), 500) * env(int(SR * 0.18), 0.002, 0.04) + tone(110, 0.18) * env(int(SR * 0.18), 0.002, 0.05))
save("jump", bandpass(noise(0.22), 900, 1.5) * np.sin(np.pi * np.clip(t_(0.22) / 0.22, 0, 1)), 0.45)

# --- geld, missies, menu
def bell(f, dur, dec):
    return (tone(f, dur) + tone(f * 2.01, dur) * 0.4 + tone(f * 3.02, dur) * 0.15) * env(int(SR * dur), 0.002, dec)
cash = np.concatenate([bell(1318, 0.09, 0.05), bell(1760, 0.35, 0.12)])
save("cash", cash, 0.6)
save("check", np.concatenate([bell(880, 0.08, 0.05), bell(1320, 0.4, 0.15)]), 0.6)
notes = [523, 659, 784, 1047]
p = np.concatenate([tone(f, 0.13, "square") * env(int(SR * 0.13), 0.005, 0.08) for f in notes[:3]] +
                   [tone(1047, 0.5, "square") * env(int(SR * 0.5), 0.005, 0.25)])
save("pass", lowpass(p, 3000), 0.6)
t = t_(0.8)
save("fail", lowpass(tone(420 - 230 * t / 0.8, 0.8, "square"), 1500) * env(len(t), 0.01, 0.35), 0.55)
a = np.concatenate([tone(1000 if k % 2 == 0 else 780, 0.22, "square") * env(int(SR * 0.22), 0.004) for k in range(14)])
save("alarm", lowpass(a, 2500), 0.5)
kc = np.concatenate([lowpass(noise(0.05), 2500) * env(int(SR * 0.05), 0.001, 0.01), bell(2093, 0.5, 0.2)])
save("buy", kc, 0.6)
save("click", tone(1400, 0.035) * env(int(SR * 0.035), 0.001, 0.008), 0.4)

# --- lussen (naadloos)
t = t_(1.0)
eng = sum(np.sin(2 * np.pi * 55 * k * t) / k for k in (1, 2, 3, 4, 6)) + 0.15 * lowpass(noise(1.0), 400)
eng *= 0.75 + 0.25 * np.sin(2 * np.pi * 27.5 * t) ** 2
save("engine", eng, 0.7)
bike = np.zeros(int(SR * 0.5))
for k in range(8):
    i = int(k * SR * 0.5 / 8)
    bike[i:i + 300] += bandpass(noise(300 / SR), 2500, 3) * env(300, 0.0005, 0.004)
bike += 0.15 * bandpass(noise(0.5), 600, 1.0)
save("bike", bike, 0.4)
dur = 2.0
t = t_(dur)
f = 950 + 350 * np.sin(2 * np.pi * t / dur)                 # gemiddeld 950 Hz -> 1900 hele trillingen in 2 s
siren = tone(f, dur, "square") * 0.6 + tone(f, dur) * 0.4
save("siren", lowpass(siren, 3000), 0.5)
print("klaar")
