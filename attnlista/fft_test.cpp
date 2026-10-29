#include <iostream>
#include <fstream>
#include <cmath>
#include "attnlista_hls_core.h"

int main() {
    csignal_t data[1][16];
    std::ifstream fin("tb_fft_16_in.txt");
    for(int i=0; i<16; i++) {
        float re, im;
        fin >> re >> im;
        data[0][i] = csignal_t(re, im);
    }
    fin.close();

    radix2_fft_rows<1, 16, 4>(data, false);

    std::ifstream fout("tb_fft_16_out.txt");
    double mse = 0;
    double sig_pwr = 0;
    for(int i=0; i<16; i++) {
        float re, im;
        fout >> re >> im;
        double err_re = (double)data[0][i].real() - re;
        double err_im = (double)data[0][i].imag() - im;
        mse += err_re*err_re + err_im*err_im;
        sig_pwr += re*re + im*im;
    }
    fout.close();

    double nmse = 10 * log10(mse / sig_pwr);
    std::cout << "16-point FFT Isolated NMSE: " << nmse << " dB" << std::endl;
    return 0;
}
