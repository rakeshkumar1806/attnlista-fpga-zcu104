#pragma once

#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// Use the same type as the LISTA core for consistency
typedef ap_fixed<32, 16> cnn_t;
typedef ap_fixed<32, 16> cnn_acc_t;

struct cnn_vec64 { cnn_t data[64]; };
struct cnn_vec48 { cnn_t data[48]; };
struct cnn_vec32 { cnn_t data[32]; };
struct cnn_vec2  { cnn_t data[2]; };

typedef ap_axiu<64, 0, 0, 0> cnn_axis_t;

void refinement_cnn_hls(
    hls::stream<cnn_vec2>& refine_input_1,
    hls::stream<cnn_vec2>& refine_input_2,
    hls::stream<cnn_vec2>& residual_base,
    hls::stream<cnn_axis_t>& out
);
