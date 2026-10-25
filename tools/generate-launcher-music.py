#!/usr/bin/env python3
"""Synthesize 'The Quiet Threshold', original music for Ashford's Halberd.

No samples, recordings, external libraries or existing melodies are used.
The sustained oscillators complete integer cycles in 32 seconds; bell tails
wrap across the seam. The resulting PCM file can be looped without a click.
Regenerate: python tools/generate-launcher-music.py [output.wav]
"""
from array import array
from pathlib import Path
import math
import sys
import wave

RATE = 22050
SECONDS = 32
TAU = math.tau


def pitch(midi):
    # Quantization is sub-cent at these frequencies and makes the pad periodic.
    return round(440 * 2 ** ((midi - 69) / 12) * SECONDS) / SECONDS


def synthesize(output):
    # Suspended D/A/E/F sonority, with an unhurried, newly composed bell figure.
    pads = [(pitch(n), a, phase) for n, a, phase in
            [(38, .083, .3), (45, .054, 1.1), (50, .047, 2.0),
             (57, .031, .7), (64, .017, 2.4), (65, .014, 1.5)]]
    bells = [(1.0, pitch(74), -.55), (6.5, pitch(81), .4),
             (12.0, pitch(77), -.25), (19.0, pitch(76), .55),
             (25.0, pitch(69), -.4), (29.0, pitch(74), .15)]
    pcm = array('h')
    peak = 0
    energy = 0.0
    for i in range(RATE * SECONDS):
        t = i / RATE
        channels = []
        for side in (-1, 1):
            value = 0.0
            for j, (freq, amp, phase) in enumerate(pads):
                breath = .72 + .18 * math.sin(TAU * t / SECONDS + phase)
                stereo = side * .14 * math.sin(TAU * 2 * t / SECONDS + j)
                value += amp * breath * math.sin(TAU * freq * t + phase + stereo)
                value += .12 * amp * math.sin(TAU * 2 * freq * t + phase)
            for start, freq, pan in bells:
                age = (t - start) % SECONDS
                if age < 10:
                    fade = min(1.0, (10 - age) / 2)
                    env = (1 - math.exp(-age * 70)) * math.exp(-age / 2.2) * fade * fade
                    tone = (math.sin(TAU * freq * age)
                            + .30 * math.sin(TAU * freq * 2.003 * age)
                            + .11 * math.sin(TAU * freq * 3.992 * age))
                    value += .095 * env * tone * (1 + side * pan) / 1.6
                    # Restrained synthetic echo; no borrowed impulse response.
                echo_age = (t - start - .375) % SECONDS
                if echo_age < 8:
                    env = (1 - math.exp(-echo_age * 50)) * math.exp(-echo_age / 2) * min(1.0, (8 - echo_age) / 2) ** 2
                    value += .021 * env * math.sin(TAU * freq * echo_age) * (1 - side * pan)
            sample = round(32767 * math.tanh(value * 1.5))
            channels.append(sample)
            peak = max(peak, abs(sample))
            energy += (sample / 32768) ** 2
        pcm.extend(channels)
    if sys.byteorder != 'little':
        pcm.byteswap()
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output), 'wb') as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(pcm.tobytes())
    rms = math.sqrt(energy / (RATE * SECONDS * 2))
    print(f'{output}: {SECONDS}s, {RATE} Hz stereo PCM16, peak {peak}, RMS {20*math.log10(rms):.1f} dBFS')


if __name__ == '__main__':
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'assets' / 'branding' / 'launcher.wav'
    synthesize(target)
