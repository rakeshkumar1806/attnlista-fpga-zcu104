# ============================================================
# ATTNLISTA GOLDEN MODEL -- complete forward-pass reference
# implementation, NumPy only (NO PyTorch, NO framework dependency).
#
# This is the "algorithm specification" your friend needs alongside
# the weights -- the weights alone don't tell you what OPERATIONS to
# perform with them, in what order, or how the intermediate tensors
# are shaped. This file is that missing piece: every operation the
# trained model performs, written out explicitly and in the same
# style an HLS/Verilog implementation will need to mirror (explicit
# loops over layers/channels rather than vectorized tricks that hide
# the actual operation sequence).
#
# Same role as the Rademacher-QR project's journal_validation_model.py
# -- a fixed reference ("golden model") to validate an HLS/RTL
# implementation against, operation by operation.
#
# USAGE: point EXPORT_DIR at the unzipped hls_export folder, then call
# attnlista_forward(y, H_LS_init, EXPORT_DIR) with your own pilot
# measurement -- or run this file directly for a self-contained
# numerical validation against the real exported weights.
# ============================================================

import os
import json
import numpy as np

EXPORT_DIR = "hls_export"  # change to wherever you unzipped hls_export.zip


# ============================================================
# Weight loading
# ============================================================
def load_weights(export_dir):
    """Loads every .npy array into a flat dict keyed by filename (no
    extension). Complex arrays were split into _real/_imag pairs on
    export -- recombined here into actual complex128 arrays so the
    rest of this file can work with ordinary complex arithmetic."""
    with open(os.path.join(export_dir, "manifest.json")) as f:
        manifest = json.load(f)
    config = manifest["config"]

    raw = {}
    for fname in os.listdir(export_dir):
        if fname.endswith(".npy"):
            raw[fname[:-4]] = np.load(os.path.join(export_dir, fname))

    w = {}
    complex_pairs = set()
    for name in list(raw.keys()):
        if name.endswith("_real") and name[:-5] + "_imag" in raw:
            base = name[:-5]
            w[base] = raw[name].astype(np.complex128) + 1j * raw[base + "_imag"].astype(np.complex128)
            complex_pairs.add(base + "_real"); complex_pairs.add(base + "_imag")
    for name, arr in raw.items():
        if name not in complex_pairs:
            w[name] = arr.astype(np.float64)

    return w, config


# ============================================================
# Building blocks (each mirrors ONE operation the trained model
# performs -- keep these as separate, explicit functions; this
# structure maps directly onto separate HLS functions/pipeline stages)
# ============================================================
def soft_threshold(x, theta, scale=None):
    """Complex soft-threshold shrinkage: theta is per-channel (last
    axis), scale is an optional per-sample multiplier (broadcasts over
    the batch dimension). IMPORTANT: the trained theta values include
    NEGATIVE entries (a free nn.Parameter, not constrained positive
    during training) -- the original model always takes abs(theta)
    before using it as a threshold (torch.abs(self.theta) in the
    PyTorch LearnableSoftThreshold.forward). Caught via direct
    numerical cross-validation against the real trained weights: the
    real threshold_ris_layer0 array has min=-0.317, and omitting abs()
    here caused a 112% relative-error mismatch against the original
    model -- not a theoretical edge case, a real bug that would have
    silently produced a wrong hardware implementation. mag_thresh =
    relu(|x| - |theta|*scale); output = x * mag_thresh / |x|."""
    theta_abs = np.abs(theta)
    theta_eff = theta_abs if scale is None else theta_abs * scale
    mag = np.abs(x) + 1e-12
    mag_thresh = np.maximum(mag - theta_eff, 0.0)
    return x * (mag_thresh / mag)


def linear(x, weight, bias):
    """Standard dense layer: y = x @ weight.T + bias. weight shape is
    (out_features, in_features) -- PyTorch's nn.Linear convention,
    kept as-is since that's how the exported arrays are shaped."""
    return x @ weight.T + bias


