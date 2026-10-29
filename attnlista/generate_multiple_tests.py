import numpy as np
from attnlista_golden_model import load_weights, attnlista_forward

w, config = load_weights("hls_export")
N_BS, N_PATHS, M_MEAS = config["N_BS"], config["N_PATHS"], config["M_MEAS"]

NUM_TESTS = 10

with open("test_vectors_multi.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write(f"#define NUM_TESTS {NUM_TESTS}\n\n")
    
    f.write("const float tb_y_inputs[NUM_TESTS][512] = {\n")
    for t in range(NUM_TESTS):
        np.random.seed(t) # deterministic
        H_true = (np.random.randn(N_BS, N_PATHS) + 1j * np.random.randn(N_BS, N_PATHS)) / np.sqrt(2)
        y = H_true @ w["P_mat"]
        
        f.write("  { ")
        vals = [f"{y[i,j].real}f, {y[i,j].imag}f" for i in range(N_BS) for j in range(M_MEAS)]
        f.write(", ".join(vals))
        f.write("  },\n")
    f.write("};\n\n")
    
    f.write("const float tb_cnn_expecteds[NUM_TESTS][2080] = {\n")
    for t in range(NUM_TESTS):
        np.random.seed(t)
        H_true = (np.random.randn(N_BS, N_PATHS) + 1j * np.random.randn(N_BS, N_PATHS)) / np.sqrt(2)
        y = H_true @ w["P_mat"]
        H_LS_init = y @ np.linalg.pinv(w["P_mat"])
        
        x_refined = attnlista_forward(y, H_LS_init, w, config)
        
        # x_refined is shape (16, 65) complex.
        # Hardware outputs: for each path l=0..64, it outputs 16 real values then 16 imag values.
        # This is exactly equivalent to: x_ri = concat([real, imag], axis=0) -> (32, 65).
        # Then transpose to (65, 32) and flatten.
        x_ri = np.concatenate([x_refined.real, x_refined.imag], axis=0)
        flat_expected = x_ri.T.flatten()
        
        f.write("  { ")
        vals = [f"{v}f" for v in flat_expected]
        f.write(", ".join(vals))
        f.write("  },\n")
    f.write("};\n")

print("Generated test_vectors_multi.h cleanly.")
