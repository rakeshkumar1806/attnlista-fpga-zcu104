// Proper diagnostic: run BOTH float and ap_fixed<32,16> in lockstep,
// compare per-layer accumulator values to determine overflow vs rounding.
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <ap_fixed.h>
#include "cnn_weights.h"

using namespace std;

typedef ap_fixed<32, 16, AP_TRN, AP_WRAP> fx_t;  // The ORIGINAL failing type

// Per-layer diagnostics
struct LayerDiag {
    const char* name;
    double max_float_acc;
    double min_float_acc;
    double max_abs_float_acc;
    int overflow_count;     // times float acc exceeds fx_t range
    double max_abs_error;   // max |float_result - fixed_result| per element
    double sum_sq_error;
    double sum_sq_signal;
    int num_elements;
};

static LayerDiag diags[10];
static int ndiags = 0;

LayerDiag& new_diag(const char* name) {
    LayerDiag& d = diags[ndiags++];
    d.name = name;
    d.max_float_acc = -1e30;
    d.min_float_acc = 1e30;
    d.max_abs_float_acc = 0;
    d.overflow_count = 0;
    d.max_abs_error = 0;
    d.sum_sq_error = 0;
    d.sum_sq_signal = 0;
    d.num_elements = 0;
    return d;
}

void track_acc(LayerDiag& d, double float_acc) {
    if (float_acc > d.max_float_acc) d.max_float_acc = float_acc;
    if (float_acc < d.min_float_acc) d.min_float_acc = float_acc;
    double a = fabs(float_acc);
    if (a > d.max_abs_float_acc) d.max_abs_float_acc = a;
    // ap_fixed<32,16> with AP_WRAP: range is [-32768, 32767.999...]
    if (float_acc > 32767.0 || float_acc < -32768.0) d.overflow_count++;
}

void track_output(LayerDiag& d, double float_val, double fixed_val) {
    double err = fabs(float_val - fixed_val);
    if (err > d.max_abs_error) d.max_abs_error = err;
    d.sum_sq_error += (float_val - fixed_val) * (float_val - fixed_val);
    d.sum_sq_signal += float_val * float_val;
    d.num_elements++;
}

template<int IN_CH, int OUT_CH, int LEN>
void conv1d_dual(const char* layer_name,
                 const float in_f[IN_CH][LEN], const fx_t in_x[IN_CH][LEN],
                 const float weight[OUT_CH][IN_CH][3], const float bias[OUT_CH],
                 float out_f[OUT_CH][LEN], fx_t out_x[OUT_CH][LEN],
                 bool apply_relu) {
    LayerDiag& d = new_diag(layer_name);
    for (int oc = 0; oc < OUT_CH; oc++) {
        for (int l = 0; l < LEN; l++) {
            // Float path
            double f_acc = bias[oc];
            // Fixed path
            fx_t x_acc = fx_t(bias[oc]);

            for (int ic = 0; ic < IN_CH; ic++) {
                for (int k = 0; k < 3; k++) {
                    int pos = l + k - 1;
                    if (pos >= 0 && pos < LEN) {
                        f_acc += (double)in_f[ic][pos] * (double)weight[oc][ic][k];
                        x_acc += in_x[ic][pos] * fx_t(weight[oc][ic][k]);
                    }
                }
            }
            track_acc(d, f_acc);

            float f_res = apply_relu ? max(0.0, f_acc) : f_acc;
            fx_t  x_res = apply_relu ? (x_acc > fx_t(0) ? x_acc : fx_t(0)) : x_acc;

            out_f[oc][l] = f_res;
            out_x[oc][l] = x_res;
            track_output(d, f_res, (double)x_res);
        }
    }
}

