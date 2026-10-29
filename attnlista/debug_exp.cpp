// Debug testbench: extract intermediate values from the streaming CNN
// to find exactly where the -6.5 dB divergence originates.
#include <iostream>
#include <fstream>
#include <cmath>
#include <ap_fixed.h>
#include <hls_math.h>
#include "cnn_weights.h"

using namespace std;
typedef ap_fixed<32, 16> fx_t;

int main() {
    // Load input
    float raw_in[64][65];
    ifstream fin("tb_cnn_input.txt");
    for (int c = 0; c < 64; c++)
        for (int l = 0; l < 65; l++)
            fin >> raw_in[c][l];
    fin.close();

    fx_t in64[64][65];
    for (int c = 0; c < 64; c++)
        for (int l = 0; l < 65; l++)
            in64[c][l] = fx_t(raw_in[c][l]);

    // Conv0: 64->48, ReLU -- exact same code as cnn_hls_core.cpp
    fx_t buf48a[48][65];
    for (int oc = 0; oc < 48; oc++) {
        for (int l = 0; l < 65; l++) {
            fx_t acc = fx_t(cnn_b0[oc]);
            for (int ic = 0; ic < 64; ic++) {
                fx_t v0 = (l-1 >= 0)   ? in64[ic][l-1] : fx_t(0);
                fx_t v1 = in64[ic][l];
                fx_t v2 = (l+1 < 65)   ? in64[ic][l+1] : fx_t(0);
                acc += v0 * fx_t(cnn_w0[oc][ic][0])
                     + v1 * fx_t(cnn_w0[oc][ic][1])
                     + v2 * fx_t(cnn_w0[oc][ic][2]);
            }
            buf48a[oc][l] = (acc > fx_t(0)) ? acc : fx_t(0);
        }
    }
    cout << "Conv0 output[0][0..4]: ";
    for (int l = 0; l < 5; l++) cout << (float)buf48a[0][l] << " ";
    cout << endl;

    // Conv3
    fx_t buf48b[48][65];
    for (int oc = 0; oc < 48; oc++) {
        for (int l = 0; l < 65; l++) {
            fx_t acc = fx_t(cnn_b3[oc]);
            for (int ic = 0; ic < 48; ic++) {
                fx_t v0 = (l-1 >= 0)   ? buf48a[ic][l-1] : fx_t(0);
                fx_t v1 = buf48a[ic][l];
                fx_t v2 = (l+1 < 65)   ? buf48a[ic][l+1] : fx_t(0);
                acc += v0 * fx_t(cnn_w3[oc][ic][0])
                     + v1 * fx_t(cnn_w3[oc][ic][1])
                     + v2 * fx_t(cnn_w3[oc][ic][2]);
            }
            buf48b[oc][l] = (acc > fx_t(0)) ? acc : fx_t(0);
        }
    }
    cout << "Conv3 output[0][0..4]: ";
    for (int l = 0; l < 5; l++) cout << (float)buf48b[0][l] << " ";
    cout << endl;

    // Conv6
    for (int oc = 0; oc < 48; oc++) {
        for (int l = 0; l < 65; l++) {
            fx_t acc = fx_t(cnn_b6[oc]);
            for (int ic = 0; ic < 48; ic++) {
                fx_t v0 = (l-1 >= 0)   ? buf48b[ic][l-1] : fx_t(0);
                fx_t v1 = buf48b[ic][l];
                fx_t v2 = (l+1 < 65)   ? buf48b[ic][l+1] : fx_t(0);
                acc += v0 * fx_t(cnn_w6[oc][ic][0])
                     + v1 * fx_t(cnn_w6[oc][ic][1])
                     + v2 * fx_t(cnn_w6[oc][ic][2]);
            }
            buf48a[oc][l] = (acc > fx_t(0)) ? acc : fx_t(0);
        }
    }
    cout << "Conv6 output[0][0..4]: ";
    for (int l = 0; l < 5; l++) cout << (float)buf48a[0][l] << " ";
    cout << endl;

    // SE: GlobalAvgPool
    fx_t sum[48];
    for (int c = 0; c < 48; c++) {
        sum[c] = 0;
        for (int l = 0; l < 65; l++) sum[c] += buf48a[c][l];
    }
    fx_t pooled[48];
    for (int c = 0; c < 48; c++) pooled[c] = sum[c] / fx_t(65);
    cout << "Pooled[0..4]: ";
    for (int c = 0; c < 5; c++) cout << (float)pooled[c] << " ";
    cout << endl;

    // Dense0: 48->12 ReLU
    fx_t hidden[12];
    for (int oc = 0; oc < 12; oc++) {
        fx_t acc = fx_t(cnn_ba0[oc]);
        for (int ic = 0; ic < 48; ic++)
            acc += pooled[ic] * fx_t(cnn_wa0[oc][ic]);
        hidden[oc] = (acc > fx_t(0)) ? acc : fx_t(0);
    }
    cout << "Hidden[0..4]: ";
    for (int c = 0; c < 5; c++) cout << (float)hidden[c] << " ";
    cout << endl;

    // Dense2: 12->48 Sigmoid
    // TEST: use standard exp vs hls::exp
    fx_t gate_std[48], gate_hls[48];
    for (int oc = 0; oc < 48; oc++) {
        fx_t acc = fx_t(cnn_ba2[oc]);
        for (int ic = 0; ic < 12; ic++)
            acc += hidden[ic] * fx_t(cnn_wa2[oc][ic]);
        // Standard double exp
        gate_std[oc] = fx_t(1.0) / (fx_t(1.0) + fx_t(exp((double)(-acc))));
        // hls::exp
        gate_hls[oc] = fx_t(1.0) / (fx_t(1.0) + fx_t(hls::exp(-acc)));
    }
    cout << "Gate (std::exp)[0..4]: ";
    for (int c = 0; c < 5; c++) cout << (float)gate_std[c] << " ";
    cout << endl;
    cout << "Gate (hls::exp)[0..4]: ";
    for (int c = 0; c < 5; c++) cout << (float)gate_hls[c] << " ";
    cout << endl;

    // Check max gate difference
    double max_gate_diff = 0;
    for (int c = 0; c < 48; c++) {
        double diff = fabs((double)gate_std[c] - (double)gate_hls[c]);
        if (diff > max_gate_diff) max_gate_diff = diff;
    }
    cout << "Max |gate_std - gate_hls|: " << max_gate_diff << endl;

    // Complete with std::exp gate -> conv9 -> output, compute NMSE
    // SE multiply
    for (int c = 0; c < 48; c++)
        for (int l = 0; l < 65; l++)
            buf48b[c][l] = buf48a[c][l] * gate_std[c];

    // Conv9: 48->32, no ReLU
    fx_t buf32[32][65];
    for (int oc = 0; oc < 32; oc++) {
        for (int l = 0; l < 65; l++) {
            fx_t acc = fx_t(cnn_b9[oc]);
            for (int ic = 0; ic < 48; ic++) {
                fx_t v0 = (l-1 >= 0)   ? buf48b[ic][l-1] : fx_t(0);
                fx_t v1 = buf48b[ic][l];
                fx_t v2 = (l+1 < 65)   ? buf48b[ic][l+1] : fx_t(0);
                acc += v0 * fx_t(cnn_w9[oc][ic][0])
                     + v1 * fx_t(cnn_w9[oc][ic][1])
                     + v2 * fx_t(cnn_w9[oc][ic][2]);
            }
            buf32[oc][l] = acc;
        }
    }

    // Load residual base
    float raw_base[32][65];
    fx_t base_x[32][65];
    ifstream fbase("tb_cnn_residual_base.txt");
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fbase >> v;
            raw_base[c][l] = v;
            base_x[c][l] = fx_t(v);
        }
    fbase.close();

    // Output
    fx_t out_x[32][65];
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++)
            out_x[c][l] = base_x[c][l] + buf32[c][l];

    // NMSE with std::exp
    ifstream fexp("tb_cnn_expected.txt");
    double mse_std = 0, pwr = 0;
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fexp >> v;
            double diff = (double)out_x[c][l] - v;
            mse_std += diff * diff;
            pwr += v * v;
        }
    fexp.close();
    cout << "NMSE with std::exp gate: " << 10.0*log10(mse_std/pwr) << " dB" << endl;

    // Now redo SE multiply + conv9 + output with hls::exp gate
    for (int c = 0; c < 48; c++)
        for (int l = 0; l < 65; l++)
            buf48b[c][l] = buf48a[c][l] * gate_hls[c];
    for (int oc = 0; oc < 32; oc++) {
        for (int l = 0; l < 65; l++) {
            fx_t acc = fx_t(cnn_b9[oc]);
            for (int ic = 0; ic < 48; ic++) {
                fx_t v0 = (l-1 >= 0)   ? buf48b[ic][l-1] : fx_t(0);
                fx_t v1 = buf48b[ic][l];
                fx_t v2 = (l+1 < 65)   ? buf48b[ic][l+1] : fx_t(0);
                acc += v0 * fx_t(cnn_w9[oc][ic][0])
                     + v1 * fx_t(cnn_w9[oc][ic][1])
                     + v2 * fx_t(cnn_w9[oc][ic][2]);
            }
            buf32[oc][l] = acc;
        }
    }
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++)
            out_x[c][l] = base_x[c][l] + buf32[c][l];
    fexp.open("tb_cnn_expected.txt");
    double mse_hls = 0; pwr = 0;
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fexp >> v;
            double diff = (double)out_x[c][l] - v;
            mse_hls += diff * diff;
            pwr += v * v;
        }
    fexp.close();
    cout << "NMSE with hls::exp gate: " << 10.0*log10(mse_hls/pwr) << " dB" << endl;

    return 0;
}
