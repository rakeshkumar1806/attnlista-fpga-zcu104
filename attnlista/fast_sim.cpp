#include <iostream>
#include <fstream>
#include <cmath>
#include "attnlista_hls.h"
#include "cnn_hls_core.h"

// Forward declarations
void attnlista_top(
    const csignal_t y[N_BS][M_MEAS],
    csignal_t h_ls_init_out[N_BS][N_PATHS],
    csignal_t h_out[N_BS][N_PATHS]
);

void refinement_cnn_hls(
    hls::stream<cnn_vec2>& refine_input_1,
    hls::stream<cnn_vec2>& refine_input_2,
    hls::stream<cnn_vec2>& residual_base,
    hls::stream<cnn_axis_t>& out
);

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "Starting Tier 1: C++ Bit-Accurate Co-Simulation..." << std::endl;
    std::cout << "==================================================" << std::endl;

    static csignal_t y[N_BS][M_MEAS];
    static csignal_t h_ls_init_out[N_BS][N_PATHS];
    static csignal_t h_out[N_BS][N_PATHS];

    // 1. Load y input
    std::ifstream fin("tb_y_input.txt");
    if (!fin.is_open()) {
        std::cerr << "Error: Cannot open tb_y_input.txt!" << std::endl;
        return 1;
    }
    for (int i = 0; i < N_BS; i++) {
        for (int j = 0; j < M_MEAS; j++) {
            double re, im;
            fin >> re >> im;
            y[i][j] = csignal_t(signal_t(re), signal_t(im));
        }
    }
    fin.close();

    // 2. Run LISTA Core
    std::cout << "[Step 1/3] Executing 16-Layer LISTA HLS Core..." << std::endl;
    attnlista_top(y, h_ls_init_out, h_out);

    // Check LISTA NMSE vs tb_h_lista_expected.txt
    std::ifstream f_lexp("tb_h_lista_expected.txt");
    double l_mse = 0, l_pwr = 0;
    for (int i = 0; i < N_BS; i++) {
        for (int j = 0; j < N_PATHS; j++) {
            double re, im;
            f_lexp >> re >> im;
            double diff_re = (double)h_out[i][j].real() - re;
            double diff_im = (double)h_out[i][j].imag() - im;
            l_mse += diff_re * diff_re + diff_im * diff_im;
            l_pwr += re * re + im * im;
        }
    }
    f_lexp.close();
    double lista_nmse = 10.0 * log10(l_mse / l_pwr);
    std::cout << ">>> LISTA Stage NMSE: " << lista_nmse << " dB" << std::endl;

    // 3. Setup Streams for CNN Core
    std::cout << "[Step 2/3] Streaming LISTA outputs to Refinement CNN..." << std::endl;
    hls::stream<cnn_vec2> refine_input_1("refine1");
    hls::stream<cnn_vec2> refine_input_2("refine2");
    hls::stream<cnn_vec2> residual_base("resbase");
    hls::stream<cnn_axis_t> out_strm("outstrm");

    // Order: for each path l (0..64):
    // refine_input_1: 8 chunks of real (BS 0..15), then 8 chunks of imag (BS 0..15)
    // refine_input_2: 8 chunks of real (BS 0..15), then 8 chunks of imag (BS 0..15)
    for (int l = 0; l < N_PATHS; l++) {
        // Real channels
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_vec2 v1, v2;
            v1.data[0] = (cnn_t)h_out[chunk*2][l].real();
            v1.data[1] = (cnn_t)h_out[chunk*2+1][l].real();
            refine_input_1.write(v1);
            
            v2.data[0] = (cnn_t)h_ls_init_out[chunk*2][l].real();
            v2.data[1] = (cnn_t)h_ls_init_out[chunk*2+1][l].real();
            refine_input_2.write(v2);
        }
        // Imag channels
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_vec2 v1, v2;
            v1.data[0] = (cnn_t)h_out[chunk*2][l].imag();
            v1.data[1] = (cnn_t)h_out[chunk*2+1][l].imag();
            refine_input_1.write(v1);
            
            v2.data[0] = (cnn_t)h_ls_init_out[chunk*2][l].imag();
            v2.data[1] = (cnn_t)h_ls_init_out[chunk*2+1][l].imag();
            refine_input_2.write(v2);
        }
    }

    // residual_base: read sequentially by CNN after all refine inputs
    for (int l = 0; l < N_PATHS; l++) {
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_vec2 vres;
            vres.data[0] = (cnn_t)h_out[chunk*2][l].real();
            vres.data[1] = (cnn_t)h_out[chunk*2+1][l].real();
            residual_base.write(vres);
        }
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_vec2 vres;
            vres.data[0] = (cnn_t)h_out[chunk*2][l].imag();
            vres.data[1] = (cnn_t)h_out[chunk*2+1][l].imag();
            residual_base.write(vres);
        }
    }

    // 4. Run CNN Core
    std::cout << "[Step 3/3] Executing Refinement CNN HLS Core..." << std::endl;
    refinement_cnn_hls(refine_input_1, refine_input_2, residual_base, out_strm);

    // 5. Read CNN Output Stream
    static float hw_out[N_BS*2][N_PATHS];
    for (int l = 0; l < N_PATHS; l++) {
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_axis_t val = out_strm.read();
            ap_uint<64> data = val.data;
            cnn_t d0, d1;
            d0.range() = data.range(31, 0);
            d1.range() = data.range(63, 32);
            hw_out[chunk*2][l] = (float)d0;
            hw_out[chunk*2+1][l] = (float)d1;
        }
        for (int chunk = 0; chunk < 8; chunk++) {
            cnn_axis_t val = out_strm.read();
            ap_uint<64> data = val.data;
            cnn_t d0, d1;
            d0.range() = data.range(31, 0);
            d1.range() = data.range(63, 32);
            hw_out[16 + chunk*2][l] = (float)d0;
            hw_out[16 + chunk*2+1][l] = (float)d1;
        }
    }

    // 6. Compute Final System NMSE vs regenerated tb_cnn_expected.txt (in [time][channel] order)
    std::ifstream fexp("tb_cnn_expected.txt");
    if (!fexp.is_open()) {
        std::cerr << "Error: Cannot open tb_cnn_expected.txt!" << std::endl;
        return 1;
    }
    double mse = 0, pwr = 0;
    for (int l = 0; l < N_PATHS; l++) {
        for (int c = 0; c < N_BS*2; c++) {
            float exp;
            fexp >> exp;
            double diff = hw_out[c][l] - exp;
            mse += diff * diff;
            pwr += exp * exp;
        }
    }
    fexp.close();
    
    double final_nmse = 10.0 * log10(mse / pwr);
    std::cout << "==================================================" << std::endl;
    std::cout << ">>> FINAL END-TO-END SYSTEM NMSE: " << final_nmse << " dB" << std::endl;
    std::cout << "==================================================" << std::endl;
    if (final_nmse < -25.0) {
        std::cout << ">>> RESULT: PASS (Hardware matches PyTorch Golden Model!)" << std::endl;
        return 0;
    } else {
        std::cout << ">>> RESULT: FAIL (NMSE >= -25 dB)" << std::endl;
        return 1;
    }
}
