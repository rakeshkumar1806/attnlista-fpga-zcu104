#include "cnn_hls_core.h"
#include "cnn_weights.h"

// =========================================================================
// RefinementCNN: 4 Conv1d layers + SE block
// Architecture: Sequential channel processing, LUT-based sigmoid
// =========================================================================

// Piecewise-linear sigmoid approximation using a lookup table.
// Uses 256 entries covering x in [-8, +8]. For |x| > 8, sigmoid ≈ 0 or 1.
// This replaces exp() and eliminates the double-precision FP pipeline.
static const unsigned short sigmoid_lut[256] = {
    22, 23, 25, 27, 28, 30, 32, 34,
    36, 39, 41, 44, 47, 50, 53, 56,
    60, 64, 68, 72, 77, 82, 87, 92,
    98, 105, 111, 119, 126, 134, 143, 152,
    162, 172, 184, 195, 208, 221, 236, 251,
    267, 284, 302, 321, 342, 364, 387, 412,
    439, 467, 497, 528, 562, 598, 636, 677,
    720, 766, 815, 867, 922, 980, 1042, 1109,
    1179, 1253, 1332, 1417, 1506, 1601, 1701, 1808,
    1921, 2041, 2168, 2303, 2446, 2598, 2758, 2928,
    3108, 3298, 3500, 3713, 3938, 4176, 4427, 4692,
    4971, 5266, 5577, 5904, 6249, 6611, 6992, 7392,
    7812, 8252, 8714, 9197, 9702, 10230, 10782, 11356,
    11955, 12578, 13226, 13898, 14595, 15316, 16062, 16831,
    17625, 18442, 19281, 20143, 21025, 21927, 22848, 23787,
    24742, 25712, 26695, 27689, 28693, 29705, 30722, 31744,
    32768, 33791, 34813, 35830, 36842, 37846, 38840, 39823,
    40793, 41748, 42687, 43608, 44510, 45392, 46254, 47093,
    47910, 48704, 49473, 50219, 50940, 51637, 52309, 52957,
    53580, 54179, 54753, 55305, 55833, 56338, 56821, 57283,
    57723, 58143, 58543, 58924, 59286, 59631, 59958, 60269,
    60564, 60843, 61108, 61359, 61597, 61822, 62035, 62237,
    62427, 62607, 62777, 62937, 63089, 63232, 63367, 63494,
    63614, 63727, 63834, 63934, 64029, 64118, 64203, 64282,
    64356, 64426, 64493, 64555, 64613, 64668, 64720, 64769,
    64815, 64858, 64899, 64937, 64973, 65007, 65038, 65068,
    65096, 65123, 65148, 65171, 65193, 65214, 65233, 65251,
    65268, 65284, 65299, 65314, 65327, 65340, 65351, 65363,
    65373, 65383, 65392, 65401, 65409, 65416, 65424, 65430,
    65437, 65443, 65448, 65453, 65458, 65463, 65467, 65471,
    65475, 65479, 65482, 65485, 65488, 65491, 65494, 65496,
    65499, 65501, 65503, 65505, 65507, 65508, 65510, 65512
};

// Synthesizable sigmoid using LUT + linear interpolation
// Input: ap_fixed<32,16>, Output: ap_fixed<32,16> in [0, 1)
static cnn_t sigmoid_approx(cnn_t x) {
#pragma HLS INLINE
    // Clamp to [-8, 8]
    const cnn_t NEG8 = cnn_t(-8.0);
    const cnn_t POS8 = cnn_t(7.9375);  // just under 8
    if (x <= NEG8) return cnn_t(0);
    if (x >= POS8) return cnn_t(1);

    // Map x from [-8, 8] to [0, 256)
    // index = (x + 8) * 16 = (x + 8) * 256/16
    cnn_t shifted = x - NEG8;  // [0, 16)
    cnn_t scaled = shifted * cnn_t(16);  // [0, 256)

    // Extract integer index
    int idx = (int)scaled;
    if (idx < 0) idx = 0;
    if (idx > 254) idx = 254;

    // LUT lookup
    unsigned short v0 = sigmoid_lut[idx];
    unsigned short v1 = sigmoid_lut[idx + 1];

    // Linear interpolation fraction
    cnn_t frac = scaled - cnn_t(idx);

    // Result = v0/65535 + frac * (v1 - v0) / 65535
    cnn_t result = cnn_t((double)v0 / 65535.0) +
                   frac * cnn_t((double)(v1 - v0) / 65535.0);

    return result;
}

template<int IN_CH, int OUT_CH, int LEN>
void conv1d_layer_stream(
    cnn_t in_buf[IN_CH][LEN],
    cnn_t out_buf[OUT_CH][LEN],
    const float weight[OUT_CH][IN_CH][3],
    const float bias[OUT_CH],
    bool apply_relu)
{
    CONV_OC: for (int oc = 0; oc < OUT_CH; oc++) {
        CONV_L: for (int l = 0; l < LEN; l++) {
            cnn_acc_t acc = cnn_t(bias[oc]);
            CONV_IC: for (int ic = 0; ic < IN_CH; ic++) {
#pragma HLS PIPELINE II=1
                cnn_t v0 = (l-1 >= 0)   ? in_buf[ic][l-1] : cnn_t(0);
                cnn_t v1 = in_buf[ic][l];
                cnn_t v2 = (l+1 < LEN)  ? in_buf[ic][l+1] : cnn_t(0);
                acc += v0 * cnn_t(weight[oc][ic][0])
                     + v1 * cnn_t(weight[oc][ic][1])
                     + v2 * cnn_t(weight[oc][ic][2]);
            }
            cnn_t res = acc;
            out_buf[oc][l] = apply_relu ? (res > cnn_t(0) ? res : cnn_t(0)) : res;
        }
    }
}

