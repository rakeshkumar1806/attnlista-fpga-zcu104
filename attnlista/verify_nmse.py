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

py_nmses = []
for t in range(1000):
    H_LS_init = y[t] @ np.linalg.pinv(w['P_mat'])
    x_refined = attnlista_forward(y[t], H_LS_init, w, config)
    
    diff = x_refined - h_true[t]
    nmse = 10 * np.log10(np.sum(np.abs(diff)**2) / np.sum(np.abs(h_true[t])**2))
    py_nmses.append(nmse)

py_nmses = np.array(py_nmses)
print(f'Python Model Average NMSE: {np.mean(py_nmses):.2f} dB')
print(f'Python Model Passed (<-10dB): {np.sum(py_nmses < -10)}')
print(f'Python Model Failed: {np.sum(py_nmses >= -10)}')
