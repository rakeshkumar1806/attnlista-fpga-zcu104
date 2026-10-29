import numpy as np
from attnlista_golden_model import load_weights
import torch

w, _ = load_weights('hls_export')
y = np.load('tb_y_input.npy')
h_init = y @ np.linalg.pinv(w['P_mat'])
h_ri = np.concatenate([h_init.real.flatten(), h_init.imag.flatten()])

w0 = np.load('hls_export/early_noise_est_linear0_weight.npy')
b0 = np.load('hls_export/early_noise_est_linear0_bias.npy')
hidden = np.maximum(0, h_ri @ w0.T + b0)

w2 = np.load('hls_export/early_noise_est_linear2_weight.npy')
b2 = np.load('hls_export/early_noise_est_linear2_bias.npy')
scales = 1 / (1 + np.exp(-(hidden @ w2.T + b2))) * 2.0

x = h_init / (1 + scales[0])
print('Before grad x[0,0]:', x[0, 0])

res = x @ w['P_mat'] - y
print('residual[0][0]:', res[0, 0])

grad = res @ w['P_mat_H']
print('grad[0][0]:', grad[0, 0])

x = x - w['gamma_per_layer'][0] * grad
print('After grad x[0,0]:', x[0, 0])

x_ris = x[:, :64]
ang = np.fft.fft2(x_ris, norm='ortho')

def st(x, th, sc):
    mag = torch.abs(x)
    th_eff = torch.abs(th) * sc
    return x * torch.relu(mag - th_eff) / (mag + 1e-4)

ang = st(torch.tensor(ang), torch.tensor(w['threshold_ris_layer0']), scales[0]).numpy()
print('After thresh x_ris_angular[0][0]:', ang[0, 0])
