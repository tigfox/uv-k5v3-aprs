#!/usr/bin/env python3
"""Write ADC test vectors for test_aprs_demod.c from armel's Python model.

For each case: <name>.s16 (little-endian uint16 samples at 9.6 kHz, as the ADC delivers them)
and <name>.exp (one hex frame per line, FCS included: what the Python model's demodulator
decodes from the very same samples). The C demodulator must produce the same list.
Usage: gen_vectors.py <outdir>
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "App", "apps", "aprsrx", "test"))
import model_rx as M            # noqa: E402
from ax25 import test_frames    # noqa: E402

# name, source, twist dB, mode, frame, noise Hz, offset Hz, ppm, seed
CASES = [
    ("flipper_pos_raw",      "flipper", 0.0, "raw", "pos",    0,    0,      0, 1),
    ("flipper_long_raw",     "flipper", 0.0, "raw", "long",   0,    0,      0, 1),
    ("flipper_pos_std",      "flipper", 0.0, "std", "pos",    0,    0,      0, 1),
    ("flipper_badfcs",       "flipper", 0.0, "raw", "badfcs", 0,    0,      0, 1),
    ("flipper_noise1500",    "flipper", 0.0, "raw", "pos",    1500, 0,      0, 2),
    ("flipper_noise3000",    "flipper", 0.0, "raw", "pos",    3000, 0,      0, 3),
    ("flipper_noise4500",    "flipper", 0.0, "raw", "pos",    4500, 0,      0, 1),
    ("flipper_off_neg",      "flipper", 0.0, "std", "pos",    1500, -4000,  0, 1),
    ("flipper_off_pos",      "flipper", 0.0, "std", "pos",    1500, 4000,   0, 1),
    ("flipper_long_ppm_neg", "flipper", 0.0, "std", "long",   1500, 0,  -10000, 1),
    ("flipper_long_ppm_pos", "flipper", 0.0, "std", "long",   1500, 0,   10000, 1),
    ("sine_twist_m6",        "sine",   -6.0, "std", "pos",    1500, 0,      0, 1),
    ("sine_twist_0",         "sine",    0.0, "raw", "digi",   1500, 0,      0, 1),
    ("sine_twist_p6",        "sine",    6.0, "std", "msg",    1500, 0,      0, 2),
    ("sine_twist_p9_raw",    "sine",    9.0, "raw", "status", 1500, 0,      0, 1),
]

def main(out):
    os.makedirs(out, exist_ok=True)
    fr = test_frames()
    for name, src, tw, mode, fname, noise, off, ppm, seed in CASES:
        wave = M.flipper_wave(fr[fname]) if src == "flipper" else M.sine_wave(fr[fname], twist_db=tw)
        adc = M.channel(wave, mode, noise, off, ppm, seed)
        got = M.run(adc)
        with open(os.path.join(out, name + ".s16"), "wb") as f:
            f.write(struct.pack("<%dH" % len(adc), *adc))
        with open(os.path.join(out, name + ".exp"), "w") as f:
            f.write("# truth: %s\n" % ("none" if fname == "badfcs" else fr[fname].hex()))
            for g in got:
                f.write(g.hex() + "\n")
        print("%-22s %6d samples, model decoded %d" % (name, len(adc), len(got)))

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "vectors")
