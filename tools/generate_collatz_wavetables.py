"""
Generate Collatz-derived wavetable banks for Collonka.

Outputs 21 binary files into ../resources/:
  collatz_K{k}_rule{r}.bin   for k in {3..9} and r in {0, 1, 2}

Each bank is 256 frames x 2048 samples x float32 = 2 MB. Total 42 MB.

Frame layout: frame N (N = 0..255) carries residue r = N mod 2^k. For k < 8
this means frames repeat every 2^k along the 256-frame axis.

Three derivation rules:

  rule 0 -- K_k transfer-matrix spectrum at residue r
            K_k is the 2^k x 2^k row-stochastic matrix of one-step Collatz
            transitions on residues mod 2^k under the compressed map
            T(n) = (3n+1)/2 if n odd else n/2, with higher bits assumed
            uniform. Iterating K_k a few steps spreads probability mass and
            yields a residue-dependent harmonic distribution. Row r of K_k^N
            is used as harmonic magnitudes.

  rule 1 -- alpha_det-phased saw harmonics
            Magnitudes: 1/m saw spectrum (residue-independent).
            Phase of harmonic m offset by alpha_det(r) * m * pi where
                alpha_det(r) = prefix_steps(r) + Kh * log(a_final(r) / 2^k)
            with Kh = 3 / log(4/3) ~= 10.428, prefix_steps(r) the Collatz
            descent time from r to its first iterate < r, and a_final(r) the
            value of that first iterate.

  rule 2 -- prefix-iteration parity sequence (Walsh-like)
            Iterate Collatz from r, take parity bit at each step, map
            {0,1} -> {-1,+1}, tile to 2048 samples.

All frames are DC-removed and peak-normalized to [-1, +1].
"""

import math
import os
import sys
import time
import numpy as np

NUM_FRAMES = 256
SAMPLES    = 2048
K_VALUES   = list(range(3, 10))   # 3..9 inclusive
RULES      = [0, 1, 2]
KH         = 3.0 / math.log(4.0 / 3.0)
KK_POWER   = 8                    # K_k^N used for rule 0 spectrum

# Bass-synth harmonic discipline: keep the residue-dependent movement, drop
# the upper-harmonic mud. 24 harmonics gets us through ~1.5 kHz at C2 fundamental.
MAX_HARM    = 12
TILT_EXP    = 2.5     # rule 0 darkening: weight harmonic m by K_k_row[m] / m^TILT_EXP

# Fundamental dominance: blend a fixed sine at harmonic 1 with the Collatz-derived
# content. Without this, residues whose K_k row puts near-zero weight on harmonic 1
# produce upper-register-only waveforms with no perceived fundamental, so the
# played note "ghosts" up an octave or more.
FUND_BLEND  = 0.65    # 0..1; how much pure sine fundamental to mix with the Collatz signal

THIS_DIR  = os.path.dirname(os.path.abspath(__file__))
RESOURCES = os.path.normpath(os.path.join(THIS_DIR, "..", "resources"))


# ---------------------------------------------------------------------------
# Collatz primitives
# ---------------------------------------------------------------------------

def collatz_step(n: int) -> int:
    """Compressed Collatz map: T(n) = (3n+1)/2 if n odd else n/2."""
    return (3 * n + 1) // 2 if (n & 1) else (n >> 1)


def prefix_descent(r: int, max_steps: int = 256):
    """Iterate from r until trajectory drops strictly below r.

    Returns (steps, n_final). For r in {0, 1} returns (0, max(r, 1)).
    """
    if r <= 1:
        return 0, max(r, 1)
    n = r
    for s in range(1, max_steps + 1):
        n = collatz_step(n)
        if n < r:
            return s, n
    return max_steps, n


def alpha_det(r: int, k: int) -> float:
    if r == 0:
        return 0.0
    steps, n_final = prefix_descent(r)
    return steps + KH * math.log(max(n_final, 1) / float(2 ** k))


# ---------------------------------------------------------------------------
# K_k transfer matrix
# ---------------------------------------------------------------------------

