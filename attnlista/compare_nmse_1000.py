import numpy as np
from attnlista_golden_model import load_weights, attnlista_forward

w, config = load_weights('hls_export')
N_BS, N_PATHS = config['N_BS'], config['N_PATHS']

y = np.load('C:/Users/jbvrr/Downloads/deepmimo_tb_y_input.npy').astype(np.float32)
y = y[0::2] + 1j * y[1::2]
y = y.reshape(1000, 16, 16)

h_true = np.load('C:/Users/jbvrr/Downloads/deepmimo_tb_h_true_expected.npy').astype(np.float32)
h_true = h_true[0::2] + 1j * h_true[1::2]
h_true = h_true.reshape(1000, 16, 65)

hw_nmses = []
with open('hw_nmse_1000.txt', 'r') as f:
    hw_nmses = [float(line.strip()) for line in f]

diffs = []
for t in range(1000):
    H_LS_init = y[t] @ np.linalg.pinv(w['P_mat'])
    x_refined = attnlista_forward(y[t], H_LS_init, w, config)
    diff_tensor = x_refined - h_true[t]
    py_nmse = 10 * np.log10(np.sum(np.abs(diff_tensor)**2) / np.sum(np.abs(h_true[t])**2))
    
    diffs.append(abs(hw_nmses[t] - py_nmse))

diffs = np.array(diffs)
print(f'Max absolute difference: {np.max(diffs):.6f} dB')
print(f'Mean absolute difference: {np.mean(diffs):.6f} dB')
print(f'Standard deviation of difference: {np.std(diffs):.6f} dB')