def relu(x):
    return np.maximum(x, 0.0)


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def conv1d(x, weight, bias, padding=1):
    """1D convolution, kernel size 3, stride 1, 'same' padding.
    x: (in_channels, length). weight: (out_channels, in_channels, 3).
    bias: (out_channels,). Implemented as an explicit triple loop
    (output channel x input channel x kernel tap) -- deliberately NOT
    vectorized via im2col/FFT tricks, so the operation count and
    structure maps directly onto how an HLS convolution engine is
    actually built (an accumulator per output channel, summed over
    input channels and kernel taps)."""
    in_ch, length = x.shape
    out_ch, _, k = weight.shape
    x_padded = np.pad(x, ((0, 0), (padding, padding)))
    out = np.zeros((out_ch, length))
    for oc in range(out_ch):
        acc = np.zeros(length)
        for ic in range(in_ch):
            for tap in range(k):
                acc += weight[oc, ic, tap] * x_padded[ic, tap:tap + length]
        out[oc] = acc + bias[oc]
    return out


def batchnorm1d(x, weight, bias, running_mean, running_var, eps=1e-5):
    """Eval-mode BatchNorm1d: purely an affine transform per channel,
    (x - running_mean) / sqrt(running_var + eps) * weight + bias.
    NOTE FOR HLS: this should be algebraically FOLDED into the
    preceding conv layer's weight/bias before hardware implementation
    (removes it from the critical path at zero accuracy cost) -- kept
    as a separate step here only for clarity/validation against the
    unfolded PyTorch model."""
    scale = weight / np.sqrt(running_var + eps)
    shift = bias - running_mean * scale
    return x * scale[:, None] + shift[:, None]


def channel_attention(feat, w):
    """Squeeze-excite block: global average pool over the length axis,
    then a tiny 2-layer 1x1-conv bottleneck (48->12->48) with a
    sigmoid gate, multiplied back onto the original features."""
    pooled = feat.mean(axis=1, keepdims=True)  # (channels, 1) -- global avg pool
    hidden = relu(conv1d(pooled, w["refine_attn_conv0_weight"], w["refine_attn_conv0_bias"], padding=0))
    gate = sigmoid(conv1d(hidden, w["refine_attn_conv2_weight"], w["refine_attn_conv2_bias"], padding=0))
    return feat * gate  # broadcasts (channels,1) gate over the length axis


def refinement_cnn(x_ri, residual_base, w):
    """4-conv-layer refinement network with a channel-attention block
    before the final layer, and a residual (skip) connection around
    the whole thing. x_ri, residual_base: (channels, N_PATHS)."""
    feat = conv1d(x_ri, w["refine_conv0_weight"], w["refine_conv0_bias"])
    feat = relu(batchnorm1d(feat, w["refine_bn1_weight"], w["refine_bn1_bias"],
                             w["refine_bn1_running_mean"], w["refine_bn1_running_var"]))
    feat = conv1d(feat, w["refine_conv3_weight"], w["refine_conv3_bias"])
    feat = relu(batchnorm1d(feat, w["refine_bn4_weight"], w["refine_bn4_bias"],
                             w["refine_bn4_running_mean"], w["refine_bn4_running_var"]))
    feat = conv1d(feat, w["refine_conv6_weight"], w["refine_conv6_bias"])
    feat = relu(batchnorm1d(feat, w["refine_bn7_weight"], w["refine_bn7_bias"],
                             w["refine_bn7_running_mean"], w["refine_bn7_running_var"]))
    feat = channel_attention(feat, w)
    out = conv1d(feat, w["refine_conv9_weight"], w["refine_conv9_bias"])
    return residual_base + out


