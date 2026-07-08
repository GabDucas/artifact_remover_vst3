"""
Generate a synthetic test signal for the Artifact Remover plugin.

The algorithm is designed for EMG-like signals:
  - SIGNAL   : broadband noise within the target band (10-300 Hz)
               → SVD components have spread-out spectra → low energy concentration → kept
  - ARTIFACT : narrow pure-tone sinusoid outside the band (400 Hz)
               → SVD component has concentrated spectrum + out-of-band frequency → removed

Using a pure sinusoid as the "signal" would fail because pure tones also have concentrated
energy and would be flagged by the energy-ratio rejection criterion.
"""

import numpy as np

SR = 48000
DURATION = 5.0
t = np.arange(0, DURATION, 1.0 / SR)
N = len(t)

rng = np.random.default_rng(42)

# --- Signal: band-limited noise 10–300 Hz (simulated EMG) ---
freqs = np.fft.rfftfreq(N, 1.0 / SR)
spectrum = (rng.standard_normal(len(freqs)) + 1j * rng.standard_normal(len(freqs)))
spectrum[np.abs(freqs) < 10]  = 0.0   # remove sub-10 Hz
spectrum[np.abs(freqs) > 300] = 0.0   # remove above 300 Hz
emg = np.fft.irfft(spectrum, n=N)
emg = emg / np.max(np.abs(emg)) * 0.5  # normalise to ±0.5

# --- Artifact: pure 400 Hz sinusoid (above the 300 Hz upper bound) ---
artifact = 0.3 * np.sin(2 * np.pi * 400 * t)

mixed = emg + artifact
np.savetxt("test_48khz.txt", mixed, fmt="%.8f")

# --- Stats ---
fft_mixed   = np.abs(np.fft.rfft(mixed))
fft_emg     = np.abs(np.fft.rfft(emg))
fft_art     = np.abs(np.fft.rfft(artifact))

band_power_signal   = np.sum(fft_emg[  (freqs >= 10) & (freqs <= 300)] ** 2)
band_power_artifact = np.sum(fft_art[  (freqs >= 380) & (freqs <= 420)] ** 2)

print(f"Signal generated: {N} samples ({DURATION}s @ {SR} Hz)")
print(f"  Signal   : band-limited noise 10–300 Hz  amplitude≈0.5  (should be PRESERVED)")
print(f"  Artifact : 400 Hz pure tone              amplitude=0.3  (should be REMOVED)")
print(f"  Band power ratio signal/artifact: {band_power_signal/band_power_artifact:.1f}x")
print(f"Saved to: test_48khz.txt")
