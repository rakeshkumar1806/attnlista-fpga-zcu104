#include <hls_stream.h>
#include <ap_int.h>

struct csignal_t {
    float real;
    float imag;
};

struct cnn_vec64 {
    float data[64];
};

void stream_adapter(
    hls::stream<csignal_t>& lista_in,
    hls::stream<cnn_vec64>& cnn_out
) {
#pragma HLS INTERFACE axis port=lista_in
#pragma HLS INTERFACE axis port=cnn_out
#pragma HLS INTERFACE s_axilite port=return

    // Read 80 elements from LISTA (only the first 64 are channels? Or do we need to reshape?)
    // This is fundamentally an architectural question: how does 80 paths map to 64 channels?
    // I will halt and ask the user.
}
