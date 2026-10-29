#include <iostream>
#include <complex>
#include <cmath>
#include "attnlista_hls.h"
#include "attnlista_hls_core.h"

int main() {
    csignal_t x(0.5, 0.5);
    signal_t th(0.3);
    signal_t scale(1.0);
    csignal_t y = soft_threshold<signal_t>(x, th, scale);
    std::cout << "y = " << y.real() << " + " << y.imag() << "j\n";
    return 0;
}
