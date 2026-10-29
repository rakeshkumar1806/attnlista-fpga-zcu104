import numpy as np
from attnlista_golden_model import load_weights

w, config = load_weights("hls_export")
N_BS, N_PATHS, M_MEAS = config["N_BS"], config["N_PATHS"], config["M_MEAS"]

np.random.seed(0)
H_true = (np.random.randn(N_BS, N_PATHS) + 1j * np.random.randn(N_BS, N_PATHS)) / np.sqrt(2)
y = H_true @ w["P_mat"]
np.save("tb_y_input.npy", y)
with open("tb_y_input.txt", "w") as f:
    for i in range(N_BS):
        for j in range(M_MEAS):
            f.write(f"{y[i,j].real} {y[i,j].imag}\n")

H_LS_init = y @ np.linalg.pinv(w["P_mat"])
K = config["K_unrolled_layers"]
N_RIS = config["N_RIS"]
use_skip = config["use_skip"]
P_mat, P_mat_H = w["P_mat"], w["P_mat_H"]

def linear(x, weight, bias): return x @ weight.T + bias
def relu(x): return np.maximum(x, 0.0)
def sigmoid(x): return 1.0 / (1.0 + np.exp(-x))
def soft_threshold(x, theta, scale):
    theta_eff = np.abs(theta) * scale
    mag = np.abs(x) + 1e-12
    mag_thresh = np.maximum(mag - theta_eff, 0.0)
    return x * (mag_thresh / mag)

h_ls_ri = np.concatenate([H_LS_init.real.flatten(), H_LS_init.imag.flatten()])
hidden = relu(linear(h_ls_ri, w["early_noise_est_linear0_weight"], w["early_noise_est_linear0_bias"]))
layer_scales = sigmoid(linear(hidden, w["early_noise_est_linear2_weight"], w["early_noise_est_linear2_bias"])) * 2.0

x = H_LS_init * (1.0 / (1.0 + layer_scales[0]))
for k in range(K):
    grad = (x @ P_mat - y) @ P_mat_H
    x = x - w["gamma_per_layer"][k] * grad
    x_ris, x_direct = x[:, :N_RIS], x[:, N_RIS:]
    x_ris_angular = np.fft.fft2(x_ris, norm="ortho")
    scale_k = layer_scales[k]
    x_ris_angular = soft_threshold(x_ris_angular, w[f"threshold_ris_layer{k}"], scale=scale_k)
    x_direct = soft_threshold(x_direct, w[f"threshold_direct_layer{k}"], scale=scale_k)
    x_ris = np.fft.ifft2(x_ris_angular, norm="ortho")
    x = np.concatenate([x_ris, x_direct], axis=1)

np.save("tb_h_lista_expected.npy", x)
with open("tb_h_lista_expected.txt", "w") as f:
    for i in range(N_BS):
        for j in range(N_PATHS):
            f.write(f"{x[i,j].real} {x[i,j].imag}\n")