def build_transfer_matrix(k: int) -> np.ndarray:
    """Row-stochastic 2^k x 2^k matrix.

    Even residue r:  successors r/2 and r/2 + 2^(k-1), each prob 1/2.
    Odd residue r:   successors (3(r-1)/2 + 2) mod 2^k and that + 2^(k-1).
    """
    N = 2 ** k
    K = np.zeros((N, N), dtype=np.float64)
    for r in range(N):
        if r % 2 == 0:
            s1 = r // 2
            s2 = (r // 2 + N // 2) % N
        else:
            base = (3 * (r - 1) // 2 + 2) % N
            s1 = base
            s2 = (base + N // 2) % N
        K[r, s1] += 0.5
        K[r, s2] += 0.5
    return K


_KK_CACHE: dict = {}

def kk_power_row(k: int, r: int) -> np.ndarray:
    """Row r of K_k ** KK_POWER. Cached per k."""
    if k not in _KK_CACHE:
        K = build_transfer_matrix(k)
        _KK_CACHE[k] = np.linalg.matrix_power(K, KK_POWER)
    return _KK_CACHE[k][r % (2 ** k)]


# ---------------------------------------------------------------------------
# Frame rendering (vectorized)
# ---------------------------------------------------------------------------

# Phase grid: one period across SAMPLES samples.
_T = np.arange(SAMPLES, dtype=np.float64) / float(SAMPLES)


def _harmonic_basis_sin(num_harmonics: int) -> np.ndarray:
    """(num_harmonics, SAMPLES) matrix where row m has sin(2 pi (m+1) t)."""
    m = np.arange(1, num_harmonics + 1, dtype=np.float64)
    return np.sin(2.0 * math.pi * np.outer(m, _T))


def render_kk_spectrum_frame(k: int, r: int) -> np.ndarray:
    """Rule 0: K_k row entries 1..MAX_HARM as magnitudes, with 1/m^TILT_EXP darkening.

    Residue-dependent structure (the "movement") survives because K_k_row[m] still
    varies per residue; the tilt just darkens the upper harmonics so the bank
    sits clean in a bass register.
    """
    spectrum = kk_power_row(k, r)
    N = len(spectrum)
    if N <= 1:
        return np.zeros(SAMPLES, dtype=np.float64)
    num_harm = min(N - 1, MAX_HARM)
    m = np.arange(1, num_harm + 1, dtype=np.float64)
    mags = spectrum[1:num_harm + 1] / np.power(m, TILT_EXP)
    sins = _harmonic_basis_sin(num_harm)
    return mags @ sins


def render_alpha_phase_frame(k: int, r: int) -> np.ndarray:
    """Rule 1: 1/m saw magnitudes with alpha_det-derived per-harmonic phase offsets, capped."""
    a = alpha_det(r, k)
    num_harm = min(2 ** k, MAX_HARM)
    m = np.arange(1, num_harm + 1, dtype=np.float64)
    mags = 1.0 / m
    phase = a * m * math.pi
    arg = 2.0 * math.pi * np.outer(m, _T) + phase[:, None]
    sins = np.sin(arg)
    return mags @ sins


def render_parity_frame(k: int, r: int) -> np.ndarray:
    if r == 0:
        # Square ±1 alternating per sample. Will tile then be peak-normalized.
        seq = np.empty(SAMPLES, dtype=np.float64)
        seq[0::2] = -1.0
        seq[1::2] = 1.0
        return seq

    n = max(r, 1)
    parity = []
    seen = set()
    max_iters = SAMPLES
    for _ in range(max_iters):
        parity.append(n & 1)
        n = collatz_step(n)
        if n == 1 or n in seen:
            break
        seen.add(n)
    if not parity:
        parity = [0]

    bits = np.asarray(parity, dtype=np.float64)
    waveform = np.where(bits > 0.5, 1.0, -1.0)
    if len(waveform) < SAMPLES:
        reps = (SAMPLES + len(waveform) - 1) // len(waveform)
        waveform = np.tile(waveform, reps)
    waveform = waveform[:SAMPLES]

    # Brick-wall low-pass to MAX_HARM harmonics: FFT, zero high bins, IFFT.
    # Keeps the bit-sequence-driven residue character but kills the ultrasonic
    # edge content that makes raw parity sequences sound brittle and muddy at the same time.
    spec = np.fft.rfft(waveform)
    spec[MAX_HARM + 1:] = 0.0
    waveform = np.fft.irfft(spec, n=SAMPLES)
    return waveform


def normalize(buf: np.ndarray) -> np.ndarray:
    buf = buf - np.mean(buf)
    peak = np.max(np.abs(buf))
    if peak > 1e-9:
        buf = buf / peak
    return buf


_FUND_SINE = np.sin(2.0 * math.pi * _T)


def render_frame(k: int, r: int, rule: int) -> np.ndarray:
    if rule == 0:
        buf = render_kk_spectrum_frame(k, r)
    elif rule == 1:
        buf = render_alpha_phase_frame(k, r)
    elif rule == 2:
        buf = render_parity_frame(k, r)
    else:
        raise ValueError(rule)

    # Normalize the Collatz-derived character first so the blend ratio is meaningful.
    buf = normalize(buf)
    # Blend with strong sine fundamental — residues that lacked harmonic-1 content
    # now sit at the played pitch instead of ghosting up an octave.
    blended = FUND_BLEND * _FUND_SINE + (1.0 - FUND_BLEND) * buf
    return normalize(blended).astype(np.float32)


def render_bank(k: int, rule: int) -> np.ndarray:
    bank = np.zeros((NUM_FRAMES, SAMPLES), dtype=np.float32)
    K_max = 2 ** k
    for n in range(NUM_FRAMES):
        r = n % K_max
        bank[n] = render_frame(k, r, rule)
    return bank


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def main() -> int:
    os.makedirs(RESOURCES, exist_ok=True)
    t0 = time.time()
    total_bytes = 0
    for k in K_VALUES:
        for rule in RULES:
            tb = time.time()
            bank = render_bank(k, rule)
            out_path = os.path.join(RESOURCES, f"collatz_K{k}_rule{rule}.bin")
            bank.tofile(out_path)
            sz = os.path.getsize(out_path)
            total_bytes += sz
            print(f"K{k} rule{rule}  ->  {out_path}  ({sz/1024/1024:.2f} MB, {time.time()-tb:.1f}s)", flush=True)
    print(f"Done. {total_bytes/1024/1024:.1f} MB across {len(K_VALUES)*len(RULES)} banks in {time.time()-t0:.1f}s.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
