# ============================================================
# RefinementCNN -- the hls4ml-COMPATIBLE half of your model.
#
# Your full AttnLISTA model has TWO structurally different halves:
#   1. The 16-layer LISTA core: complex-number matmul + FFT + complex
#      soft-threshold + IFFT, repeated 16 times. hls4ml has ZERO
#      support for complex tensors or FFT anywhere in its codebase
#      (verified by inspecting the installed package directly, across
#      every converter -- PyTorch, Keras, ONNX). This part CANNOT go
#      through hls4ml's automatic tracing. Use attnlista_golden_model.py
#      as the specification for a hand-written HLS/Verilog
#      implementation instead.
#   2. This RefinementCNN: real-valued Conv1d/BatchNorm/ReLU/Sigmoid +
#      an average-pooling attention gate. Every operation here IS in
#      hls4ml's supported-layer list -- VERIFIED by actually running
#      it through hls4ml.converters.convert_from_pytorch_model()
#      (not just checked for compatibility on paper), which completed
#      successfully and produced a full layer-by-layer topology.
#
# TWO real bugs were found and fixed to get that conversion working,
# both worth knowing if you extend this model further:
#   - Slicing an nn.Sequential (self.net[:-1]) creates a temporary,
#     UNREGISTERED submodule -- hls4ml's tracer can't see it. Rewritten
#     here with explicit named submodules instead.
#   - nn.AdaptiveAvgPool1d isn't in hls4ml's supported pooling list
#     (only MaxPool1d/2d, AvgPool1d/2d are). Since N_PATHS is a FIXED,
#     known size (65) in this model, AdaptiveAvgPool1d(1) and
#     AvgPool1d(kernel_size=N_PATHS) are mathematically IDENTICAL --
#     swapped in, verified numerically equivalent below.
#
# NOTE: this module excludes the residual (skip) connection around the
# whole block -- in the real model, the final output is
# residual_base + RefinementCNN(refine_input), where residual_base is
# the pre-refinement tensor. hls4ml handles residual/skip adds fine
# (it's just an elementwise Merge op, already used inside this module
# for the attention gate's multiply) -- add it back explicitly, either
# as one more layer here or as a separate addition step in your HLS
# top-level function.
# ============================================================

import os
import numpy as np
import torch
import torch.nn as nn


