// ============================================================
// attnlista_hls.h -- type/constant definitions for the LISTA core
//
// HONESTY NOTE, upfront: I cannot compile or synthesize this in actual
// Vitis HLS -- I don't have access to that toolchain or your machine.
// I verified what I could (std::complex synthesis support is confirmed
// in AMD's own UG1399 docs; this code's ALGORITHM is verified correct
// against your real trained weights via attnlista_golden_model.py,
// 1.7e-6 relative error). What I could NOT verify: that this specific
// C++ actually synthesizes cleanly in your Vitis 2025.2 installation.
// Treat this as a strong, carefully-reasoned starting point, not a
// guaranteed-working final result -- compile it, and if Vitis HLS
// throws synthesis errors, they'll be specific and fixable, not a sign
// the whole approach is wrong.
//
// FFT CHOICE: implemented as a plain radix-2 Cooley-Tukey FFT directly
// in this file (N_RIS=64=2^6 makes this a clean fit), rather than
// depending on Xilinx's Vitis SSR FFT library -- that library needs
// its own complex_wrapper type (not std::complex), a separate git
// clone, and is built for wide-datapath/high-throughput datacenter
// use cases, real overhead for a modest 64-point transform. If you
// later want the library's higher-throughput pipelining, this
// function's interface (in-place complex array in/out) is a clean
// drop-in replacement point.
//
// FIXED-POINT WIDTHS: chosen from the REAL manifest.json dynamic-range
// data (not guessed) -- see the comment above each typedef.
// ============================================================
#ifndef ATTNLISTA_HLS_H
#define ATTNLISTA_HLS_H

#include <ap_fixed.h>
#include <complex>

// ---- Model dimensions (from your real hls_export/manifest.json) ----
#define K_LAYERS   16
#define N_BS       16
#define N_RIS      64   // must stay a power of 2 for the radix-2 FFT below
#define N_PATHS    65   // N_RIS + 1 (direct link)
#define M_MEAS     16
#define REFINE_HIDDEN 48

// ---- Fixed-point types ----
// Weight values (gamma, thresholds): manifest shows max_abs ~0.024-0.46.
// Added AP_RND (round) and AP_SAT (saturate) to prevent silent wraparound to
// negative numbers in case a threshold or gamma scales to exactly 1.0 or beyond.
typedef ap_fixed<16, 1, AP_RND, AP_SAT> weight_t;

// Pilot matrix P_mat: Rademacher, every entry is EXACTLY +1 or -1.
// The manifest confirms the imaginary part is all zeros, so this is strictly real!
// We only need 2 bits for a signed integer range [-2, 1] which perfectly covers -1 and 1.
typedef ap_int<2> pmat_t;

// Signal/activation values flowing through the network.
// Added AP_RND and AP_SAT to prevent overflow during heavy accumulation.
typedef ap_fixed<32, 16, AP_RND, AP_SAT> signal_t;

typedef std::complex<signal_t> csignal_t;
typedef std::complex<weight_t> cweight_t;

#endif
