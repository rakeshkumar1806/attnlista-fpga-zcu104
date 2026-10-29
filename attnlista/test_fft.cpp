#include <iostream>
#include <complex>
#include <cmath>
#include "attnlista_hls.h"
#include "attnlista_hls_core.h"

int main() {
    signal_t data_real[16] = {0.1, 0.2, -0.3, 0.4, 0.5, 0.6, -0.7, 0.8, 0.9, 1.0, -1.1, 1.2, 1.3, 1.4, -1.5, 1.6};
    csignal_t data[16];
    for (int i=0; i<16; i++) data[i] = csignal_t(data_real[i], signal_t(0));

    fft_radix2<signal_t, 16>(data, false);
    fft_radix2<signal_t, 16>(data, true);
    
    for (int i=0; i<16; i++) {
        std::cout << data[i].real() << " + " << data[i].imag() << "j\n";
    }
    return 0;
}