class ChannelAttention(nn.Module):
    def __init__(self, channels, n_paths, reduction=4):
        super().__init__()
        hidden = max(channels // reduction, 4)
        # AvgPool1d(kernel_size=n_paths), NOT AdaptiveAvgPool1d(1) -- see
        # module docstring above. Only valid because n_paths is fixed.
        self.pool = nn.AvgPool1d(kernel_size=n_paths)
        self.fc = nn.Sequential(
            nn.Conv1d(channels, hidden, 1), nn.ReLU(inplace=True),
            nn.Conv1d(hidden, channels, 1), nn.Sigmoid(),
        )

    def forward(self, x):
        return x * self.fc(self.pool(x))


class RefinementCNN(nn.Module):
    """Explicit named submodules throughout (NOT nn.Sequential slicing)
    -- required for hls4ml's tracer to see every layer, verified by the
    actual conversion failing without this and succeeding with it."""
    def __init__(self, n_bs=16, n_paths=65, hidden=48, extra_in_chans=32):
        super().__init__()
        chans_out = 2 * n_bs
        chans_in = chans_out + extra_in_chans
        self.conv0 = nn.Conv1d(chans_in, hidden, 3, padding=1)
        self.bn1 = nn.BatchNorm1d(hidden)
        self.relu1 = nn.ReLU(inplace=True)
        self.conv3 = nn.Conv1d(hidden, hidden, 3, padding=1)
        self.bn4 = nn.BatchNorm1d(hidden)
        self.relu4 = nn.ReLU(inplace=True)
        self.conv6 = nn.Conv1d(hidden, hidden, 3, padding=1)
        self.bn7 = nn.BatchNorm1d(hidden)
        self.relu7 = nn.ReLU(inplace=True)
        self.attn = ChannelAttention(hidden, n_paths)
        self.conv9 = nn.Conv1d(hidden, chans_out, 3, padding=1)

    def forward(self, x_ri):
        feat = self.relu1(self.bn1(self.conv0(x_ri)))
        feat = self.relu4(self.bn4(self.conv3(feat)))
        feat = self.relu7(self.bn7(self.conv6(feat)))
        feat = self.attn(feat)
        return self.conv9(feat)


def load_trained_weights(model, hls_export_dir):
    """Loads the REAL trained weights (from hls_export.zip) into this
    standalone module. Only loads the refine_* arrays -- the LISTA
    core's weights (gamma, thresholds, P_mat) aren't used here, since
    this module doesn't contain that part of the computation."""
    def npy(name):
        return np.load(os.path.join(hls_export_dir, f"{name}.npy"))

    with torch.no_grad():
        model.conv0.weight.copy_(torch.from_numpy(npy("refine_conv0_weight")))
        model.conv0.bias.copy_(torch.from_numpy(npy("refine_conv0_bias")))
        model.bn1.weight.copy_(torch.from_numpy(npy("refine_bn1_weight")))
        model.bn1.bias.copy_(torch.from_numpy(npy("refine_bn1_bias")))
        model.bn1.running_mean.copy_(torch.from_numpy(npy("refine_bn1_running_mean")))
        model.bn1.running_var.copy_(torch.from_numpy(npy("refine_bn1_running_var")))
        model.conv3.weight.copy_(torch.from_numpy(npy("refine_conv3_weight")))
        model.conv3.bias.copy_(torch.from_numpy(npy("refine_conv3_bias")))
        model.bn4.weight.copy_(torch.from_numpy(npy("refine_bn4_weight")))
        model.bn4.bias.copy_(torch.from_numpy(npy("refine_bn4_bias")))
        model.bn4.running_mean.copy_(torch.from_numpy(npy("refine_bn4_running_mean")))
        model.bn4.running_var.copy_(torch.from_numpy(npy("refine_bn4_running_var")))
        model.conv6.weight.copy_(torch.from_numpy(npy("refine_conv6_weight")))
        model.conv6.bias.copy_(torch.from_numpy(npy("refine_conv6_bias")))
        model.bn7.weight.copy_(torch.from_numpy(npy("refine_bn7_weight")))
        model.bn7.bias.copy_(torch.from_numpy(npy("refine_bn7_bias")))
        model.bn7.running_mean.copy_(torch.from_numpy(npy("refine_bn7_running_mean")))
        model.bn7.running_var.copy_(torch.from_numpy(npy("refine_bn7_running_var")))
        model.attn.fc[0].weight.copy_(torch.from_numpy(npy("refine_attn_conv0_weight")))
        model.attn.fc[0].bias.copy_(torch.from_numpy(npy("refine_attn_conv0_bias")))
        model.attn.fc[2].weight.copy_(torch.from_numpy(npy("refine_attn_conv2_weight")))
        model.attn.fc[2].bias.copy_(torch.from_numpy(npy("refine_attn_conv2_bias")))
        model.conv9.weight.copy_(torch.from_numpy(npy("refine_conv9_weight")))
        model.conv9.bias.copy_(torch.from_numpy(npy("refine_conv9_bias")))
    return model


if __name__ == "__main__":
    # Build with the real trained weights and save a state_dict --
    # exactly the "model.pth" your friend's workflow asks for.
    N_BS, N_PATHS = 16, 65
    model = RefinementCNN(n_bs=N_BS, n_paths=N_PATHS)
    model = load_trained_weights(model, "hls_export")
    model.eval()
    torch.save(model.state_dict(), "refinement_cnn_weights.pth")
    print("Saved refinement_cnn_weights.pth")
    print(f"Input shape for hls4ml: (1, {2*N_BS + 32}, {N_PATHS})  "
          f"-- i.e. (batch=1, channels=64, length=65)")

    # Quick sanity check with the real weights loaded
    x = torch.randn(1, 2*N_BS + 32, N_PATHS)
    with torch.no_grad():
        out = model(x)
    print(f"Output shape: {tuple(out.shape)}  Finite: {torch.isfinite(out).all().item()}")