int main() {
    // Load input
    float in_f[64][65];
    fx_t  in_x[64][65];
    ifstream fin("tb_cnn_input.txt");
    for (int c = 0; c < 64; c++)
        for (int l = 0; l < 65; l++) {
            float v; fin >> v;
            in_f[c][l] = v;
            in_x[c][l] = fx_t(v);
        }
    fin.close();

    float base_f[32][65];
    fx_t  base_x[32][65];
    ifstream fbase("tb_cnn_residual_base.txt");
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fbase >> v;
            base_f[c][l] = v;
            base_x[c][l] = fx_t(v);
        }
    fbase.close();

    // Conv Layer 0: 64->48
    float f48a_f[48][65]; fx_t f48a_x[48][65];
    conv1d_dual<64, 48, 65>("Conv0 (64->48, ReLU)", in_f, in_x,
                             cnn_w0, cnn_b0, f48a_f, f48a_x, true);

    // Conv Layer 3: 48->48
    float f48b_f[48][65]; fx_t f48b_x[48][65];
    conv1d_dual<48, 48, 65>("Conv3 (48->48, ReLU)", f48a_f, f48a_x,
                             cnn_w3, cnn_b3, f48b_f, f48b_x, true);

    // Conv Layer 6: 48->48
    float f48c_f[48][65]; fx_t f48c_x[48][65];
    conv1d_dual<48, 48, 65>("Conv6 (48->48, ReLU)", f48b_f, f48b_x,
                             cnn_w6, cnn_b6, f48c_f, f48c_x, true);

    // SE block: global avg pool
    LayerDiag& dse = new_diag("SE-GlobalAvgPool+Dense");
    float pooled_f[48]; fx_t pooled_x[48];
    for (int c = 0; c < 48; c++) {
        double fsum = 0; fx_t xsum = 0;
        for (int l = 0; l < 65; l++) {
            fsum += f48c_f[c][l];
            xsum += f48c_x[c][l];
        }
        track_acc(dse, fsum);
        pooled_f[c] = fsum / 65.0;
        pooled_x[c] = xsum / fx_t(65);
    }

    // Dense0: 48->12 ReLU
    float hid_f[12]; fx_t hid_x[12];
    for (int oc = 0; oc < 12; oc++) {
        double facc = cnn_ba0[oc]; fx_t xacc = fx_t(cnn_ba0[oc]);
        for (int ic = 0; ic < 48; ic++) {
            facc += pooled_f[ic] * cnn_wa0[oc][ic];
            xacc += pooled_x[ic] * fx_t(cnn_wa0[oc][ic]);
        }
        track_acc(dse, facc);
        hid_f[oc] = max(0.0, facc);
        hid_x[oc] = xacc > fx_t(0) ? xacc : fx_t(0);
    }

    // Dense2: 12->48 Sigmoid
    float gate_f[48]; fx_t gate_x[48];
    for (int oc = 0; oc < 48; oc++) {
        double facc = cnn_ba2[oc]; fx_t xacc = fx_t(cnn_ba2[oc]);
        for (int ic = 0; ic < 12; ic++) {
            facc += hid_f[ic] * cnn_wa2[oc][ic];
            xacc += hid_x[ic] * fx_t(cnn_wa2[oc][ic]);
        }
        track_acc(dse, facc);
        gate_f[oc] = 1.0 / (1.0 + exp(-facc));
        // For fixed: approximate sigmoid
        gate_x[oc] = fx_t(1.0) / (fx_t(1.0) + fx_t(exp((double)(-xacc))));
        track_output(dse, gate_f[oc], (double)gate_x[oc]);
    }
    dse.num_elements = 48;

    // SE multiply
    float f48d_f[48][65]; fx_t f48d_x[48][65];
    for (int c = 0; c < 48; c++)
        for (int l = 0; l < 65; l++) {
            f48d_f[c][l] = f48c_f[c][l] * gate_f[c];
            f48d_x[c][l] = f48c_x[c][l] * gate_x[c];
        }

    // Conv Layer 9: 48->32, no ReLU
    float f32_f[32][65]; fx_t f32_x[32][65];
    conv1d_dual<48, 32, 65>("Conv9 (48->32, linear)", f48d_f, f48d_x,
                             cnn_w9, cnn_b9, f32_f, f32_x, false);

    // Residual add
    LayerDiag& dout = new_diag("Output (residual add)");
    float out_f[32][65]; fx_t out_x[32][65];
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            out_f[c][l] = base_f[c][l] + f32_f[c][l];
            out_x[c][l] = base_x[c][l] + f32_x[c][l];
            track_output(dout, out_f[c][l], (double)out_x[c][l]);
        }

    // Load expected and compute final NMSE
    ifstream fexp("tb_cnn_expected.txt");
    double mse = 0, pwr = 0;
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fexp >> v;
            double diff = (double)out_x[c][l] - v;
            mse += diff * diff;
            pwr += v * v;
        }
    fexp.close();
    double nmse = 10.0 * log10(mse / pwr);

    // Print report
    cout << "========================================" << endl;
    cout << "  ap_fixed<32,16,AP_TRN,AP_WRAP> DIAGNOSIS" << endl;
    cout << "  Range: [-32768, 32767.999...]" << endl;
    cout << "========================================" << endl;
    for (int i = 0; i < ndiags; i++) {
        LayerDiag& d = diags[i];
        cout << endl << "--- " << d.name << " ---" << endl;
        cout << "  Float acc range: [" << d.min_float_acc << ", " << d.max_float_acc << "]" << endl;
        cout << "  Max |float acc|: " << d.max_abs_float_acc << endl;
        cout << "  OVERFLOW COUNT (|acc| > 32767): " << d.overflow_count << endl;
        if (d.num_elements > 0) {
            cout << "  Max |error| per element: " << d.max_abs_error << endl;
            double layer_nmse = 10.0 * log10(d.sum_sq_error / max(d.sum_sq_signal, 1e-30));
            cout << "  Layer NMSE: " << layer_nmse << " dB" << endl;
        }
    }
    cout << endl << "========================================" << endl;
    cout << "FINAL OUTPUT NMSE (ap_fixed<32,16> vs golden): " << nmse << " dB" << endl;
    cout << "========================================" << endl;

    return 0;
}
