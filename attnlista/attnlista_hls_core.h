#ifndef ATTNLISTA_HLS_CORE_H
#define ATTNLISTA_HLS_CORE_H

#include "attnlista_hls.h"
#include <hls_math.h>

const signal_t twiddle_re[32] = {
1.0, 0.9951847266721969, 0.9807852804032304, 0.9569403357322088, 0.9238795325112867, 0.881921264348355, 0.8314696123025452, 0.773010453362737, 0.7071067811865476, 0.6343932841636455, 0.5555702330196023, 0.4713967368259978, 0.3826834323650898, 0.2902846772544623, 0.1950903220161283, 0.09801714032956077, 0.0, -0.09801714032956061, -0.1950903220161286, -0.2902846772544621, -0.3826834323650896, -0.4713967368259979, -0.5555702330196022, -0.6343932841636454, -0.7071067811865475, -0.7730104533627369, -0.8314696123025453, -0.881921264348355, -0.9238795325112867, -0.9569403357322088, -0.9807852804032304, -0.9951847266721969
};

const signal_t twiddle_im[32] = {
0.0, -0.0980171403295606, -0.1950903220161282, -0.2902846772544623, -0.3826834323650897, -0.4713967368259976, -0.555570233019602, -0.6343932841636453, -0.7071067811865475, -0.7730104533627369, -0.8314696123025453, -0.8819212643483549, -0.9238795325112867, -0.9569403357322089, -0.9807852804032304, -0.9951847266721969, -1.0, -0.9951847266721969, -0.9807852804032304, -0.956940335732209, -0.9238795325112868, -0.881921264348355, -0.8314696123025455, -0.773010453362737, -0.7071067811865477, -0.6343932841636455, -0.5555702330196023, -0.471396736825998, -0.3826834323650898, -0.2902846772544626, -0.1950903220161286, -0.09801714032956083
};

template<int ROWS, int COLS, int LOG2_COLS>
void radix2_fft_rows(csignal_t data[ROWS][COLS], bool inverse) {
#pragma HLS INLINE off
    for (int b = 0; b < ROWS; b++) {
        for (int i = 1, j = 0; i < COLS; i++) {
            int bit = COLS >> 1;
            for (; j & bit; bit >>= 1) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=6 avg=3
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                csignal_t temp = data[b][i];
                data[b][i] = data[b][j];
                data[b][j] = temp;
            }
        }
    }

    for (int stage = 1; stage <= LOG2_COLS; stage++) {
        int m = 1 << stage;
        int half_m = m >> 1;
        int twiddle_step = 32 / half_m;
        
        for (int k = 0; k < half_m; k++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32 avg=16
            int twiddle_idx = k * twiddle_step;
            signal_t t_re = twiddle_re[twiddle_idx];
            signal_t t_im = inverse ? signal_t(-twiddle_im[twiddle_idx]) : twiddle_im[twiddle_idx];
            csignal_t w(t_re, t_im);
            
            for (int b = 0; b < ROWS; b++) {
                for (int i = k; i < COLS; i += m) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32 avg=16
#pragma HLS PIPELINE II=1

                    int j = i + half_m;
                    csignal_t t = w * data[b][j];
#pragma HLS BIND_OP variable=t op=mul impl=dsp latency=2
                    csignal_t u = data[b][i];
                    data[b][i] = u + t;
                    data[b][j] = u - t;
                }
            }
        }
    }

    signal_t scale = signal_t(1.0 / hls::sqrt((double)COLS));
    for (int b = 0; b < ROWS; b++) {
        for (int i = 0; i < COLS; i++) {
#pragma HLS PIPELINE II=1

            signal_t re = data[b][i].real();
            signal_t im = data[b][i].imag();
            data[b][i] = csignal_t(re * scale, im * scale);
        }
    }
}

