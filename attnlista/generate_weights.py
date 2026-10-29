import numpy as np
import os
import json

hls_export_dir = "hls_export"
output_header = "attnlista_weights.h"

# Read P_mat
P_mat_real = np.load(os.path.join(hls_export_dir, "P_mat_real.npy"))
P_mat_imag = np.load(os.path.join(hls_export_dir, "P_mat_imag.npy"))
P_mat = P_mat_real + 1j * P_mat_imag
P_mat_H_real = np.load(os.path.join(hls_export_dir, "P_mat_H_real.npy"))
P_mat_H_imag = np.load(os.path.join(hls_export_dir, "P_mat_H_imag.npy"))
P_mat_pinv = np.linalg.pinv(P_mat)
P_mat_pinv_real = np.real(P_mat_pinv)
P_mat_pinv_imag = np.imag(P_mat_pinv)

gamma = np.load(os.path.join(hls_export_dir, "gamma_per_layer.npy"))

ene_l0_w = np.load(os.path.join(hls_export_dir, "early_noise_est_linear0_weight.npy"))
ene_l0_b = np.load(os.path.join(hls_export_dir, "early_noise_est_linear0_bias.npy"))
ene_l2_w = np.load(os.path.join(hls_export_dir, "early_noise_est_linear2_weight.npy"))
ene_l2_b = np.load(os.path.join(hls_export_dir, "early_noise_est_linear2_bias.npy"))

with open(output_header, "w") as f:
    f.write("#ifndef ATTNLISTA_WEIGHTS_H\n")
    f.write("#define ATTNLISTA_WEIGHTS_H\n\n")
    f.write("#include \"attnlista_hls.h\"\n\n")

    # P_mat
    f.write("const pmat_t P_mat[N_PATHS][M_MEAS] = {\n")
    for i in range(P_mat_real.shape[0]):
        f.write("    {")
        for j in range(P_mat_real.shape[1]):
            f.write(f"pmat_t({int(P_mat_real[i,j])})")
            if j < P_mat_real.shape[1] - 1:
                f.write(", ")
        f.write("}")
        if i < P_mat_real.shape[0] - 1:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")

    # P_mat_H
    f.write("const pmat_t P_mat_H[M_MEAS][N_PATHS] = {\n")
    for i in range(P_mat_H_real.shape[0]):
        f.write("    {")
        for j in range(P_mat_H_real.shape[1]):
            f.write(f"pmat_t({int(P_mat_H_real[i,j])})")
            if j < P_mat_H_real.shape[1] - 1:
                f.write(", ")
        f.write("}")
        if i < P_mat_H_real.shape[0] - 1:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")
    
    # P_mat_pinv
    # wait, P_mat_pinv values are not necessarily exact +/-1. 
    # we need a typedef for it, maybe weight_t or signal_t. 
    f.write("const std::complex<weight_t> P_mat_pinv[M_MEAS][N_PATHS] = {\n")
    for i in range(P_mat_pinv_real.shape[0]):
        f.write("    {")
        for j in range(P_mat_pinv_real.shape[1]):
            f.write(f"std::complex<weight_t>({P_mat_pinv_real[i,j]}, {P_mat_pinv_imag[i,j]})")
            if j < P_mat_pinv_real.shape[1] - 1:
                f.write(", ")
        f.write("}")
        if i < P_mat_pinv_real.shape[0] - 1:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")

    # Gamma array
    f.write("const weight_t gamma_per_layer[K_LAYERS] = {\n")
    f.write("    " + ", ".join([str(g) for g in gamma.flatten()]) + "\n")
    f.write("};\n\n")

    # Threshold RIS array of arrays
    f.write("const weight_t threshold_ris[K_LAYERS][N_RIS] = {\n")
    for k in range(16):
        t_ris = np.load(os.path.join(hls_export_dir, f"threshold_ris_layer{k}.npy"))
        f.write("    {")
        f.write(", ".join([str(t) for t in t_ris.flatten()]))
        f.write("}")
        if k < 15:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")

    # Threshold Direct array
    f.write("const weight_t threshold_direct[K_LAYERS] = {\n")
    for k in range(16):
        t_dir = np.load(os.path.join(hls_export_dir, f"threshold_direct_layer{k}.npy"))
        f.write("    " + str(t_dir.flatten()[0]))
        if k < 15:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")
    
    def write_2d_array(name, data):
        f.write(f"const weight_t {name}[{data.shape[0]}][{data.shape[1]}] = {{\n")
        for i in range(data.shape[0]):
            f.write("    {")
            f.write(", ".join([str(x) for x in data[i]]))
            f.write("}")
            if i < data.shape[0] - 1:
                f.write(",")
            f.write("\n")
        f.write("};\n\n")
        
    def write_1d_array(name, data):
        f.write(f"const weight_t {name}[{data.shape[0]}] = {{\n")
        f.write("    " + ", ".join([str(x) for x in data.flatten()]) + "\n")
        f.write("};\n\n")
        
    write_2d_array("ene_l0_w", ene_l0_w)
    write_1d_array("ene_l0_b", ene_l0_b)
    write_2d_array("ene_l2_w", ene_l2_w)
    write_1d_array("ene_l2_b", ene_l2_b)

    f.write("#endif // ATTNLISTA_WEIGHTS_H\n")