void refinement_cnn_hls(
    hls::stream<cnn_vec2>& refine_input_1,
    hls::stream<cnn_vec2>& refine_input_2,
    hls::stream<cnn_vec2>& residual_base,
    hls::stream<cnn_axis_t>& out
) {
#pragma HLS INTERFACE axis port=refine_input_1
#pragma HLS INTERFACE axis port=refine_input_2
#pragma HLS INTERFACE axis port=residual_base
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=return

    // ---- Read input from stream into local RAM ----
    // We expect 65 timesteps, each with 64 channels.
    cnn_t in64[64][65];
#pragma HLS ARRAY_PARTITION variable=in64 dim=1 complete

    READ_IN: for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
#pragma HLS PIPELINE II=1
            cnn_vec2 v1 = refine_input_1.read();
            in64[chunk*2][l] = v1.data[0];
            in64[chunk*2+1][l] = v1.data[1];
        }
        for (int chunk = 0; chunk < 16; chunk++) {
#pragma HLS PIPELINE II=1
            cnn_vec2 v2 = refine_input_2.read();
            in64[32 + chunk*2][l] = v2.data[0];
            in64[32 + chunk*2+1][l] = v2.data[1];
        }
    }

    // ---- Read residual base from stream ----
    cnn_t res_base[32][65];
#pragma HLS ARRAY_PARTITION variable=res_base dim=1 complete

    READ_RES: for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
#pragma HLS PIPELINE II=1
            cnn_vec2 v = residual_base.read();
            res_base[chunk*2][l] = v.data[0];
            res_base[chunk*2+1][l] = v.data[1];
        }
    }

    // ---- Conv Layer 0: 64 -> 48, ReLU ----
    cnn_t buf48a[48][65];
    conv1d_layer_stream<64, 48, 65>(in64, buf48a, cnn_w0, cnn_b0, true);

    // ---- Conv Layer 3: 48 -> 48, ReLU ----
    cnn_t buf48b[48][65];
    conv1d_layer_stream<48, 48, 65>(buf48a, buf48b, cnn_w3, cnn_b3, true);

    // ---- Conv Layer 6: 48 -> 48, ReLU ----
    conv1d_layer_stream<48, 48, 65>(buf48b, buf48a, cnn_w6, cnn_b6, true);

    // ---- SE Block: Global Avg Pool ----
    cnn_acc_t sum[48];
#pragma HLS ARRAY_PARTITION variable=sum complete dim=1
    SE_INIT: for (int c = 0; c < 48; c++) {
#pragma HLS UNROLL
        sum[c] = 0;
    }
    SE_SUM: for (int l = 0; l < 65; l++) {
#pragma HLS PIPELINE II=1
        for (int c = 0; c < 48; c++) {
#pragma HLS UNROLL
            sum[c] += buf48a[c][l];
        }
    }

    cnn_t pooled[48];
    SE_DIV: for (int c = 0; c < 48; c++) {
#pragma HLS UNROLL
        pooled[c] = sum[c] / cnn_t(65);
    }

    // ---- SE Dense0: 48 -> 12, ReLU (serialized) ----
    cnn_t hidden[12];
    SE_D0: for (int oc = 0; oc < 12; oc++) {
        cnn_acc_t acc = cnn_t(cnn_ba0[oc]);
        SE_D0_IC: for (int ic = 0; ic < 48; ic++) {
#pragma HLS PIPELINE II=1
            acc += pooled[ic] * cnn_t(cnn_wa0[oc][ic]);
        }
        cnn_t r = acc;
        hidden[oc] = (r > cnn_t(0)) ? r : cnn_t(0);
    }

    // ---- SE Dense2: 12 -> 48, Sigmoid (serialized, LUT sigmoid) ----
    cnn_t gate[48];
    SE_D2: for (int oc = 0; oc < 48; oc++) {
        cnn_acc_t acc = cnn_t(cnn_ba2[oc]);
        SE_D2_IC: for (int ic = 0; ic < 12; ic++) {
#pragma HLS PIPELINE II=1
            acc += hidden[ic] * cnn_t(cnn_wa2[oc][ic]);
        }
        cnn_t val = acc;
        gate[oc] = sigmoid_approx(val);
    }

    // ---- SE Multiply (serialized: 1 channel per cycle) ----
    SE_MUL: for (int c = 0; c < 48; c++) {
        SE_MUL_L: for (int l = 0; l < 65; l++) {
#pragma HLS PIPELINE II=1
            buf48b[c][l] = buf48a[c][l] * gate[c];
        }
    }

    // ---- Conv Layer 9: 48 -> 32, no ReLU ----
    cnn_t buf32[32][65];
    conv1d_layer_stream<48, 32, 65>(buf48b, buf32, cnn_w9, cnn_b9, false);

    // ---- Output: residual add + write to stream ----
    WRITE_OUT: for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
#pragma HLS PIPELINE II=1
            cnn_axis_t o;
            
            // Reinterpret casting memory directly avoids ap_fixed conversion issues
            cnn_t tmp_0 = res_base[chunk*2][l] + buf32[chunk*2][l];
            cnn_t tmp_1 = res_base[chunk*2+1][l] + buf32[chunk*2+1][l];
            
            // Pack into 64-bit uint for AXI stream
            ap_uint<32> raw_0 = tmp_0.range(31,0);
            ap_uint<32> raw_1 = tmp_1.range(31,0);
            o.data = (ap_uint<64>(raw_1) << 32) | raw_0;
            
            // AXI required side-band signals
            o.keep = 0xFF; // All bytes valid
            o.strb = 0xFF;
            
            // TLAST generation
            if (l == 64 && chunk == 15) {
                o.last = 1;
            } else {
                o.last = 0;
            }
            out.write(o);
        }
    }
}