inline void lista_layer(
    csignal_t x[N_BS][N_PATHS],
    const csignal_t y[N_BS][M_MEAS],
    const pmat_t P_mat[N_PATHS][M_MEAS],
    const pmat_t P_mat_H[M_MEAS][N_PATHS],
    weight_t gamma_k,
    const weight_t threshold_ris[64],
    weight_t threshold_direct,
    signal_t scale_k,
    int layer_idx
) {
#pragma HLS INLINE off
    csignal_t residual[N_BS][M_MEAS];
    csignal_t grad[N_BS][N_PATHS];
#pragma HLS ARRAY_PARTITION variable=residual complete dim=2
#pragma HLS ARRAY_PARTITION variable=grad cyclic factor=16 dim=2

    for (int b = 0; b < N_BS; b++) {
        for (int m = 0; m < M_MEAS; m++) {
#pragma HLS PIPELINE II=1

            csignal_t acc(0, 0);
            for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1
                if (P_mat[p][m] > 0) acc += x[b][p];
                else acc -= x[b][p];
            }
            residual[b][m] = acc - y[b][m];
        }
    }

    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1

            csignal_t acc(0, 0);
            for (int m = 0; m < M_MEAS; m++) {
#pragma HLS PIPELINE II=1
                if (P_mat_H[m][p] > 0) acc += residual[b][m];
                else acc -= residual[b][m];
            }
            grad[b][p] = acc;
        }
    }

    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_PATHS; p++) {
#pragma HLS PIPELINE II=1

            signal_t re = x[b][p].real() - gamma_k * grad[b][p].real();
            signal_t im = x[b][p].imag() - gamma_k * grad[b][p].imag();
            x[b][p] = csignal_t(re, im);
        }
    }

    csignal_t x_ris[N_BS][N_RIS];
#pragma HLS ARRAY_PARTITION variable=x_ris cyclic factor=16 dim=2
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            x_ris[b][p] = x[b][p];
        }
    }

    radix2_fft_rows<N_BS, N_RIS, 6>(x_ris, false);
    
    csignal_t x_ris_T[N_RIS][N_BS];
#pragma HLS ARRAY_PARTITION variable=x_ris_T cyclic factor=16 dim=2
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            x_ris_T[p][b] = x_ris[b][p];
        }
    }
    radix2_fft_rows<N_RIS, N_BS, 4>(x_ris_T, false);
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            x_ris[b][p] = x_ris_T[p][b];
        }
    }

#pragma HLS ALLOCATION operation instances=dsqrt limit=1
#pragma HLS ALLOCATION operation instances=ddiv limit=1
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            signal_t re = x_ris[b][p].real();
            signal_t im = x_ris[b][p].imag();
            signal_t mag2 = re*re + im*im;
            signal_t mag = hls::sqrt((double)mag2);
            signal_t t_val = threshold_ris[p];
            if (t_val < 0) t_val = -t_val;
            signal_t t = t_val * scale_k;
            if (mag > t) {
                signal_t s = (mag - t) / mag;
                x_ris[b][p] = csignal_t(re * s, im * s);
            } else {
                x_ris[b][p] = csignal_t(0, 0);
            }
        }
    }

    for (int b = 0; b < N_BS; b++) {

        signal_t re = x[b][N_RIS].real();
        signal_t im = x[b][N_RIS].imag();
        signal_t mag2 = re*re + im*im;
        signal_t mag = hls::sqrt((double)mag2);
        signal_t t_val = threshold_direct;
        if (t_val < 0) t_val = -t_val;
        signal_t t = t_val * scale_k;
        if (mag > t) {
            signal_t s = (mag - t) / mag;
            x[b][N_RIS] = csignal_t(re * s, im * s);
        } else {
            x[b][N_RIS] = csignal_t(0, 0);
        }
    }

    radix2_fft_rows<N_BS, N_RIS, 6>(x_ris, true);
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            x_ris_T[p][b] = x_ris[b][p];
        }
    }
    radix2_fft_rows<N_RIS, N_BS, 4>(x_ris_T, true);
    for (int b = 0; b < N_BS; b++) {
        for (int p = 0; p < N_RIS; p++) {
#pragma HLS PIPELINE II=1

            x[b][p] = x_ris_T[p][b];
        }
    }
}

#endif
 