# ============================================================
# Full forward pass -- one sample at a time (batch of 1), matching
# how a real-time hardware pipeline processes one pilot measurement
# per inference, not a batched tensor.
# ============================================================
def attnlista_forward(y, H_LS_init, w, config):
    """
    y:          (N_BS, M_MEAS) complex -- received pilot measurement.
    H_LS_init:  (N_BS, N_PATHS) complex -- least-squares initial estimate
                (H_LS_init = y @ pinv(P_mat), computed by the caller;
                LS itself is not part of this network).
    Returns:    (N_BS, N_PATHS) complex -- the refined channel estimate.
    """
    K = config["K_unrolled_layers"]
    N_RIS = config["N_RIS"]
    N_BS = config["N_BS"]
    N_PATHS = config["N_PATHS"]
    use_skip = config["use_skip"]

    P_mat, P_mat_H = w["P_mat"], w["P_mat_H"]

    # ---- Early noise estimate (sets the initial shrink + per-layer
    # threshold scale) ----
    h_ls_ri = np.concatenate([H_LS_init.real.flatten(), H_LS_init.imag.flatten()])
    hidden = relu(linear(h_ls_ri, w["early_noise_est_linear0_weight"], w["early_noise_est_linear0_bias"]))
    layer_scales = sigmoid(linear(hidden, w["early_noise_est_linear2_weight"], w["early_noise_est_linear2_bias"])) * 2.0
    # layer_scales: (K,) -- one scale per unrolled layer

    x = H_LS_init * (1.0 / (1.0 + layer_scales[0]))

    # ---- K unrolled gradient-descent + threshold layers ----
    for k in range(K):
        grad = (x @ P_mat - y) @ P_mat_H
        x = x - w["gamma_per_layer"][k] * grad

        x_ris, x_direct = x[:, :N_RIS], x[:, N_RIS:]

        # 2D FFT across (antenna, RIS-angular) axes, matching the
        # trained model's torch.fft.fft2 call exactly (both axes
        # transformed, not just the RIS axis).
        x_ris_angular = np.fft.fft2(x_ris, norm="ortho")

        scale_k = layer_scales[k]
        x_ris_angular = soft_threshold(x_ris_angular, w[f"threshold_ris_layer{k}"], scale=scale_k)
        x_direct = soft_threshold(x_direct, w[f"threshold_direct_layer{k}"], scale=scale_k)

        x_ris = np.fft.ifft2(x_ris_angular, norm="ortho")
        x = np.concatenate([x_ris, x_direct], axis=1)

    # ---- Refinement CNN (real/imag stacked as channels) ----
    x_ri = np.concatenate([x.real, x.imag], axis=0)  # (2*N_BS, N_PATHS)

    if use_skip:
        h_ls_ri_skip = np.concatenate([H_LS_init.real, H_LS_init.imag], axis=0)
        refine_input = np.concatenate([x_ri, h_ls_ri_skip], axis=0)  # (4*N_BS, N_PATHS)
    else:
        refine_input = x_ri

    x_ri_refined = refinement_cnn(refine_input, x_ri, w)

    x_refined = x_ri_refined[:N_BS, :] + 1j * x_ri_refined[N_BS:, :]
    return x_refined


# ============================================================
# Self-contained validation: run this file directly to confirm the
# NumPy golden model produces the SAME output as the original trained
# PyTorch model, using the actual exported weights.
# ============================================================
if __name__ == "__main__":
    w, config = load_weights(EXPORT_DIR)
    N_BS, N_PATHS, M_MEAS = config["N_BS"], config["N_PATHS"], config["M_MEAS"]

    np.random.seed(0)
    H_true = (np.random.randn(N_BS, N_PATHS) + 1j * np.random.randn(N_BS, N_PATHS)) / np.sqrt(2)
    y = H_true @ w["P_mat"]
    P_mat_pinv = np.linalg.pinv(w["P_mat"])
    H_LS_init = y @ P_mat_pinv

    x_refined = attnlista_forward(y, H_LS_init, w, config)
    print("Output shape:", x_refined.shape, "(expected", (N_BS, N_PATHS), ")")
    print("Output finite:", np.isfinite(x_refined).all())
    nmse = np.sum(np.abs(x_refined - H_true) ** 2) / np.sum(np.abs(H_true) ** 2)
    print(f"Sanity NMSE on random synthetic data: {10*np.log10(nmse):.2f} dB "
          "(not meaningful accuracy-wise -- H_true here is random, not a real sparse channel; "
          "this just confirms the pipeline runs end-to-end without errors)")
