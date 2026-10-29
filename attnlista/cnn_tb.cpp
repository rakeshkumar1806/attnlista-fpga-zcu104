#include <iostream>
#include <fstream>
#include <cmath>
#include "cnn_hls_core.h"

int main() {
    hls::stream<cnn_vec2> refine_input_strm;
    hls::stream<cnn_vec2> residual_base_strm;
    hls::stream<cnn_vec2> out_strm;

    // Read input in [channel][time] order, feed as [time]-major stream
    float raw_in[64][65];
    std::ifstream fin("tb_cnn_input.txt");
    for (int c = 0; c < 64; c++)
        for (int l = 0; l < 65; l++)
            fin >> raw_in[c][l];
    fin.close();

    for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 32; chunk++) {
            cnn_vec2 v;
            v.data[0] = raw_in[chunk*2][l];
    float tb_res[32][65];
    std::ifstream fbase("tb_cnn_residual_base.txt");
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++)
            fbase >> tb_res[c][l];
    fbase.close();

    hls::stream<cnn_vec2> refine_input_1;
    hls::stream<cnn_vec2> refine_input_2;
    hls::stream<cnn_vec2> residual_base;
    hls::stream<cnn_vec2> out_r;

    // Load inputs
    for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
            cnn_vec2 v;
            v.data[0] = tb_input[chunk*2][l];
            v.data[1] = tb_input[chunk*2+1][l];
            refine_input_1.write(v);
        }
        for (int chunk = 0; chunk < 16; chunk++) {
            cnn_vec2 v;
            v.data[0] = tb_input[32 + chunk*2][l];
            v.data[1] = tb_input[32 + chunk*2+1][l];
            refine_input_2.write(v);
        }
    }
    for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
            cnn_vec2 v;
            v.data[0] = tb_res[chunk*2][l];
            v.data[1] = tb_res[chunk*2+1][l];
            residual_base.write(v);
        }
    }

    // Run DUT
    refinement_cnn_hls(refine_input_1, refine_input_2, residual_base, out_r);

    // Read output
    float raw_out[32][65];
    for (int l = 0; l < 65; l++) {
        for (int chunk = 0; chunk < 16; chunk++) {
            cnn_vec2 v = out_r.read();
            raw_out[chunk*2][l] = (float)v.data[0];
            raw_out[chunk*2+1][l] = (float)v.data[1];
        }
    }

    // Compare to golden
    std::ifstream fexp("tb_cnn_expected.txt");
    double mse = 0, pwr = 0;
    for (int c = 0; c < 32; c++)
        for (int l = 0; l < 65; l++) {
            float v; fexp >> v;
            double diff = raw_out[c][l] - v;
            mse += diff * diff;
            pwr += v * v;
        }
    fexp.close();

    double nmse = 10.0 * log10(mse / pwr);
    std::cout << "CNN NMSE: " << nmse << " dB" << std::endl;
    if (nmse < -25.0) {
        std::cout << "PASS" << std::endl;
        return 0;
    }
    std::cout << "FAIL" << std::endl;
    return 1;
}
