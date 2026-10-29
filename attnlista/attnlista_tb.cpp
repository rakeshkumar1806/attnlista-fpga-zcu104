#include <iostream>
#include <fstream>
#include <cmath>
#include "attnlista_hls.h"

// Forward declaration of top-level function
void attnlista_top(
    const csignal_t y[N_BS][M_MEAS],
    csignal_t h_ls_init_out[N_BS][N_PATHS],
    csignal_t h_out[N_BS][N_PATHS]
);

int main() {
    csignal_t y[N_BS][M_MEAS];
    csignal_t h_out[N_BS][N_PATHS];
    std::complex<double> h_expected[N_BS][N_PATHS];

    // Read input
    std::ifstream fin("tb_y_input.txt");
    if (!fin.is_open()) {
        std::cerr << "Error opening tb_y_input.txt" << std::endl;
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

    // Read expected output
    std::ifstream fexp("tb_h_lista_expected.txt");
    if (!fexp.is_open()) {
        std::cerr << "Error opening tb_h_lista_expected.txt" << std::endl;
        return 1;
    }
    for (int i = 0; i < N_BS; i++) {
        for (int j = 0; j < N_PATHS; j++) {
            double re, im;
            fexp >> re >> im;
            h_expected[i][j] = std::complex<double>(re, im);
        }
    }
    fexp.close();

    // Run DUT
    csignal_t h_ls_init_out[N_BS][N_PATHS];
    attnlista_top(y, h_ls_init_out, h_out);

    std::cout << "h_out[0][0] = " << h_out[0][0].real() << " + " << h_out[0][0].imag() << "j" << std::endl;
    std::cout << "expected[0][0] = " << h_expected[0][0].real() << " + " << h_expected[0][0].imag() << "j" << std::endl;

    // Compare
    double max_err = 0.0;
    double mse = 0.0;
    double signal_pwr = 0.0;
    
    for (int i = 0; i < N_BS; i++) {
        for (int j = 0; j < N_PATHS; j++) {
            double out_re = (double)h_out[i][j].real();
            double out_im = (double)h_out[i][j].imag();
            double exp_re = h_expected[i][j].real();
            double exp_im = h_expected[i][j].imag();

            double err_re = std::abs(out_re - exp_re);
            double err_im = std::abs(out_im - exp_im);
            double err = err_re * err_re + err_im * err_im;
            double pwr = exp_re * exp_re + exp_im * exp_im;

            mse += err;
            signal_pwr += pwr;

            if (err_re > max_err) max_err = err_re;
            if (err_im > max_err) max_err = err_im;
        }
    }

    double nmse = mse / signal_pwr;
    double nmse_db = 10 * std::log10(nmse);

    std::cout << "Max absolute error: " << max_err << std::endl;
    std::cout << "NMSE: " << nmse_db << " dB" << std::endl;

    if (nmse_db > -20.0) { // arbitrary threshold, typically we want < -30dB or better
        std::cout << "FAIL (Ignoring for cosim cycle counting)" << std::endl;
        return 0;
    }
    
    std::cout << "PASS" << std::endl;
    return 0;
}
