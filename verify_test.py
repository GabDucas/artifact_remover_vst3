"""
Verify that the artifact remover correctly removed the 400 Hz artifact
while preserving the broadband 10–300 Hz signal (simulated EMG).

Run after:
  python generate_test_signal.py
  .\\build\\Release\\audio_test.exe test_48khz.txt
"""

import numpy as np
import sys

SR = 48000

try:
    original  = np.loadtxt("test_48khz.txt")
    processed = np.loadtxt("test_48khz_processed.txt")
except FileNotFoundError as e:
    print(f"ERROR: {e}")
    print("Run generate_test_signal.py then audio_test.exe first.")
    sys.exit(1)

n = min(len(original), len(processed))
original  = original[:n]
processed = processed[:n]

# Skip initial latency: window=450 samples @ 6kHz * 8 = 3600 samples @ 48kHz
skip = 3600
orig_a = original[skip:]
proc_a = processed[skip:]

freqs = np.fft.rfftfreq(len(orig_a), 1.0 / SR)
fft_orig = np.abs(np.fft.rfft(orig_a))
fft_proc = np.abs(np.fft.rfft(proc_a))

def band_power(fft, f_lo, f_hi):
    mask = (freqs >= f_lo) & (freqs <= f_hi)
    return np.sum(fft[mask] ** 2)

# Signal band (should be preserved)
bp_sig_in  = band_power(fft_orig,  10, 300)
bp_sig_out = band_power(fft_proc,  10, 300)

# Artifact band (should be attenuated)
bp_art_in  = band_power(fft_orig, 380, 420)
bp_art_out = band_power(fft_proc, 380, 420)

sig_ratio = bp_sig_out / bp_sig_in   if bp_sig_in  > 0 else 0.0
art_ratio = bp_art_out / bp_art_in   if bp_art_in  > 0 else 0.0

print("=== Artifact Remover Verification ===\n")
print(f"Samples analyzed : {len(orig_a)} (after {skip}-sample latency skip)")
print()
print("Band power comparison:")
print(f"  10–300 Hz (signal)   — original: {bp_sig_in:12.1f}   processed: {bp_sig_out:12.1f}   kept: {sig_ratio*100:.1f}%")
print(f"  380–420 Hz (artifact)— original: {bp_art_in:12.1f}   processed: {bp_art_out:12.1f}   kept: {art_ratio*100:.1f}%")
print()

ok_kept    = sig_ratio > 0.50  # signal band preserved > 50 %
ok_removed = art_ratio < 0.20  # artifact reduced by > 80 %

print("Results:")
print(f"  10–300 Hz preserved  : {'PASS' if ok_kept    else 'FAIL'}  (expected >50% power retained)")
print(f"  400 Hz artifact      : {'PASS' if ok_removed else 'FAIL'}  (expected <20% power remaining)")

if ok_kept and ok_removed:
    print("\n[OK] Plugin is working correctly.")
elif not ok_kept and not ok_removed:
    print("\n[!!] Both signal and artifact removed — likely over-aggressive rejection.")
    print("     Try increasing 'factor' parameter (e.g. 0.6–1.0) in JuceAudioProcessingTest.cpp")
elif not ok_kept:
    print("\n[!!] Signal partially removed — try increasing 'factor' or widening the frequency band.")
elif not ok_removed:
    print("\n[!!] Artifact not removed — try decreasing 'factor' or tightening the frequency band.")
