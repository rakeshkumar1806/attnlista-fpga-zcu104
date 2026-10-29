#include "attnlista_hls.h"
#include "attnlista_hls_core.h"
#include "attnlista_weights.h"
#include <hls_math.h>
#include <iostream>

// Math utilities
template <typename T>
T relu(T x) {
#pragma HLS INLINE
    return x > T(0) ? x : T(0);
}

template <typename T>
T sigmoid(T x) {
#pragma HLS INLINE
    return T(1.0) / (T(1.0) + T(hls::exp(-x)));
}

// -------------------------------------------------------------
// The Top-Level Function
// -------------------------------------------------------------
void attnlista_top(
    const csignal_t y[N_BS][M_MEAS],
    csignal_t h_ls_init_out[N_BS][N_PATHS],
    csignal_t h_out[N_BS][N_PATHS]  // Output: the output of LISTA core before RefinementCNN
) {
#pragma HLS INTERFACE mode=axis port=y
#pragma HLS INTERFACE mode=axis port=h_ls_init_out
#pragma HLS INTERFACE mode=axis port=h_out
#pragma HLS INTERFACE mode=s_axilite port=return
    csignal_t y_local[N_BS][M_MEAS];
    for (int b = 0; b < N_BS; b++) {
        for (int m = 0; m < M_MEAS; m++) {
#pragma HLS PIPELINE II=1

            y_local[b][m] = y[b][m];
        }
    }


    // 1. Initial Least Squares Estimate: H_LS_init = y @ P_mat_pinv
    csignal_t h_ls_init[N_BS][N_PATHS];
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1

            csignal_t acc(0, 0);
            for (int m = 0; m < M_MEAS; m++) {
#pragma HLS PIPELINE II=1
                acc += y_local[b][m] * csignal_t(signal_t(P_mat_pinv[m][p].real()), signal_t(P_mat_pinv[m][p].imag()));
            }
            h_ls_init[b][p] = acc;
        }
    }

    // 2. Early noise estimate
    signal_t h_ls_ri[2080];
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1

            int idx = b * N_PATHS + p;
            h_ls_ri[idx] = h_ls_init[b][p].real();
            h_ls_ri[idx + N_BS * N_PATHS] = h_ls_init[b][p].imag();
        }
    }

    signal_t hidden[32];
    for (int i = 0; i < 32; i++) {

        signal_t acc = ene_l0_b[i];
        for (int j = 0; j < 2080; j++) {
            acc += signal_t(ene_l0_w[i][j]) * h_ls_ri[j];
        }
        hidden[i] = relu(acc);
    }

    signal_t layer_scales[K_LAYERS];
    for (int i = 0; i < K_LAYERS; i++) {
#pragma HLS PIPELINE II=1

        signal_t acc = ene_l2_b[i];
        for (int j = 0; j < 32; j++) {
            acc += signal_t(ene_l2_w[i][j]) * hidden[j];
        }
        layer_scales[i] = sigmoid(acc) * signal_t(2.0);
    }

    // 3. Initialize x = H_LS_init * (1.0 / (1.0 + layer_scales[0]))
    csignal_t x[N_BS][N_PATHS];
    signal_t init_scale = signal_t(1.0) / (signal_t(1.0) + signal_t(layer_scales[0]));
#ifndef __SYNTHESIS__
    std::cout << "layer_scales[0] = " << (double)layer_scales[0] << std::endl;
    std::cout << "h_ls_init[0][0] = " << (double)h_ls_init[0][0].real() << " + " << (double)h_ls_init[0][0].imag() << "j" << std::endl;
#endif
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1

            x[b][p].real(h_ls_init[b][p].real() * init_scale);
            x[b][p].imag(h_ls_init[b][p].imag() * init_scale);
        }
    }

    // 4. K_LAYERS LISTA Core calls
    for (int k = 0; k < K_LAYERS; k++) {
        lista_layer(x, y_local, P_mat, P_mat_H, gamma_per_layer[k], threshold_ris[k], threshold_direct[k], layer_scales[k], k);
    }

    // 5. Output write-back (handoff point)
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1
            h_ls_init_out[b][p] = h_ls_init[b][p];
            h_out[b][p] = x[b][p];
        }
    }
}
